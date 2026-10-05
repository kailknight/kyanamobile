-- =============================================================================
-- Friend mail item attachments - schema setup / migration.
--
-- Run against the DataServer's SQL Server database (the one with Character,
-- T_FriendMain and T_FriendMail). Applied by hand, like RedeemCode_Schema.sql.
--
-- Safe to re-run: the table is created only if missing (attachments already
-- stored are never touched) and the procedures are dropped and recreated every
-- run, so re-running after an update always leaves them on the latest logic.
--
-- One row per attached item. ItemData is the item exactly as one inventory
-- slot stores it (16 bytes, serial included - CItemManager::DBItemByteConvert),
-- so an item is never re-created on the way through: what is claimed is the
-- same item, same serial, that was sent.
--
-- The rules the game relies on:
--   * WZ_MailItem_Claim is the ONLY way an item leaves the mailbox, and it only
--     succeeds when it flips Status 0 -> 1 itself. However often Claim is
--     pressed or a packet replayed, an item can be claimed once.
--   * A letter that still has unclaimed items cannot be deleted (the
--     DataServer checks Status = 0 rows before calling WZ_DelMail).
--   * Admin mail (Titan Editor) uses SenderName 'Admin' and ExpireAt NULL:
--     it never returns.
-- =============================================================================

IF OBJECT_ID('T_FriendMailItem', 'U') IS NULL
BEGIN
    CREATE TABLE T_FriendMailItem (
        ItemID      BIGINT IDENTITY(1,1) PRIMARY KEY,
        GUID        INT NOT NULL,                   -- owner of the mailbox (T_FriendMain.GUID)
        MemoIndex   INT NOT NULL,                   -- the letter (T_FriendMail.MemoIndex, per GUID)
        SenderName  VARCHAR(10) NOT NULL,           -- character name, or 'Admin'
        ItemData    VARBINARY(16) NOT NULL,         -- inventory-slot item bytes
        Status      TINYINT NOT NULL DEFAULT 0,     -- 0 waiting, 1 claimed
        CreatedAt   DATETIME NOT NULL DEFAULT GETDATE(),
        ExpireAt    DATETIME NULL,                  -- NULL = never returns
        ClaimedAt   DATETIME NULL,
        ClaimedBy   VARCHAR(10) NULL
    );

    CREATE INDEX IX_T_FriendMailItem_Box ON T_FriendMailItem (GUID, MemoIndex, Status);
    CREATE INDEX IX_T_FriendMailItem_Expire ON T_FriendMailItem (Status, ExpireAt);
END
GO

-- Coin attachments (added later - ALTERed in so an existing table keeps its rows).
-- CoinType 0 = the row is an item (ItemData); 1 WCoinC, 2 WCoinP, 3 Goblin Points,
-- with CoinAmount of it and ItemData unused. Claimed and returned exactly like an item.
IF COL_LENGTH('T_FriendMailItem', 'CoinType') IS NULL
    ALTER TABLE T_FriendMailItem ADD CoinType TINYINT NOT NULL CONSTRAINT DF_T_FriendMailItem_CoinType DEFAULT 0;
IF COL_LENGTH('T_FriendMailItem', 'CoinAmount') IS NULL
    ALTER TABLE T_FriendMailItem ADD CoinAmount INT NOT NULL CONSTRAINT DF_T_FriendMailItem_CoinAmount DEFAULT 0;
GO

-- -----------------------------------------------------------------------------
-- Attach one item to an existing letter. The letter itself is created with the
-- existing WZ_WriteMail (the game and Titan Editor both already use it), then
-- this is called once per item.
--   @ExpireDays <= 0 -> never returns (admin mail).
-- Returns: Result 1 = stored.
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_MailItem_Add', 'P') IS NOT NULL DROP PROCEDURE WZ_MailItem_Add;
GO
CREATE PROCEDURE WZ_MailItem_Add
    @GUID        INT,
    @MemoIndex   INT,
    @SenderName  VARCHAR(10),
    @ItemData    VARBINARY(16),
    @ExpireDays  INT,
    @CoinType    INT = 0,         -- coin row: eFriendMailCoin (Titan Editor omits both)
    @CoinAmount  INT = 0
AS
BEGIN
    SET NOCOUNT ON;

    INSERT INTO T_FriendMailItem (GUID, MemoIndex, SenderName, ItemData, Status, ExpireAt, CoinType, CoinAmount)
    VALUES (@GUID, @MemoIndex, @SenderName, @ItemData, 0,
            CASE WHEN @ExpireDays > 0 THEN DATEADD(DAY, @ExpireDays, GETDATE()) ELSE NULL END,
            @CoinType, @CoinAmount);

    SELECT 1 AS Result, CAST(SCOPE_IDENTITY() AS BIGINT) AS ItemID;
END
GO

-- -----------------------------------------------------------------------------
-- Claim one item. Succeeds only if THIS call flips it from waiting to claimed.
-- Returns: Result 1 = claimed now (ItemData returned), 0 = not claimable
-- (already claimed, not in this mailbox, or no such item).
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_MailItem_Claim', 'P') IS NOT NULL DROP PROCEDURE WZ_MailItem_Claim;
GO
CREATE PROCEDURE WZ_MailItem_Claim
    @GUID       INT,
    @ItemID     BIGINT,
    @ClaimedBy  VARCHAR(10)
AS
BEGIN
    SET NOCOUNT ON;

    DECLARE @Claimed TABLE (ItemData VARBINARY(16), CoinType TINYINT, CoinAmount INT);

    UPDATE T_FriendMailItem
       SET Status = 1, ClaimedAt = GETDATE(), ClaimedBy = @ClaimedBy
    OUTPUT inserted.ItemData, inserted.CoinType, inserted.CoinAmount INTO @Claimed
     WHERE ItemID = @ItemID AND GUID = @GUID AND Status = 0;

    IF EXISTS (SELECT 1 FROM @Claimed)
        SELECT 1 AS Result, ItemData, CoinType, CoinAmount FROM @Claimed;
    ELSE
        SELECT 0 AS Result, CAST(NULL AS VARBINARY(16)) AS ItemData, CAST(0 AS TINYINT) AS CoinType, 0 AS CoinAmount;
END
GO

-- -----------------------------------------------------------------------------
-- Undo a claim the game could not complete (the player left between the claim
-- and the item reaching their inventory). Only reverses a claim made by the
-- same character in the last 10 minutes.
-- -----------------------------------------------------------------------------
IF OBJECT_ID('WZ_MailItem_Unclaim', 'P') IS NOT NULL DROP PROCEDURE WZ_MailItem_Unclaim;
GO
CREATE PROCEDURE WZ_MailItem_Unclaim
    @GUID       INT,
    @ItemID     BIGINT,
    @ClaimedBy  VARCHAR(10)
AS
BEGIN
    SET NOCOUNT ON;

    UPDATE T_FriendMailItem
       SET Status = 0, ClaimedAt = NULL, ClaimedBy = NULL
     WHERE ItemID = @ItemID AND GUID = @GUID AND Status = 1
       AND ClaimedBy = @ClaimedBy AND ClaimedAt > DATEADD(MINUTE, -10, GETDATE());

    SELECT @@ROWCOUNT AS Result;
END
GO

-- -----------------------------------------------------------------------------
-- Repair half-finished friendships.
--
-- WZ_WaitFriendAdd writes the requester's row with Del=1; accepting
-- (WZ_FriendAdd) only adds the acceptor's row, so the requester's stayed
-- Del=1 for good: each side saw the other as offline, item mail refused the
-- pair, and adding again said "already registered". The DataServer now clears
-- it on accept; this fixes the pairs accepted before that.
--
-- Only rows whose reverse row exists and is live are touched - those are
-- accepted friendships. A Del=1 row with no reverse row is a request still
-- waiting, or someone who removed you, and is left alone.
-- -----------------------------------------------------------------------------
UPDATE f
   SET Del = 0
  FROM T_FriendList f
 WHERE f.Del = 1
   AND EXISTS (SELECT 1 FROM T_FriendList r
                WHERE r.GUID = f.FriendGuid AND r.FriendGuid = f.GUID AND r.Del = 0);
GO
