modded class ExpansionMarketMenuItem
{
    // Keep item eligibility separate from the trader gate so prices remain cash prices.
    override bool HasRepBuy()
    {
        if (GetMarketMenu() && BDE_EMU_ReputationAccess.IsBelowRequirement(PlayerBase.Cast(g_Game.GetPlayer()), GetMarketMenu().GetMarketTrader()))
        {
            return false;
        }

        return super.HasRepBuy();
    }

    override void UpdateButtons()
    {
        super.UpdateButtons();
        if (!HasRepBuy())
        {
            market_item_fastbuy.Show(false);
        }
    }
}

modded class ExpansionMarketMenu
{
    override void SetTraderObject(ExpansionTraderObjectBase trader, bool complete)
    {
        super.SetTraderObject(trader, complete);
        if (trader && trader.GetTraderMarket() && m_MarketMenuController)
        {
            m_MarketMenuController.MarketName = trader.GetTraderMarket().DisplayName;
            if (BDE_EMU_ReputationAccess.IsBelowRequirement(PlayerBase.Cast(g_Game.GetPlayer()), trader.GetTraderMarket()))
            {
                m_MarketMenuController.MarketName += " (RP Not Met)";
            }
            m_MarketMenuController.NotifyPropertyChanged("MarketName");
        }
    }
}
