-- =============================================================================
-- Hardware ID + IP records - schema setup / migration.
--
-- Run against the DataServer's SQL Server database (the MuOnline one with
-- MEMB_INFO). Applied by hand, like PlayBilling_Schema.sql.
--
-- Safe to re-run: tables are created only if missing and the procedures are
-- dropped and recreated every run.
--
-- HardwareLog  one row per (account, hardware id, IP) ever seen logging in,
--              with first/last time and a count. PC and phone alike; a client
--              too old to send a hardware id is stored with HardwareId = ''.
-- HardwareBan  one row per ban, keyed by the hardware id (or "ip:a.b.c.d" for a
--              client too old to send one). Active = 0 after an unban, so the
--              history stays. A ban also sets MEMB_INFO.BLOC_CODE = 1 on the
--              account; the unban clears it.
--
-- These are a RECORD. The GameServer checks logins against its own
-- Data\Custom\DeviceBan.txt, so editing HardwareBan by hand does not ban or
-- unban anyone - use /bandevice, /unbandevice or the GameServer window.
--
-- Useful lookups:
--   every account on one device:
--     SELECT * FROM HardwareLog WHERE HardwareId = '...' ORDER BY LastSeen DESC
--   every device an account used:
--     SELECT * FROM HardwareLog WHERE AccountID = '...' ORDER BY LastSeen DESC
--   everyone from one IP:
--     SELECT * FROM HardwareLog WHERE IpAddress = '...' ORDER BY LastSeen DESC
-- =============================================================================

IF OBJECT_ID('HardwareLog', 'U') IS NULL
BEGIN
    CREATE TABLE HardwareLog (
        Id          BIGINT IDENTITY(1,1) NOT NULL PRIMARY KEY,
        AccountID   VARCHAR(10) NOT NULL,
        IpAddress   VARCHAR(16) NOT NULL DEFAULT '',
        HardwareId  VARCHAR(32) NOT NULL DEFAULT '',
        Platform    TINYINT     NOT NULL DEFAULT 0,      -- 0 = PC, 1 = phone
        FirstSeen   DATETIME    NOT NULL DEFAULT GETDATE(),
        LastSeen    DATETIME    NOT NULL DEFAULT GETDATE(),
        LoginCount  INT         NOT NULL DEFAULT 1
    );

    CREATE UNIQUE INDEX IX_HardwareLog_Key ON HardwareLog (AccountID, HardwareId, IpAddress);
    CREATE INDEX IX_HardwareLog_Hardware ON HardwareLog (HardwareId);
    CREATE INDEX IX_HardwareLog_Ip ON HardwareLog (IpAddress);
END
GO

IF OBJECT_ID('HardwareBan', 'U') IS NULL
BEGIN
    CREATE TABLE HardwareBan (
        HardwareId  VARCHAR(32) NOT NULL PRIMARY KEY,
        AccountID   VARCHAR(10) NOT NULL DEFAULT '',
        CharName    VARCHAR(10) NOT NULL DEFAULT '',
        IpAddress   VARCHAR(16) NOT NULL DEFAULT '',
        Platform    TINYINT     NOT NULL DEFAULT 0,
        BannedBy    VARCHAR(10) NOT NULL DEFAULT '',
        BannedAt    DATETIME    NOT NULL DEFAULT GETDATE(),
        Active      BIT         NOT NULL DEFAULT 1,
        UnbannedBy  VARCHAR(10) NULL,
        UnbannedAt  DATETIME    NULL
    );
END
GO

IF OBJECT_ID('WZ_HardwareLogin', 'P') IS NOT NULL DROP PROCEDURE WZ_HardwareLogin;
GO
CREATE PROCEDURE WZ_HardwareLogin
    @AccountID  VARCHAR(10),
    @IpAddress  VARCHAR(16),
    @HardwareId VARCHAR(32),
    @Platform   TINYINT
AS
BEGIN
    SET NOCOUNT ON;

    UPDATE HardwareLog
       SET LastSeen = GETDATE(), LoginCount = LoginCount + 1, Platform = @Platform
     WHERE AccountID = @AccountID AND HardwareId = @HardwareId AND IpAddress = @IpAddress;

    IF @@ROWCOUNT = 0
        INSERT INTO HardwareLog (AccountID, IpAddress, HardwareId, Platform)
        VALUES (@AccountID, @IpAddress, @HardwareId, @Platform);
END
GO

IF OBJECT_ID('WZ_HardwareBan', 'P') IS NOT NULL DROP PROCEDURE WZ_HardwareBan;
GO
CREATE PROCEDURE WZ_HardwareBan
    @HardwareId VARCHAR(32),
    @AccountID  VARCHAR(10),
    @CharName   VARCHAR(10),
    @IpAddress  VARCHAR(16),
    @Platform   TINYINT,
    @By         VARCHAR(10),
    @Banned     BIT
AS
BEGIN
    SET NOCOUNT ON;

    IF @Banned = 1
    BEGIN
        UPDATE HardwareBan
           SET AccountID = @AccountID, CharName = @CharName, IpAddress = @IpAddress, Platform = @Platform,
               BannedBy = @By, BannedAt = GETDATE(), Active = 1, UnbannedBy = NULL, UnbannedAt = NULL
         WHERE HardwareId = @HardwareId;

        IF @@ROWCOUNT = 0
            INSERT INTO HardwareBan (HardwareId, AccountID, CharName, IpAddress, Platform, BannedBy)
            VALUES (@HardwareId, @AccountID, @CharName, @IpAddress, @Platform, @By);

        -- The account is banned together with its hardware and IP. Same flag
        -- the in-game /blocacc sets, so the JoinServer refuses it at login.
        IF @AccountID <> ''
        BEGIN
            UPDATE MEMB_INFO SET BLOC_CODE = 1 WHERE MEMB___ID = @AccountID;

            -- BLOC_EXPIRE is optional (the JoinServer only reads BLOC_CODE);
            -- 50 years so it also fits a smalldatetime column (max 2079).
            IF COL_LENGTH('MEMB_INFO', 'BLOC_EXPIRE') IS NOT NULL
                EXEC sp_executesql
                    N'UPDATE MEMB_INFO SET BLOC_EXPIRE = DATEADD(YEAR, 50, GETDATE()) WHERE MEMB___ID = @a',
                    N'@a VARCHAR(10)', @a = @AccountID;
        END
    END
    ELSE
    BEGIN
        -- Unban lifts the account block that came with this ban.
        UPDATE MEMB_INFO
           SET BLOC_CODE = 0
         WHERE MEMB___ID = (SELECT AccountID FROM HardwareBan WHERE HardwareId = @HardwareId AND Active = 1);

        UPDATE HardwareBan
           SET Active = 0, UnbannedBy = @By, UnbannedAt = GETDATE()
         WHERE HardwareId = @HardwareId;
    END
END
GO
