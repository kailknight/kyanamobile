-- =============================================================================
-- Master skill tree pages - see Working Project\MD Files\MasterSkillPages-PLAN.md
--
-- A character has up to 5 master skill pages. Page 1 is free; the others are
-- opened one at a time by the Master Page Scroll. Only the ACTIVE page is live,
-- and its real skills stay in MasterSkillTree.MasterSkill exactly as before:
-- the blob stored here for the active page is only kept in step by the
-- GameServer and is ignored when a character loads. This script never touches
-- MasterSkillTree.
--
-- No row = defaults (page 1 active, 1 page open, names "Page 1".."Page 5", empty
-- pages), so every existing character needs no migration: the tree it has now
-- is its page 1.
--
-- MasterSkill is 360 bytes: 120 entries x 3 bytes [index low][level][index high],
-- an empty entry is FF 00 FF (the same blob MasterSkillTree.MasterSkill holds).
--
-- Safe to run again: tables are created only if missing.
-- Run this BEFORE starting the new DataServer (deploy order: SQL, DataServer,
-- GameServers, client). Without the tables the DataServer logs a message and
-- the pages stay off for the session; nothing else is affected.
-- =============================================================================

IF OBJECT_ID('MasterSkillPageInfo', 'U') IS NULL
BEGIN
    CREATE TABLE MasterSkillPageInfo (
        Name            VARCHAR(10) NOT NULL PRIMARY KEY,   -- character name
        ActivePage      TINYINT     NOT NULL DEFAULT 1,     -- 1..5, the live page
        UnlockedPages   TINYINT     NOT NULL DEFAULT 1      -- 1..5, how many pages are open
    );
END
GO

IF OBJECT_ID('MasterSkillPage', 'U') IS NULL
BEGIN
    CREATE TABLE MasterSkillPage (
        Name            VARCHAR(10)    NOT NULL,            -- character name
        Page            TINYINT        NOT NULL,            -- 1..5
        PageName        VARCHAR(12)    NOT NULL,
        MasterSkill     VARBINARY(360) NOT NULL,
        CONSTRAINT PK_MasterSkillPage PRIMARY KEY (Name, Page)
    );
END
GO

-- -----------------------------------------------------------------------------
-- Optional housekeeping, NOT run by this script: drop the page rows of characters
-- that no longer exist (a deleted character, or the old name of a renamed one).
-- The stored procedures that delete and rename a character do not know these
-- tables. Stale rows are harmless: a new character that reuses a name finds
-- entries of another class and the GameServer drops them on load, refunding the
-- points. Run these two lines now and then if the tables should stay tidy.
-- -----------------------------------------------------------------------------
-- DELETE FROM MasterSkillPage     WHERE Name NOT IN (SELECT Name FROM Character);
-- DELETE FROM MasterSkillPageInfo WHERE Name NOT IN (SELECT Name FROM Character);
-- GO
