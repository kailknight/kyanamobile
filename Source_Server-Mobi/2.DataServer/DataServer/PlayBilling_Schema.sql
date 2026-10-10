-- =============================================================================
-- Google Play WCoin (C) recharge - schema setup / migration.
--
-- Run against the DataServer's SQL Server database (the one with
-- CashShopData). Applied by hand, like RedeemCode_Schema.sql.
--
-- Safe to re-run: the table is created only if missing and the procedure is
-- dropped and recreated every run.
--
-- One row per Google purchase, keyed on the purchase token. The rules:
--   * WZ_PlayStoreCredit is the only thing that writes here, and it records the
--     purchase and adds the WCoin in one transaction. A token already in the
--     table is answered "already credited" and adds nothing, however often the
--     phone or a replayed packet sends it.
--   * The DataServer only calls it after Google has said the purchase is paid
--     (PlayBilling.cpp). The WCoin amount comes from PlayBilling.ini, never
--     from the client.
-- =============================================================================

IF OBJECT_ID('PlayStorePurchases', 'U') IS NULL
BEGIN
    CREATE TABLE PlayStorePurchases (
        PurchaseToken   VARCHAR(512) NOT NULL PRIMARY KEY,
        OrderId         VARCHAR(64)  NOT NULL DEFAULT '',
        AccountID       VARCHAR(10)  NOT NULL,
        ProductId       VARCHAR(32)  NOT NULL,
        WCoinC          INT          NOT NULL,
        CreatedAt       DATETIME     NOT NULL DEFAULT GETDATE()
    );

    CREATE INDEX IX_PlayStorePurchases_Account ON PlayStorePurchases (AccountID, CreatedAt);
END
GO

IF OBJECT_ID('WZ_PlayStoreCredit', 'P') IS NOT NULL
    DROP PROCEDURE WZ_PlayStoreCredit;
GO

-- Result: 0 = credited now, 1 = this token was credited before (nothing added),
--         2 = Google shows it consumed but it was never credited here (refused).
CREATE PROCEDURE WZ_PlayStoreCredit
    @Token      VARCHAR(512),
    @OrderId    VARCHAR(64),
    @AccountID  VARCHAR(10),
    @ProductId  VARCHAR(32),
    @WCoinC     INT,
    @Consumed   INT
AS
BEGIN
    SET NOCOUNT ON;
    SET XACT_ABORT ON;

    BEGIN TRAN;

    IF EXISTS (SELECT 1 FROM PlayStorePurchases WITH (UPDLOCK, HOLDLOCK) WHERE PurchaseToken = @Token)
    BEGIN
        COMMIT TRAN;
        SELECT 1 AS Result;
        RETURN;
    END

    -- Consumed on Google's side but never recorded here: not something this
    -- server did, so it does not pay out.
    IF @Consumed <> 0
    BEGIN
        COMMIT TRAN;
        SELECT 2 AS Result;
        RETURN;
    END

    INSERT INTO PlayStorePurchases (PurchaseToken, OrderId, AccountID, ProductId, WCoinC)
    VALUES (@Token, @OrderId, @AccountID, @ProductId, @WCoinC);

    IF NOT EXISTS (SELECT 1 FROM CashShopData WHERE AccountID = @AccountID)
        INSERT INTO CashShopData (AccountID, WCoinC, WCoinP, GoblinPoint, Ruud) VALUES (@AccountID, 0, 0, 0, 0);

    UPDATE CashShopData
    SET WCoinC = CASE WHEN CAST(WCoinC AS BIGINT) + @WCoinC > 2147483647 THEN 2147483647 ELSE WCoinC + @WCoinC END
    WHERE AccountID = @AccountID;

    COMMIT TRAN;

    SELECT 0 AS Result;
END
GO
