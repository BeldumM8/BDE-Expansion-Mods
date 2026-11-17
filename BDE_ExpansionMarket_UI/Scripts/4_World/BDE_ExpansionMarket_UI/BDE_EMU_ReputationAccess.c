class BDE_EMU_ReputationAccess
{
    static int GetDisplayRequirement(ExpansionMarketItem item, ExpansionMarketTrader trader)
    {
#ifdef EXPANSIONMODHARDLINE
        auto settings = GetExpansionSettings().GetHardline();
        if (settings.UseReputation)
        {
            if (item && settings.UseItemRarityForMarketPurchase && item.m_RequiredRep > 0)
            {
                return item.m_RequiredRep;
            }
            if (trader)
            {
                return Math.Max(0, trader.MinRequiredReputation);
            }
        }
#endif
        return 0;
    }

    static bool IsBelowRequirement(PlayerBase player, ExpansionMarketTrader trader)
    {
#ifdef EXPANSIONMODHARDLINE
        if (!player && g_Game.IsClient())
        {
            player = PlayerBase.Cast(g_Game.GetPlayer());
        }

        if (player && trader && GetExpansionSettings().GetHardline().UseReputation)
        {
            return player.Expansion_GetReputation() < trader.MinRequiredReputation;
        }
#endif
        return false;
    }
}

modded class ExpansionMarketModule
{
    override bool CheckCanUseTrader(PlayerBase player, ExpansionTraderObjectBase trader)
    {
        if (!player || !trader || !trader.GetTraderMarket())
        {
            return false;
        }

        ExpansionMarketTrader market = trader.GetTraderMarket();
        if (!BDE_EMU_ReputationAccess.IsBelowRequirement(player, market))
        {
            return super.CheckCanUseTrader(player, trader);
        }

        // Relax only the lower bound for this synchronous access check.
        // Faction and maximum-reputation restrictions still use Expansion's checks.
        int minimum = market.MinRequiredReputation;
        market.MinRequiredReputation = int.MIN;
        bool allowed = super.CheckCanUseTrader(player, trader);
        market.MinRequiredReputation = minimum;
        return allowed;
    }

    override bool FindSellPrice(notnull PlayerBase player, array<EntityAI> items, int stock, int amountWanted, ExpansionMarketSell sell, bool includeAttachments = true, out ExpansionMarketResult result = ExpansionMarketResult.Success, out string failedClassName = "")
    {
        bool success = super.FindSellPrice(player, items, stock, amountWanted, sell, includeAttachments, result, failedClassName);
        // Apply after PVP/PVE and acquisition-mode pricing, including attachments.
        // This same quote is used by the client cards and server transactions.
        if (sell && sell.Trader && BDE_EMU_ReputationAccess.IsBelowRequirement(player, sell.Trader.GetTraderMarket()) && sell.Price > 0)
        {
            sell.Price = Math.Floor(sell.Price * 0.75);
        }
        return success;
    }

    override private void Exec_RequestPurchase(notnull PlayerBase player, int itemID, int count, int currentPrice, ExpansionTraderObjectBase trader, bool includeAttachments = true, int skinIndex = -1, TIntArray attachmentIDs = NULL)
    {
        if (trader && BDE_EMU_ReputationAccess.IsBelowRequirement(player, trader.GetTraderMarket()))
        {
            m_Exec_RequestPurchase_Called = true;
            ExpansionNotification("Trader reputation", "This trader is sell-only until you meet its reputation requirement. Sales currently pay 25% less.").Error(player.GetIdentity());
            return;
        }

        super.Exec_RequestPurchase(player, itemID, count, currentPrice, trader, includeAttachments, skinIndex, attachmentIDs);
    }
}

// Expansion does not normally send trader reputation bounds to the client.
modded class ExpansionTraderObjectBase
{
    override void WriteCategories(ParamsWriteContext ctx)
    {
        super.WriteCategories(ctx);
        ctx.Write(m_Trader.MinRequiredReputation);
        ctx.Write(m_Trader.MaxRequiredReputation);
    }

    override bool ReadCategories(ParamsReadContext ctx)
    {
        if (!super.ReadCategories(ctx))
        {
            return false;
        }

        if (!ctx.Read(m_Trader.MinRequiredReputation))
        {
            return false;
        }

        return ctx.Read(m_Trader.MaxRequiredReputation);
    }
}
