-- =============================================================================
-- Redemption Code system - schema setup / migration.
--
-- Run this against the DataServer's SQL Server database (same DB as
-- MEMB_INFO / Character / CardPhone / etc). Not part of the build - there is
-- no existing convention in this codebase for auto-applying schema scripts,
-- so this is applied by hand like the rest of this project's DB setup.
--
-- Safe to re-run: tables are only created if missing (existing data, codes,
-- and redemptions are never touched), new columns are added only if they
-- don't already exist yet, and the two stored procedures are dropped and
-- recreated every run so they always end up on the latest logic. This lets
-- you re-run this same file after pulling an updated copy instead of having
-- to hand-diff or drop everything first.
--
-- Item detail fields on CustomRedemptionCodeItems are deliberately shaped to
-- match GDCreateItemSend's own parameter breakdown (DSProtocol.h:778, see the
-- call in MocNap.cpp:442/448) rather than a generic "Option1" byte, so a
-- GameServer-side grant is a direct 1:1 mapping with no repacking:
--   ItemLevel      -> level
--   ItemSkill      -> Option1 (Skill, 0/1)
--   ItemLuck       -> Option2 (Luck, 0/1)
--   ItemOption     -> Option3 (the +option value, 0-7)
--   ItemExcellent  -> NewOption (excellent bitmask, 0-63)
--   ItemDuration   -> duration (seconds; 0 = permanent)
-- Sockets (5 slots, ItemSocket1..5 + SocketBonus) map onto
-- GDCreateItemSend's own SocketOption[MAX_SOCKET_OPTION]/SocketOptionBonus
-- parameters the same direct way. LootIndex/Anc/JewelOfHarmonyOption/
-- ItemOptionEx are still NOT admin-configurable - the GameServer-side grant
-- call passes fixed defaults (-1, 0, 0, 0) for those.
-- =============================================================================

IF OBJECT_ID('CustomRedemptionCodes', 'U') IS NULL
BEGIN
    CREATE TABLE CustomRedemptionCodes (
        CodeID       INT IDENTITY PRIMARY KEY,
        Code         VARCHAR(32) NOT NULL UNIQUE,
        MaxUses      INT NOT NULL DEFAULT 0,       -- 0 = unlimited
        UsedCount    INT NOT NULL DEFAULT 0,
        ExpiresAt    DATETIME NULL,                -- NULL = never expires
        IsActive     BIT NOT NULL DEFAULT 1,       -- admin kill switch
        Description  VARCHAR(255) NULL,
        CreatedAt    DATETIME NOT NULL DEFAULT GETDATE(),
        -- Currency rewards. Per-CODE, not per-item: these aren't items, they
        -- have no inventory footprint and no item options, so hanging them off
        -- CustomRedemptionCodeItems would mean a row that is an item in every
        -- column except the ones that matter. Granted in one call to
        -- CCashShop::GDCashShopAddPointSaveSend (CashShop.cpp:1701), which is
        -- the engine's own primitive for all four and handles both the DB save
        -- and the live player's Coin1/Coin2/Coin3/Ruud.
        RewardWCoinC      INT NOT NULL DEFAULT 0,  -- -> lpObj->Coin1
        RewardWCoinP      INT NOT NULL DEFAULT 0,  -- -> lpObj->Coin2
        RewardGoblinPoint INT NOT NULL DEFAULT 0,  -- -> lpObj->Coin3
        RewardRuud        INT NOT NULL DEFAULT 0   -- -> lpObj->Ruud
    );
END
ELSE
BEGIN
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodes') AND name = 'RewardWCoinC')
        ALTER TABLE CustomRedemptionCodes ADD RewardWCoinC INT NOT NULL DEFAULT 0;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodes') AND name = 'RewardWCoinP')
        ALTER TABLE CustomRedemptionCodes ADD RewardWCoinP INT NOT NULL DEFAULT 0;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodes') AND name = 'RewardGoblinPoint')
        ALTER TABLE CustomRedemptionCodes ADD RewardGoblinPoint INT NOT NULL DEFAULT 0;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodes') AND name = 'RewardRuud')
        ALTER TABLE CustomRedemptionCodes ADD RewardRuud INT NOT NULL DEFAULT 0;
END

IF OBJECT_ID('CustomRedemptionCodeItems', 'U') IS NULL
BEGIN
    CREATE TABLE CustomRedemptionCodeItems (
        CodeItemID    INT IDENTITY PRIMARY KEY,
        CodeID        INT NOT NULL FOREIGN KEY REFERENCES CustomRedemptionCodes(CodeID),
        ItemIndex     INT NOT NULL,                -- this engine's packed item index (BConverITEM)
        ItemLevel     TINYINT NOT NULL DEFAULT 0,  -- + level, 0-15
        ItemSkill     BIT NOT NULL DEFAULT 0,
        ItemLuck      BIT NOT NULL DEFAULT 0,
        ItemOption    TINYINT NOT NULL DEFAULT 0,  -- + option value, 0-7
        ItemExcellent TINYINT NOT NULL DEFAULT 0,  -- excellent bitmask, 0-63
        ItemDuration  INT NOT NULL DEFAULT 0,       -- seconds; 0 = permanent
        Quantity      INT NOT NULL DEFAULT 1,
        -- Sockets: 5 slots + one bonus byte, matching GDCreateItemSend's
        -- SocketOption[MAX_SOCKET_OPTION]/SocketOptionBonus exactly (DSProtocol.h,
        -- MAX_SOCKET_OPTION=5). 0xFF = empty slot, matching the engine's own
        -- "no socket" sentinel (see GDCreateItemSend's own NULL-SocketOption
        -- default fill).
        ItemSocket1   TINYINT NOT NULL DEFAULT 255,
        ItemSocket2   TINYINT NOT NULL DEFAULT 255,
        ItemSocket3   TINYINT NOT NULL DEFAULT 255,
        ItemSocket4   TINYINT NOT NULL DEFAULT 255,
        ItemSocket5   TINYINT NOT NULL DEFAULT 255,
        SocketBonus   TINYINT NOT NULL DEFAULT 0,
        -- "380 Option" - maps to GDCreateItemSend's ItemOptionEx bit 0x80
        -- (C380ItemOption::Is380Item, 380ItemOption.cpp:114). Purely cosmetic
        -- (2 extra tooltip lines from the client's own static ItemAddOption.bmd,
        -- keyed by item Type) unless the item's type also appears in the
        -- client's Item\380ItemType.txt - that data file decides whether it
        -- carries a real stat bonus too, this flag alone never breaks anything
        -- for a type with no such entry.
        Item380       BIT NOT NULL DEFAULT 0,
        -- "Yellow Option" (Jewel of Harmony) - maps to GDCreateItemSend's
        -- JewelOfHarmonyOption parameter, packed there as one byte (high
        -- nibble = category-relative option index, low nibble = required
        -- level 0-15 - wsclientinline.h:1075). The "category" (Weapon/Staff/
        -- Defense) is derived purely from the item's own type at render time
        -- (UIJewelHarmony.cpp's GetItemType) - it is NOT stored in this byte,
        -- so the same index value means different things on different item
        -- types; the admin tool picks the right name list from ItemIndex.
        -- Kept unpacked here for a friendlier admin UI; packed only at grant
        -- time (RedeemCode.cpp) and when rebuilding the wire byte for the
        -- client-side tooltip preview.
        HarmonyOption      TINYINT NOT NULL DEFAULT 0, -- category-relative option index (0-10 for Weapon, 0-8 for Staff/Defense - the item's own type decides which list applies, not this column); 0 = none
        HarmonyOptionLevel TINYINT NOT NULL DEFAULT 0  -- 0-15, the level the item must reach to light the option yellow
    );
END
ELSE
BEGIN
    -- Table already existed from an earlier version of this script (before
    -- sockets/380/harmony existed) - add whatever columns are missing without
    -- touching any existing rows or the codes/items already in them.
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'ItemSocket1')
        ALTER TABLE CustomRedemptionCodeItems ADD ItemSocket1 TINYINT NOT NULL DEFAULT 255;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'ItemSocket2')
        ALTER TABLE CustomRedemptionCodeItems ADD ItemSocket2 TINYINT NOT NULL DEFAULT 255;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'ItemSocket3')
        ALTER TABLE CustomRedemptionCodeItems ADD ItemSocket3 TINYINT NOT NULL DEFAULT 255;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'ItemSocket4')
        ALTER TABLE CustomRedemptionCodeItems ADD ItemSocket4 TINYINT NOT NULL DEFAULT 255;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'ItemSocket5')
        ALTER TABLE CustomRedemptionCodeItems ADD ItemSocket5 TINYINT NOT NULL DEFAULT 255;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'SocketBonus')
        ALTER TABLE CustomRedemptionCodeItems ADD SocketBonus TINYINT NOT NULL DEFAULT 0;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'Item380')
        ALTER TABLE CustomRedemptionCodeItems ADD Item380 BIT NOT NULL DEFAULT 0;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'HarmonyOption')
        ALTER TABLE CustomRedemptionCodeItems ADD HarmonyOption TINYINT NOT NULL DEFAULT 0;
    IF NOT EXISTS (SELECT 1 FROM sys.columns WHERE object_id = OBJECT_ID('CustomRedemptionCodeItems') AND name = 'HarmonyOptionLevel')
        ALTER TABLE CustomRedemptionCodeItems ADD HarmonyOptionLevel TINYINT NOT NULL DEFAULT 0;
END

IF OBJECT_ID('CustomCodeRedemptions', 'U') IS NULL
BEGIN
    CREATE TABLE CustomCodeRedemptions (
        RedemptionID   INT IDENTITY PRIMARY KEY,
        CodeID         INT NOT NULL FOREIGN KEY REFERENCES CustomRedemptionCodes(CodeID),
        AccountID      VARCHAR(20) NOT NULL,        -- matches MEMB_INFO.memb___id width
        CharacterName  VARCHAR(20) NULL,            -- which character received it (audit)
        RedeemedAt     DATETIME NOT NULL DEFAULT GETDATE(),
        CONSTRAINT UQ_CustomCodeRedemptions_Code_Account UNIQUE (CodeID, AccountID)
    );
END

GO

-- =============================================================================
-- WZ_PeekRedeemCode - read-only, non-binding preview.
--
-- No lock is taken and nothing is mutated - this is deliberately advisory
-- (see the plan's Atomicity section for why holding a lock across the
-- GameServer<->DataServer round trip would be unsafe). Called twice in the
-- normal flow: once for the player-visible "Check Code" preview, and again,
-- silently, by GameServer right before it commits - so this proc alone never
-- needs to distinguish those two callers.
--
-- Result codes (match eRedeemCodeResult in Protocol.h):
--   0 = REDEEM_SUCCESS        (unused here, commit-only)
--   1 = REDEEM_OK_TO_REDEEM
--   2 = REDEEM_NOT_FOUND
--   3 = REDEEM_INACTIVE
--   4 = REDEEM_EXPIRED
--   5 = REDEEM_CAP_REACHED
--   6 = REDEEM_ALREADY_REDEEMED
-- =============================================================================
IF OBJECT_ID('WZ_PeekRedeemCode', 'P') IS NOT NULL
    DROP PROCEDURE WZ_PeekRedeemCode;
GO

CREATE PROCEDURE WZ_PeekRedeemCode
    @Code VARCHAR(32),
    @AccountID VARCHAR(20)
AS
BEGIN
    SET NOCOUNT ON;

    DECLARE @CodeID INT, @MaxUses INT, @UsedCount INT, @ExpiresAt DATETIME, @IsActive BIT;

    SELECT
        @CodeID = CodeID, @MaxUses = MaxUses, @UsedCount = UsedCount,
        @ExpiresAt = ExpiresAt, @IsActive = IsActive
    FROM CustomRedemptionCodes
    WHERE Code = @Code;

    IF @CodeID IS NULL
    BEGIN
        SELECT 2 AS Result;
        RETURN;
    END

    IF @IsActive = 0
    BEGIN
        SELECT 3 AS Result;
        RETURN;
    END

    IF @ExpiresAt IS NOT NULL AND @ExpiresAt <= GETDATE()
    BEGIN
        SELECT 4 AS Result;
        RETURN;
    END

    IF @MaxUses > 0 AND @UsedCount >= @MaxUses
    BEGIN
        SELECT 5 AS Result;
        RETURN;
    END

    IF EXISTS (SELECT 1 FROM CustomCodeRedemptions WHERE CodeID = @CodeID AND AccountID = @AccountID)
    BEGIN
        SELECT 6 AS Result;
        RETURN;
    END

    -- Clear to redeem - return the result code plus the full bundle in one
    -- round trip (CQueryManager::ExecQuery expects exactly one result set per
    -- call, so this SELECT is the only thing sent back).
    SELECT
        1 AS Result,
        c.RewardWCoinC, c.RewardWCoinP, c.RewardGoblinPoint, c.RewardRuud,
        -- LEFT JOIN, not a plain SELECT off the items table: a currency-only
        -- code has no item rows at all, and an empty result set makes the
        -- DataServer's reader (RedeemCode.cpp) fail closed with SERVER_ERROR
        -- because it never sees a Result column. Joining from the code row
        -- guarantees exactly one row even with zero items.
        --
        -- ISNULL(...,-1) on ItemIndex is what marks that no-items row as
        -- "not an item": the reader already skips any row whose ItemIndex is
        -- negative, so a currency-only code yields ItemCount 0 naturally.
        ISNULL(i.ItemIndex, -1) AS ItemIndex,
        ISNULL(i.ItemLevel, 0) AS ItemLevel, ISNULL(i.ItemSkill, 0) AS ItemSkill,
        ISNULL(i.ItemLuck, 0) AS ItemLuck, ISNULL(i.ItemOption, 0) AS ItemOption,
        ISNULL(i.ItemExcellent, 0) AS ItemExcellent, ISNULL(i.ItemDuration, 0) AS ItemDuration,
        ISNULL(i.Quantity, 0) AS Quantity,
        ISNULL(i.ItemSocket1, 255) AS ItemSocket1, ISNULL(i.ItemSocket2, 255) AS ItemSocket2,
        ISNULL(i.ItemSocket3, 255) AS ItemSocket3, ISNULL(i.ItemSocket4, 255) AS ItemSocket4,
        ISNULL(i.ItemSocket5, 255) AS ItemSocket5, ISNULL(i.SocketBonus, 0) AS SocketBonus,
        ISNULL(i.Item380, 0) AS Item380, ISNULL(i.HarmonyOption, 0) AS HarmonyOption,
        ISNULL(i.HarmonyOptionLevel, 0) AS HarmonyOptionLevel
    FROM CustomRedemptionCodes c
    LEFT JOIN CustomRedemptionCodeItems i ON i.CodeID = c.CodeID
    WHERE c.CodeID = @CodeID;
END

GO

-- =============================================================================
-- WZ_CommitRedeemCode - the one authoritative, atomic gate.
--
-- Re-runs the exact same checks as WZ_PeekRedeemCode, but this time inside a
-- transaction holding UPDLOCK+HOLDLOCK on the code's own row for the duration
-- of this single EXEC call only. Two different accounts hitting a code at its
-- cap boundary at the same instant serialize here: whichever transaction's
-- SELECT executes first holds the row lock until it commits (or rolls back),
-- and the second one's identical SELECT blocks until then and re-reads the
-- post-commit UsedCount - so it is never possible for both to observe
-- "under the cap" and both proceed. UNIQUE(CodeID, AccountID) is the
-- independent second backstop for the same-account case.
--
-- Result codes: same as WZ_PeekRedeemCode, plus:
--   0 = REDEEM_SUCCESS   (this proc's only success outcome)
-- =============================================================================
IF OBJECT_ID('WZ_CommitRedeemCode', 'P') IS NOT NULL
    DROP PROCEDURE WZ_CommitRedeemCode;
GO

CREATE PROCEDURE WZ_CommitRedeemCode
    @Code VARCHAR(32),
    @AccountID VARCHAR(20),
    @CharacterName VARCHAR(20)
AS
BEGIN
    SET NOCOUNT ON;

    DECLARE @CodeID INT, @MaxUses INT, @UsedCount INT, @ExpiresAt DATETIME, @IsActive BIT;

    BEGIN TRANSACTION;

    SELECT
        @CodeID = CodeID, @MaxUses = MaxUses, @UsedCount = UsedCount,
        @ExpiresAt = ExpiresAt, @IsActive = IsActive
    FROM CustomRedemptionCodes WITH (UPDLOCK, HOLDLOCK)
    WHERE Code = @Code;

    IF @CodeID IS NULL
    BEGIN
        ROLLBACK TRANSACTION;
        SELECT 2 AS Result;
        RETURN;
    END

    IF @IsActive = 0
    BEGIN
        ROLLBACK TRANSACTION;
        SELECT 3 AS Result;
        RETURN;
    END

    IF @ExpiresAt IS NOT NULL AND @ExpiresAt <= GETDATE()
    BEGIN
        ROLLBACK TRANSACTION;
        SELECT 4 AS Result;
        RETURN;
    END

    IF @MaxUses > 0 AND @UsedCount >= @MaxUses
    BEGIN
        ROLLBACK TRANSACTION;
        SELECT 5 AS Result;
        RETURN;
    END

    IF EXISTS (SELECT 1 FROM CustomCodeRedemptions WHERE CodeID = @CodeID AND AccountID = @AccountID)
    BEGIN
        ROLLBACK TRANSACTION;
        SELECT 6 AS Result;
        RETURN;
    END

    INSERT INTO CustomCodeRedemptions (CodeID, AccountID, CharacterName)
    VALUES (@CodeID, @AccountID, @CharacterName);

    UPDATE CustomRedemptionCodes SET UsedCount = UsedCount + 1 WHERE CodeID = @CodeID;

    COMMIT TRANSACTION;

    -- Authoritative-at-commit-time bundle, in case an admin edited it between
    -- this player's preview and their confirm - cheap to re-select, and it's
    -- what GameServer actually grants.
    SELECT
        0 AS Result,
        c.RewardWCoinC, c.RewardWCoinP, c.RewardGoblinPoint, c.RewardRuud,
        -- LEFT JOIN, not a plain SELECT off the items table: a currency-only
        -- code has no item rows at all, and an empty result set makes the
        -- DataServer's reader (RedeemCode.cpp) fail closed with SERVER_ERROR
        -- because it never sees a Result column. Joining from the code row
        -- guarantees exactly one row even with zero items.
        --
        -- ISNULL(...,-1) on ItemIndex is what marks that no-items row as
        -- "not an item": the reader already skips any row whose ItemIndex is
        -- negative, so a currency-only code yields ItemCount 0 naturally.
        ISNULL(i.ItemIndex, -1) AS ItemIndex,
        ISNULL(i.ItemLevel, 0) AS ItemLevel, ISNULL(i.ItemSkill, 0) AS ItemSkill,
        ISNULL(i.ItemLuck, 0) AS ItemLuck, ISNULL(i.ItemOption, 0) AS ItemOption,
        ISNULL(i.ItemExcellent, 0) AS ItemExcellent, ISNULL(i.ItemDuration, 0) AS ItemDuration,
        ISNULL(i.Quantity, 0) AS Quantity,
        ISNULL(i.ItemSocket1, 255) AS ItemSocket1, ISNULL(i.ItemSocket2, 255) AS ItemSocket2,
        ISNULL(i.ItemSocket3, 255) AS ItemSocket3, ISNULL(i.ItemSocket4, 255) AS ItemSocket4,
        ISNULL(i.ItemSocket5, 255) AS ItemSocket5, ISNULL(i.SocketBonus, 0) AS SocketBonus,
        ISNULL(i.Item380, 0) AS Item380, ISNULL(i.HarmonyOption, 0) AS HarmonyOption,
        ISNULL(i.HarmonyOptionLevel, 0) AS HarmonyOptionLevel
    FROM CustomRedemptionCodes c
    LEFT JOIN CustomRedemptionCodeItems i ON i.CodeID = c.CodeID
    WHERE c.CodeID = @CodeID;
END
