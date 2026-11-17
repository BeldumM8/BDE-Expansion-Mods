// Stock-zero parts remain unavailable as standalone purchases.
modded class ExpansionMarketItem
{
    bool BDE_EMU_BundleStockActive;

    bool BDE_EMU_IsHelicopterBundlePart()
    {
        if (MinStockThreshold != 0 || MaxStockThreshold != 0)
        {
            return false;
        }

        string name = ClassName;
        name.ToLower();
        return name == "expansionhelicopterbattery" || name == "expansionhydraulichoses" || name == "expansionigniterplug";
    }

    override bool IsStaticStock()
    {
        return BDE_EMU_BundleStockActive || super.IsStaticStock();
    }
}

modded class ExpansionMarketTraderZone
{
    override void ClearReservedStock(string className, int reserved)
    {
        ExpansionMarketItem item = ExpansionMarketCategory.GetGlobalItem(className);
        if (item && item.BDE_EMU_IsHelicopterBundlePart())
        {
            // Bundle quotes never reserve stock for these zero-stock parts.
            // Subtracting here would create negative reservations and fake supply.
            return;
        }
        super.ClearReservedStock(className, reserved);
    }
}
