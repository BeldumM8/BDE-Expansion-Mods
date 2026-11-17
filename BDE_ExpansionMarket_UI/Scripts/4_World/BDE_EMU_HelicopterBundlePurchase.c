modded class ExpansionMarketModule
{
    protected bool m_BDE_EMU_QuotingHelicopter;

    override protected bool FindPriceOfPurchaseInternal(ExpansionMarketItem item, ExpansionMarketTraderZone zone, ExpansionMarketTrader trader, PlayerBase player, int amountWanted, inout int price, bool includeAttachments = true, inout ExpansionMarketResult result = ExpansionMarketResult.Success, out ExpansionMarketReserve reserved = NULL, inout map<string, int> removedStock = NULL, out TStringArray outOfStockList = NULL, int level = 0)
    {
        bool previousHelicopter = m_BDE_EMU_QuotingHelicopter;
        if (level == 0)
        {
            m_BDE_EMU_QuotingHelicopter = item && GetGame().IsKindOf(item.ClassName, "ExpansionHelicopterScript");
        }

        bool previousBundleStock;
        if (item)
        {
            previousBundleStock = item.BDE_EMU_BundleStockActive;
            item.BDE_EMU_BundleStockActive = level > 0 && m_BDE_EMU_QuotingHelicopter && item.BDE_EMU_IsHelicopterBundlePart();
        }

        // Retain Expansion's prices, trader permissions, reputation checks and
        // reservation entries. Only attachment stock availability is overridden.
        bool success = super.FindPriceOfPurchaseInternal(item, zone, trader, player, amountWanted, price, includeAttachments, result, reserved, removedStock, outOfStockList, level);

        if (item)
        {
            item.BDE_EMU_BundleStockActive = previousBundleStock;
        }
        m_BDE_EMU_QuotingHelicopter = previousHelicopter;
        return success;
    }
}
