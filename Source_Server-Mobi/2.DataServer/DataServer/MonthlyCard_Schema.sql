-- =============================================================================
-- Monthly WCoin (Kira Pass) - see Working Project\MD Files\MonthlyWCoin-PLAN.md
--
-- One purchase = 30 days. The instant reward is mailed on the next login; each
-- day after that, the daily reward is mailed on that day's first login only (a
-- missed day is lost, like Genshin's Welkin Moon). Buying again adds 30 days;
-- the daily reward never doubles. Days are Philippine calendar days (UTC+8).
--
-- Safe to run again: tables are created only if missing, procedures replaced.
-- =============================================================================

IF OBJECT_ID('MonthlyCard', 'U') IS NULL
BEGIN
    CREATE TABLE MonthlyCard (
        AccountID       VARCHAR(10) NOT NULL PRIMARY KEY,
        EndDay          DATE        NOT NULL,           -- first PH day NOT covered
        LastPaidDay     DATE        NULL,               -- PH day the last daily reward went out
        PendingInstant  INT         NOT NULL DEFAULT 0, -- bought, not mailed yet
        UpdatedAt       DATETIME    NOT NULL DEFAULT GETDATE()
    );
END
GO

-- One row per purchase, so a purchase delivered twice (a webhook retry, Google
-- re-delivering a token) never grants twice.
IF OBJECT_ID('MonthlyCardPurchases', 'U') IS NULL
BEGIN
    CREATE TABLE MonthlyCardPurchases (
        PurchaseID  INT IDENTITY(1,1) PRIMARY KEY,
        AccountID   VARCHAR(10)  NOT NULL,
        Source      VARCHAR(16)  NOT NULL,              -- google | apple | web | admin
        Ref         VARCHAR(200) NOT NULL,              -- order id / token hash / reference
        Days        INT          NOT NULL,
        CreatedAt   DATETIME     NOT NULL DEFAULT GETDATE()
    );
    CREATE UNIQUE INDEX UX_MonthlyCardPurchases_Ref ON MonthlyCardPurchases (Source, Ref);
    CREATE INDEX IX_MonthlyCardPurchases_Account ON MonthlyCardPurchases (AccountID, CreatedAt);
END
GO

-- -----------------------------------------------------------------------------
-- Grant one purchase. Result 0 = granted now, 1 = this purchase was granted
-- before (nothing added).
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_MonthlyCard_Grant', 'P') IS NOT NULL DROP PROCEDURE WZ_MonthlyCard_Grant;
GO
CREATE PROCEDURE WZ_MonthlyCard_Grant
    @AccountID  VARCHAR(10),
    @Source     VARCHAR(16),
    @Ref        VARCHAR(200),
    @Days       INT = 30,
    @Instant    INT = 50
AS
BEGIN
    SET NOCOUNT ON;
    SET XACT_ABORT ON;

    DECLARE @Today DATE = CAST(DATEADD(HOUR, 8, GETUTCDATE()) AS DATE);

    BEGIN TRAN;

    IF EXISTS (SELECT 1 FROM MonthlyCardPurchases WITH (UPDLOCK, HOLDLOCK) WHERE Source = @Source AND Ref = @Ref)
    BEGIN
        COMMIT TRAN;
        SELECT 1 AS Result, (SELECT EndDay FROM MonthlyCard WHERE AccountID = @AccountID) AS EndDay;
        RETURN;
    END

    INSERT INTO MonthlyCardPurchases (AccountID, Source, Ref, Days) VALUES (@AccountID, @Source, @Ref, @Days);

    IF EXISTS (SELECT 1 FROM MonthlyCard WITH (UPDLOCK, HOLDLOCK) WHERE AccountID = @AccountID)
        UPDATE MonthlyCard
           SET EndDay = DATEADD(DAY, @Days, CASE WHEN EndDay > @Today THEN EndDay ELSE @Today END),
               PendingInstant = PendingInstant + @Instant,
               UpdatedAt = GETDATE()
         WHERE AccountID = @AccountID;
    ELSE
        INSERT INTO MonthlyCard (AccountID, EndDay, LastPaidDay, PendingInstant)
        VALUES (@AccountID, DATEADD(DAY, @Days, @Today), NULL, @Instant);

    COMMIT TRAN;

    SELECT 0 AS Result, (SELECT EndDay FROM MonthlyCard WHERE AccountID = @AccountID) AS EndDay;
END
GO

-- -----------------------------------------------------------------------------
-- A character of this account entered the game: take what is owed right now -
-- any instant reward not mailed yet, plus today's daily reward if the card
-- covers today and today wasn't paid. Marks it taken in the same transaction,
-- so two logins at once can't both collect. The caller mails Amount; if it
-- can't, it gives it back with WZ_MonthlyCard_Refund.
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_MonthlyCard_TakeToday', 'P') IS NOT NULL DROP PROCEDURE WZ_MonthlyCard_TakeToday;
GO
CREATE PROCEDURE WZ_MonthlyCard_TakeToday
    @AccountID  VARCHAR(10),
    @Daily      INT = 25
AS
BEGIN
    SET NOCOUNT ON;
    SET XACT_ABORT ON;

    DECLARE @Today DATE = CAST(DATEADD(HOUR, 8, GETUTCDATE()) AS DATE);
    DECLARE @EndDay DATE, @LastPaid DATE, @Pending INT, @Pay INT = 0;

    BEGIN TRAN;

    SELECT @EndDay = EndDay, @LastPaid = LastPaidDay, @Pending = PendingInstant
      FROM MonthlyCard WITH (UPDLOCK, HOLDLOCK)
     WHERE AccountID = @AccountID;

    IF @EndDay IS NULL
    BEGIN
        COMMIT TRAN;
        SELECT 0 AS Amount, 0 AS Instant, 0 AS Daily, 0 AS DaysLeft;
        RETURN;
    END

    IF @EndDay > @Today AND (@LastPaid IS NULL OR @LastPaid < @Today)
        SET @Pay = @Daily;

    UPDATE MonthlyCard
       SET PendingInstant = 0,
           LastPaidDay = CASE WHEN @Pay > 0 THEN @Today ELSE LastPaidDay END,
           UpdatedAt = GETDATE()
     WHERE AccountID = @AccountID;

    COMMIT TRAN;

    SELECT @Pending + @Pay AS Amount, @Pending AS Instant, @Pay AS Daily,
           CASE WHEN @EndDay > @Today THEN DATEDIFF(DAY, @Today, @EndDay) ELSE 0 END AS DaysLeft;
END
GO

-- Put back what TakeToday handed out when the mail could not be written
-- (full mailbox): it is mailed on the next login instead of being lost.
IF OBJECT_ID('WZ_MonthlyCard_Refund', 'P') IS NOT NULL DROP PROCEDURE WZ_MonthlyCard_Refund;
GO
CREATE PROCEDURE WZ_MonthlyCard_Refund
    @AccountID  VARCHAR(10),
    @Amount     INT
AS
BEGIN
    SET NOCOUNT ON;
    UPDATE MonthlyCard SET PendingInstant = PendingInstant + @Amount, UpdatedAt = GETDATE() WHERE AccountID = @AccountID;
    SELECT @@ROWCOUNT AS Result;
END
GO

-- Days left on the card (0 = none / ended) and whether today was paid - for the
-- client panel.
IF OBJECT_ID('WZ_MonthlyCard_Status', 'P') IS NOT NULL DROP PROCEDURE WZ_MonthlyCard_Status;
GO
CREATE PROCEDURE WZ_MonthlyCard_Status
    @AccountID  VARCHAR(10)
AS
BEGIN
    SET NOCOUNT ON;
    DECLARE @Today DATE = CAST(DATEADD(HOUR, 8, GETUTCDATE()) AS DATE);
    SELECT CASE WHEN EndDay > @Today THEN DATEDIFF(DAY, @Today, EndDay) ELSE 0 END AS DaysLeft,
           CASE WHEN LastPaidDay = @Today THEN 1 ELSE 0 END AS PaidToday
      FROM MonthlyCard WHERE AccountID = @AccountID;
END
GO
