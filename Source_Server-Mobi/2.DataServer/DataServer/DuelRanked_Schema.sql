-- =============================================================================
-- Ranked Duel (cross-server 1v1 matchmaking) - see Working Project\MD Files\RankedDuel-PLAN.md
--
-- The database holds only the rating (MMR) and the match history. Rank and
-- division are worked out from DuelRanked.ini on every read, so retuning the
-- rank table never needs a migration. Elo: K = @K, start = @Start, nothing goes
-- below @Floor. Each result is rounded once and the loser gets exactly the
-- opposite, so a match is zero-sum (the floor can shrink a loss; the real
-- deltas are returned).
--
-- DuelRankedMatch.Status: 0 created, 1 settled, 2 draw, 3 void, 4 abandoned.
-- WZ_DuelRanked_Result is idempotent: a result sent twice (the host resends
-- until it is acknowledged) is applied once; the second call returns the stored
-- result with Applied = 0.
--
-- The legacy RankingDuel table is not touched.
--
-- Safe to run again: tables are created only if missing, procedures replaced.
-- =============================================================================

IF OBJECT_ID('DuelRanked', 'U') IS NULL
BEGIN
    CREATE TABLE DuelRanked (
        Name        VARCHAR(10) NOT NULL PRIMARY KEY,
        Account     VARCHAR(10) NOT NULL,
        MMR         INT         NOT NULL,
        Wins        INT         NOT NULL DEFAULT 0,
        Losses      INT         NOT NULL DEFAULT 0,
        Draws       INT         NOT NULL DEFAULT 0,
        PeakMMR     INT         NOT NULL,
        LastMatchAt DATETIME    NULL
    );
    CREATE INDEX IX_DuelRanked_MMR ON DuelRanked (MMR DESC);
END
GO

IF OBJECT_ID('DuelRankedMatch', 'U') IS NULL
BEGIN
    CREATE TABLE DuelRankedMatch (
        MatchId     INT IDENTITY(1,1) NOT NULL PRIMARY KEY,
        Status      TINYINT     NOT NULL DEFAULT 0,
        NameA       VARCHAR(10) NOT NULL,
        NameB       VARCHAR(10) NOT NULL,
        WinnerName  VARCHAR(10) NULL,
        MmrABefore  INT         NOT NULL,
        MmrAAfter   INT         NULL,
        MmrBBefore  INT         NOT NULL,
        MmrBAfter   INT         NULL,
        Reason      TINYINT     NULL,
        ScoreA      TINYINT     NULL,
        ScoreB      TINYINT     NULL,
        HostServer  INT         NOT NULL DEFAULT 0,
        CreatedAt   DATETIME    NOT NULL DEFAULT GETDATE(),
        SettledAt   DATETIME    NULL
    );
    CREATE INDEX IX_DuelRankedMatch_Status ON DuelRankedMatch (Status, CreatedAt);
    CREATE INDEX IX_DuelRankedMatch_NameA ON DuelRankedMatch (NameA);
    CREATE INDEX IX_DuelRankedMatch_NameB ON DuelRankedMatch (NameB);
END
GO

-- -----------------------------------------------------------------------------
-- One character's rating. A character that never fought is created here at
-- @Start. Returns MMR, Wins, Losses, Draws.
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_DuelRanked_Get', 'P') IS NOT NULL DROP PROCEDURE WZ_DuelRanked_Get;
GO
CREATE PROCEDURE WZ_DuelRanked_Get
    @Name       VARCHAR(10),
    @Account    VARCHAR(10),
    @Start      INT = 1000
AS
BEGIN
    SET NOCOUNT ON;
    SET XACT_ABORT ON;

    BEGIN TRAN;

    IF NOT EXISTS (SELECT 1 FROM DuelRanked WITH (UPDLOCK, HOLDLOCK) WHERE Name = @Name)
        INSERT INTO DuelRanked (Name, Account, MMR, PeakMMR) VALUES (@Name, @Account, @Start, @Start);
    ELSE
        UPDATE DuelRanked SET Account = @Account WHERE Name = @Name AND Account <> @Account;

    COMMIT TRAN;

    SELECT MMR, Wins, Losses, Draws FROM DuelRanked WHERE Name = @Name;
END
GO

-- -----------------------------------------------------------------------------
-- A match was agreed: make sure both fighters have a rating row and open the
-- match (Status 0). Returns MatchId and both ratings as they are now.
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_DuelRanked_Create', 'P') IS NOT NULL DROP PROCEDURE WZ_DuelRanked_Create;
GO
CREATE PROCEDURE WZ_DuelRanked_Create
    @NameA      VARCHAR(10),
    @AccA       VARCHAR(10),
    @NameB      VARCHAR(10),
    @AccB       VARCHAR(10),
    @Host       INT,
    @Start      INT = 1000
AS
BEGIN
    SET NOCOUNT ON;
    SET XACT_ABORT ON;

    DECLARE @MmrA INT, @MmrB INT, @MatchId INT;

    BEGIN TRAN;

    IF NOT EXISTS (SELECT 1 FROM DuelRanked WITH (UPDLOCK, HOLDLOCK) WHERE Name = @NameA)
        INSERT INTO DuelRanked (Name, Account, MMR, PeakMMR) VALUES (@NameA, @AccA, @Start, @Start);

    IF NOT EXISTS (SELECT 1 FROM DuelRanked WITH (UPDLOCK, HOLDLOCK) WHERE Name = @NameB)
        INSERT INTO DuelRanked (Name, Account, MMR, PeakMMR) VALUES (@NameB, @AccB, @Start, @Start);

    SELECT @MmrA = MMR FROM DuelRanked WHERE Name = @NameA;
    SELECT @MmrB = MMR FROM DuelRanked WHERE Name = @NameB;

    INSERT INTO DuelRankedMatch (Status, NameA, NameB, MmrABefore, MmrBBefore, HostServer)
    VALUES (0, @NameA, @NameB, @MmrA, @MmrB, @Host);

    SET @MatchId = CAST(SCOPE_IDENTITY() AS INT);

    COMMIT TRAN;

    SELECT @MatchId AS MatchId, @MmrA AS MmrA, @MmrB AS MmrB;
END
GO

-- -----------------------------------------------------------------------------
-- Settle a match. Numbers only; the names come from the match row.
-- @WinnerSlot: 1 = A won, 2 = B won, 0 = draw.
-- Applied = 1: rated now. Applied = 0: it was settled (or voided) before, the
-- stored result is returned and nothing changes. Applied = -1: no such match.
-- Status in the answer is the match row's status.
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_DuelRanked_Result', 'P') IS NOT NULL DROP PROCEDURE WZ_DuelRanked_Result;
GO
CREATE PROCEDURE WZ_DuelRanked_Result
    @MatchId    INT,
    @WinnerSlot INT,
    @Reason     INT,
    @ScoreA     INT,
    @ScoreB     INT,
    @K          INT = 32,
    @Floor      INT = 0
AS
BEGIN
    SET NOCOUNT ON;
    SET XACT_ABORT ON;

    DECLARE @Status TINYINT, @NameA VARCHAR(10), @NameB VARCHAR(10);

    BEGIN TRAN;

    SELECT @Status = Status, @NameA = NameA, @NameB = NameB
      FROM DuelRankedMatch WITH (UPDLOCK, HOLDLOCK)
     WHERE MatchId = @MatchId;

    IF @NameA IS NULL
    BEGIN
        COMMIT TRAN;
        SELECT -1 AS Applied, -1 AS Status, 0 AS MmrABefore, 0 AS MmrBBefore, 0 AS NewA, 0 AS NewB,
               0 AS DeltaA, 0 AS DeltaB, '' AS NameA, '' AS NameB, '' AS WinnerName;
        RETURN;
    END

    IF @Status <> 0
    BEGIN
        COMMIT TRAN;
        SELECT 0 AS Applied, Status,
               MmrABefore, MmrBBefore,
               ISNULL(MmrAAfter, MmrABefore) AS NewA, ISNULL(MmrBAfter, MmrBBefore) AS NewB,
               ISNULL(MmrAAfter, MmrABefore) - MmrABefore AS DeltaA,
               ISNULL(MmrBAfter, MmrBBefore) - MmrBBefore AS DeltaB,
               NameA, NameB, ISNULL(WinnerName, '') AS WinnerName
          FROM DuelRankedMatch WHERE MatchId = @MatchId;
        RETURN;
    END

    IF @WinnerSlot NOT IN (0, 1, 2) SET @WinnerSlot = 0;

    -- A rating row can only be missing if someone deleted it by hand.
    IF NOT EXISTS (SELECT 1 FROM DuelRanked WITH (UPDLOCK, HOLDLOCK) WHERE Name = @NameA)
        INSERT INTO DuelRanked (Name, Account, MMR, PeakMMR)
        SELECT NameA, '', MmrABefore, MmrABefore FROM DuelRankedMatch WHERE MatchId = @MatchId;

    IF NOT EXISTS (SELECT 1 FROM DuelRanked WITH (UPDLOCK, HOLDLOCK) WHERE Name = @NameB)
        INSERT INTO DuelRanked (Name, Account, MMR, PeakMMR)
        SELECT NameB, '', MmrBBefore, MmrBBefore FROM DuelRankedMatch WHERE MatchId = @MatchId;

    DECLARE @RA INT, @RB INT;
    SELECT @RA = MMR FROM DuelRanked WHERE Name = @NameA;
    SELECT @RB = MMR FROM DuelRanked WHERE Name = @NameB;

    DECLARE @Sa FLOAT;
    SET @Sa = CASE @WinnerSlot WHEN 1 THEN 1.0 WHEN 2 THEN 0.0 ELSE 0.5 END;

    DECLARE @Ea FLOAT;
    SET @Ea = 1.0 / (1.0 + POWER(CAST(10.0 AS FLOAT), (CAST(@RB AS FLOAT) - CAST(@RA AS FLOAT)) / 400.0));

    DECLARE @dA INT;
    SET @dA = CAST(ROUND(@K * (@Sa - @Ea), 0) AS INT);

    DECLARE @NewA INT, @NewB INT;
    SET @NewA = CASE WHEN @RA + @dA < @Floor THEN @Floor ELSE @RA + @dA END;
    SET @NewB = CASE WHEN @RB - @dA < @Floor THEN @Floor ELSE @RB - @dA END;

    UPDATE DuelRanked
       SET MMR = @NewA,
           PeakMMR = CASE WHEN @NewA > PeakMMR THEN @NewA ELSE PeakMMR END,
           Wins   = Wins   + CASE WHEN @WinnerSlot = 1 THEN 1 ELSE 0 END,
           Losses = Losses + CASE WHEN @WinnerSlot = 2 THEN 1 ELSE 0 END,
           Draws  = Draws  + CASE WHEN @WinnerSlot = 0 THEN 1 ELSE 0 END,
           LastMatchAt = GETDATE()
     WHERE Name = @NameA;

    UPDATE DuelRanked
       SET MMR = @NewB,
           PeakMMR = CASE WHEN @NewB > PeakMMR THEN @NewB ELSE PeakMMR END,
           Wins   = Wins   + CASE WHEN @WinnerSlot = 2 THEN 1 ELSE 0 END,
           Losses = Losses + CASE WHEN @WinnerSlot = 1 THEN 1 ELSE 0 END,
           Draws  = Draws  + CASE WHEN @WinnerSlot = 0 THEN 1 ELSE 0 END,
           LastMatchAt = GETDATE()
     WHERE Name = @NameB;

    DECLARE @Winner VARCHAR(10);
    SET @Winner = CASE @WinnerSlot WHEN 1 THEN @NameA WHEN 2 THEN @NameB ELSE NULL END;

    UPDATE DuelRankedMatch
       SET Status = CASE WHEN @WinnerSlot = 0 THEN 2 ELSE 1 END,
           WinnerName = @Winner,
           MmrABefore = @RA, MmrAAfter = @NewA,
           MmrBBefore = @RB, MmrBAfter = @NewB,
           Reason = CASE WHEN @Reason BETWEEN 0 AND 255 THEN @Reason ELSE 0 END,
           ScoreA = CASE WHEN @ScoreA BETWEEN 0 AND 255 THEN @ScoreA ELSE 0 END,
           ScoreB = CASE WHEN @ScoreB BETWEEN 0 AND 255 THEN @ScoreB ELSE 0 END,
           SettledAt = GETDATE()
     WHERE MatchId = @MatchId;

    COMMIT TRAN;

    SELECT 1 AS Applied, CASE WHEN @WinnerSlot = 0 THEN 2 ELSE 1 END AS Status,
           @RA AS MmrABefore, @RB AS MmrBBefore, @NewA AS NewA, @NewB AS NewB,
           @NewA - @RA AS DeltaA, @NewB - @RB AS DeltaB,
           @NameA AS NameA, @NameB AS NameB, ISNULL(@Winner, '') AS WinnerName;
END
GO

-- -----------------------------------------------------------------------------
-- Close a match that never produced a result (nobody turned up, the host went
-- down, the watchdog gave up). No rating changes. Only a match that is still
-- open (Status 0) is touched. @Status 3 = void, 4 = abandoned.
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_DuelRanked_Void', 'P') IS NOT NULL DROP PROCEDURE WZ_DuelRanked_Void;
GO
CREATE PROCEDURE WZ_DuelRanked_Void
    @MatchId    INT,
    @Status     INT = 3
AS
BEGIN
    SET NOCOUNT ON;

    IF @Status NOT IN (3, 4) SET @Status = 3;

    UPDATE DuelRankedMatch
       SET Status = @Status, SettledAt = GETDATE()
     WHERE MatchId = @MatchId AND Status = 0;

    SELECT @@ROWCOUNT AS Result;
END
GO

-- -----------------------------------------------------------------------------
-- Rank-up rewards: the best rank step (division) each character has reached, so
-- a division pays its [Rewards] coins only the first time. Safe to run again.
-- -----------------------------------------------------------------------------
IF OBJECT_ID('DuelRankedBest', 'U') IS NULL
BEGIN
    CREATE TABLE DuelRankedBest (
        Name     VARCHAR(10) NOT NULL PRIMARY KEY,
        BestStep INT NOT NULL DEFAULT 0
    );
END
GO
