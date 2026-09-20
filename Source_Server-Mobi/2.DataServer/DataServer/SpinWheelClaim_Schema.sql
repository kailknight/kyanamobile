-- =============================================================================
-- Spin Wheel claim box - schema setup / migration.
--
-- Run this against the DataServer's SQL Server database (the same DB as
-- MEMB_INFO / Character / EventInventory / etc). Not part of the build - this
-- codebase has no convention for auto-applying schema scripts, so it is applied
-- by hand like RedeemCode_Schema.sql beside it.
--
-- Safe to re-run: the table is only created if it is missing, so existing held
-- prizes are never touched.
--
-- WHAT THIS STORES
-- A prize the player had no inventory room for at the moment the wheel granted
-- it. Held per CHARACTER (not per account - the prize was won by a character and
-- is claimed back into that character's inventory).
--
-- Items is a fixed blob of SPIN_CLAIM_SIZE (8) x 16-byte item records, exactly
-- the layout EventInventory uses: written with
-- gItemManager.DBItemByteConvert and read back with
-- gItemManager.ConvertItemByte. A 0xFF-filled record is an empty slot, which is
-- also what the DataServer returns for a character with no row yet.
--
-- Deliberately a blob rather than per-option columns (the shape RedeemCode uses)
-- because this list is a fixed 8 slots and the blob is what the GameServer's two
-- existing per-player load/save hooks already move - so it needs no new
-- serialisation code and cannot disagree with the inventory's own format.
--
-- NOTE: the items in this blob have NO DataServer-issued serial. The claim box
-- holds a prize TEMPLATE; the item only becomes real (and gets a serial) when
-- the player actually claims it. That is deliberate - it means a GameServer
-- crash can never orphan a live serial for an item nobody holds.
-- =============================================================================

IF OBJECT_ID('SpinWheelClaim', 'U') IS NULL
BEGIN
    CREATE TABLE SpinWheelClaim (
        Name    VARCHAR(10) NOT NULL PRIMARY KEY,   -- character name
        Items   VARBINARY(128) NULL                 -- 8 slots x 16 bytes
    );

    PRINT 'SpinWheelClaim: created.';
END
ELSE
BEGIN
    PRINT 'SpinWheelClaim: already exists, left untouched.';
END
GO

-- Diagnostic: which characters are currently holding unclaimed prizes.
-- (A row whose blob is entirely 0xFF is an empty box and can be ignored.)
-- SELECT Name, DATALENGTH(Items) AS Bytes FROM SpinWheelClaim ORDER BY Name;
