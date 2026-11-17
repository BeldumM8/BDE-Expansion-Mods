modded class ExpansionMarketModule
{
    static ref ScriptInvoker SI_BDELoadoutOpen = new ScriptInvoker;
    static ref ScriptInvoker SI_BDELoadoutOffers = new ScriptInvoker;
    static ref ScriptInvoker SI_BDELoadoutResult = new ScriptInvoker;

    ref array<ref BDE_EMU_LoadoutOffer> BDE_LM_ClientOffers = new array<ref BDE_EMU_LoadoutOffer>;
    protected ref array<ref BDE_EMU_LoadoutStation> m_BDELMClientStations = new array<ref BDE_EMU_LoadoutStation>;
    protected ref array<EntityAI> m_BDELMEntities = new array<EntityAI>;
    protected ref array<string> m_BDELMNames = new array<string>;
    protected ref map<string, int> m_BDELMRequests = new map<string, int>;
    protected int m_BDELMClientRequest;
    protected bool m_BDELMInitialized;
    static ref ScriptInvoker SI_BDELoadoutPrepared = new ScriptInvoker;
    protected ref map<string, ref BDE_EMU_CheckoutPlan> m_BDELMPlans = new map<string, ref BDE_EMU_CheckoutPlan>;
    protected int m_BDELMToken;

    override void OnInit()
    {
        super.OnInit();
        Expansion_RegisterServerRPC("RPC_BDE_LMStations");
        Expansion_RegisterClientRPC("RPC_BDE_LMStationsReply");
        Expansion_RegisterServerRPC("RPC_BDE_LMOffers");
        Expansion_RegisterClientRPC("RPC_BDE_LMOffersReply");
        Expansion_RegisterServerRPC("RPC_BDE_LMBuy");
        Expansion_RegisterServerRPC("RPC_BDE_LMCheckout");
        Expansion_RegisterServerRPC("RPC_BDE_LMConfirm");
        Expansion_RegisterServerRPC("RPC_BDE_LMCancel");
        Expansion_RegisterClientRPC("RPC_BDE_LMPrepared");
        Expansion_RegisterServerRPC("RPC_BDE_LMAmmo");
        Expansion_RegisterClientRPC("RPC_BDE_LMAmmoReply");
        Expansion_RegisterClientRPC("RPC_BDE_LMResult");
    }

    void BDE_LM_InitStations()
    {
        if (!GetGame().IsServer() || m_BDELMInitialized)
        {
            return;
        }
        m_BDELMInitialized = true;
        string folder = "$profile:BDE/MarketUI";
        string path = folder + "/LoadoutManagers.json";
        MakeDirectory("$profile:BDE");
        MakeDirectory(folder);
        BDE_EMU_LoadoutSettings settings = new BDE_EMU_LoadoutSettings;
        string error;
        if (!FileExist(path))
        {
            if (!JsonFileLoader<BDE_EMU_LoadoutSettings>.SaveFile(path, settings, error))
            {
                Error("[BDE Loadout] Cannot create settings: " + error);
            }
            return;
        }
        if (!JsonFileLoader<BDE_EMU_LoadoutSettings>.LoadFile(path, settings, error) || !settings || !settings.Stations)
        {
            Error("[BDE Loadout] Settings not loaded; existing file preserved: " + error);
            return;
        }
        // Write missing fields back without replacing existing station values.
        // Also covers stations added manually to an already migrated config.
        if (settings.Version <= 2)
        {
            settings.Version = 2;
            if (!JsonFileLoader<BDE_EMU_LoadoutSettings>.SaveFile(path, settings, error))
            {
                Error("[BDE Loadout] Cannot append settings fields: " + error);
            }
        }
        foreach (BDE_EMU_LoadoutPlacement placement: settings.Stations)
        {
            if (!placement || !placement.Enabled || placement.ClassName == "")
            {
                continue;
            }
            EntityAI entity;
            if (placement.SpawnEntity)
            {
                entity = EntityAI.Cast(GetGame().CreateObjectEx(placement.ClassName, placement.Position, ECE_SETUP | ECE_CREATEPHYSICS | ECE_KEEPHEIGHT | ECE_NOSURFACEALIGN | ECE_NOLIFETIME | ECE_NOPERSISTENCY_WORLD));
                if (entity)
                {
                    entity.SetPosition(placement.Position);
                    entity.SetOrientation(placement.Orientation);
                    entity.SetAllowDamage(false);
                }
            }
            else
            {
                array<Object> nearby = new array<Object>;
                array<CargoBase> cargo = new array<CargoBase>;
                GetGame().GetObjectsAtPosition3D(placement.Position, 0.5, nearby, cargo);
                foreach (Object candidate: nearby)
                {
                    if (candidate.GetType() == placement.ClassName)
                    {
                        entity = EntityAI.Cast(candidate);
                        break;
                    }
                }
            }
            if (!entity)
            {
                Error("[BDE Loadout] Could not resolve station " + placement.Name + " (" + placement.ClassName + ")");
                continue;
            }
            ExpansionTraderNPCBase traderNPC = ExpansionTraderNPCBase.Cast(entity);
            if (traderNPC)
            {
                // Trader NPCs request this wrapper on clients even when used
                // only as a loadout station, with no individual market file.
                if (!traderNPC.GetTraderObject())
                {
                    traderNPC.LoadTrader("");
                }
                if (placement.Loadout != "" && !ExpansionHumanLoadout.Apply(traderNPC, placement.Loadout))
                {
                    Error("[BDE Loadout] Cannot apply loadout " + placement.Loadout + " to " + placement.Name);
                }
            }
            if (m_BDELMEntities.Find(entity) == -1)
            {
                m_BDELMEntities.Insert(entity);
                m_BDELMNames.Insert(placement.Name);
            }
            if (!GetExpansionSettings().GetMarket().GetTraderZoneByPosition(entity.GetPosition()))
            {
                Print("[BDE Loadout] Station outside a trader zone; action disabled: " + placement.Name);
            }
        }
        Print("[BDE Loadout] Loaded " + m_BDELMEntities.Count().ToString() + " stations.");
    }

    bool BDE_LM_IsStation(Object object)
    {
        if (!object || !GetExpansionSettings().GetMarket().MarketSystemEnabled)
        {
            return false;
        }
        if (GetGame().IsServer())
        {
            EntityAI entity = EntityAI.Cast(object);
            return entity && m_BDELMEntities.Find(entity) != -1 && GetExpansionSettings().GetMarket().GetTraderZoneByPosition(entity.GetPosition()) != null;
        }
        int low;
        int high;
        object.GetNetworkID(low, high);
        if (low == 0 && high == 0)
        {
            return false;
        }
        foreach (BDE_EMU_LoadoutStation station: m_BDELMClientStations)
        {
            if (station.Low == low && station.High == high)
            {
                return true;
            }
        }
        return false;
    }

    void BDE_LM_RequestStations()
    {
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMStations");
        rpc.Expansion_Send(null, true);
    }

    protected bool BDE_LM_RateLimit(PlayerIdentity sender, string operation, int delay)
    {
        string key = sender.GetId() + operation;
        int now = GetGame().GetTime();
        int previous;
        if (m_BDELMRequests.Find(key, previous) && now >= previous && now - previous < delay)
        {
            return false;
        }
        m_BDELMRequests.Set(key, now);
        return true;
    }

    private void RPC_BDE_LMStations(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        if (!senderRPC || !BDE_LM_RateLimit(senderRPC, "stations", 2000))
        {
            return;
        }
        array<ref BDE_EMU_LoadoutStation> stations = new array<ref BDE_EMU_LoadoutStation>;
        for (int i = 0; i < m_BDELMEntities.Count(); i++)
        {
            EntityAI entity = m_BDELMEntities[i];
            if (!BDE_LM_IsStation(entity))
            {
                continue;
            }
            BDE_EMU_LoadoutStation station = new BDE_EMU_LoadoutStation;
            entity.GetNetworkID(station.Low, station.High);
            if (station.Low == 0 && station.High == 0)
            {
                continue;
            }
            station.Name = m_BDELMNames[i];
            stations.Insert(station);
        }
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMStationsReply");
        rpc.Write(stations);
        rpc.Expansion_Send(null, true, senderRPC);
    }

    private void RPC_BDE_LMStationsReply(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        array<ref BDE_EMU_LoadoutStation> stations;
        if (ctx.Read(stations) && stations)
        {
            m_BDELMClientStations = stations;
        }
    }

    protected bool BDE_LM_Resolve(PlayerIdentity sender, Object station, int low, int high, int slot, out PlayerBase player, out EntityAI parent, out ExpansionMarketTraderZone zone)
    {
        if (!sender || !BDE_LM_IsStation(station))
        {
            return false;
        }
        player = PlayerBase.GetPlayerByUID(sender.GetId());
        if (!player || !player.IsAlive() || player.IsUnconscious() || player.IsRestrained() || vector.DistanceSq(player.GetPosition(), station.GetPosition()) > 16)
        {
            return false;
        }
        zone = GetExpansionSettings().GetMarket().GetTraderZoneByPosition(station.GetPosition());
        parent = EntityAI.Cast(GetGame().GetObjectByNetworkId(low, high));
        return zone && BDE_EMU_LoadoutUtil.IsEquippedParent(player, parent) && BDE_EMU_LoadoutUtil.HasSlot(parent, slot);
    }

    // Mirror access gates silently while browsing; the normal modded access
    // check runs again on purchase, including other addons' restrictions.
    protected string BDE_LM_Access(PlayerBase player, ExpansionMarketTrader trader, bool selling = false)
    {
#ifdef EXPANSIONMODHARDLINE
        if (GetExpansionSettings().GetHardline().UseReputation)
        {
            int rep = player.Expansion_GetReputation();
            if (!selling && rep < trader.MinRequiredReputation)
            {
                return "Trader RP requirement not met";
            }
            if (rep > trader.MaxRequiredReputation)
            {
                return "Above this trader's RP limit";
            }
        }
#endif
#ifdef ENFUSION_AI_PROJECT
        if (trader.RequiredFaction != "" && (!player.GetGroup() || player.GetGroup().GetFaction().GetName() != trader.RequiredFaction))
        {
            return "Required faction not met";
        }
#endif
#ifdef EXPANSIONMODQUESTS
        if (GetExpansionSettings().GetQuest().EnableQuests && trader.RequiredCompletedQuestID > -1)
        {
            if (!ExpansionQuestModule.GetModuleInstance().HasCompletedQuest(trader.RequiredCompletedQuestID, player.GetIdentity().GetId()))
            {
                return "Required quest not completed";
            }
        }
#endif
        return "";
    }

    protected BDE_EMU_LoadoutOffer BDE_LM_Quote(PlayerBase player, ExpansionTraderObjectBase trader, ExpansionMarketItem item)
    {
        ExpansionMarketTrader market = trader.GetTraderMarket();
        ExpansionMarketTraderZone zone = trader.GetTraderZone();
        BDE_EMU_LoadoutOffer offer = new BDE_EMU_LoadoutOffer;
        trader.GetTraderEntity().GetNetworkID(offer.TraderLow, offer.TraderHigh);
        offer.ItemID = item.ItemID;
        offer.ClassName = item.ClassName;
        offer.TraderName = market.DisplayName;
        ExpansionMarketCategory listedCategory = BDE_LM_ListedCategory(market, item.ClassName);
        if (listedCategory)
        {
            offer.CategoryName = listedCategory.DisplayName;
        }
        offer.Stock = zone.GetStock(item.ClassName);
        if (item.IsStaticStock())
        {
            offer.Stock = -1;
        }
        offer.Reason = BDE_LM_Access(player, market);
        int requirement = BDE_EMU_ReputationAccess.GetDisplayRequirement(item, market);
#ifdef EXPANSIONMODHARDLINE
        // Both requirements must be satisfied when item rarity is enabled.
        if (GetExpansionSettings().GetHardline().UseReputation)
        {
            offer.TraderRequiredRP = market.MinRequiredReputation;
            requirement = Math.Max(requirement, market.MinRequiredReputation);
            if (player.Expansion_GetReputation() < requirement)
            {
                offer.RequiredRP = requirement;
            }
        }
#endif
        ExpansionMarketResult result;
        bool quoted = FindPriceOfPurchaseEx(item, zone, market, player, 1, offer.Price, false, result);
        int startingCount = BDE_LM_StartingAmmoCount(item);
        if (startingCount > 0)
        {
            offer.StartingAmmo = BDE_LM_StartingAmmo(player, zone, item);
            if (!offer.StartingAmmo || offer.StartingAmmo.Price <= 0 || !BDE_LM_AmmoCurrency(trader, offer.StartingAmmo))
            {
                offer.Reason = "Configured starting ammunition is unavailable";
            }
            else if (ExpansionMath.TestAdditionOverflow(offer.Price, offer.StartingAmmo.Price))
            {
                offer.Reason = "Purchase price exceeds supported value";
            }
            else
            {
                offer.Price += offer.StartingAmmo.Price;
            }
        }
        offer.PriceText = trader.GetDisplayPrice(offer.Price, false, true, true);
        if (offer.Reason == "" && (!quoted || result != ExpansionMarketResult.Success))
        {
            if (offer.RequiredRP > 0)
            {
                offer.Reason = "RP requirement not met";
            }
            else if (offer.Stock == 0)
            {
                offer.Reason = "Out of stock";
            }
            else
            {
                offer.Reason = "Unavailable";
            }
        }
        offer.CanBuy = offer.Reason == "" && offer.Price >= 0;
        return offer;
    }

    int BDE_LM_RequestOffers(Object station, EntityAI parent, int slot, string prospectiveType = "")
    {
        m_BDELMClientRequest++;
        BDE_LM_ClientOffers.Clear();
        int low;
        int high;
        parent.GetNetworkID(low, high);
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMOffers");
        rpc.Write(m_BDELMClientRequest);
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(slot);
        rpc.Write(prospectiveType);
        rpc.Expansion_Send(station, true);
        return m_BDELMClientRequest;
    }

    private void RPC_BDE_LMOffers(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        int request;
        int low;
        int high;
        int slot;
        string prospectiveType;
        if (!senderRPC || !ctx.Read(request) || !ctx.Read(low) || !ctx.Read(high) || !ctx.Read(slot) || !ctx.Read(prospectiveType))
        {
            return;
        }
        array<ref BDE_EMU_LoadoutOffer> batch = new array<ref BDE_EMU_LoadoutOffer>;
        PlayerBase player;
        EntityAI parent;
        ExpansionMarketTraderZone zone;
        int validationSlot = slot;
        if (prospectiveType != "")
        {
            validationSlot = InventorySlots.GetSlotIdFromString("Shoulder");
            if (!GetGame().ConfigIsExisting(BDE_EMU_LoadoutUtil.ConfigPath(prospectiveType)))
            {
                return;
            }
        }
        if (!BDE_LM_RateLimit(senderRPC, "offers", 150) || !BDE_LM_Resolve(senderRPC, target, low, high, validationSlot, player, parent, zone))
        {
            BDE_LM_SendOffers(senderRPC, request, batch, true, "Station or equipment unavailable. Select the slot again.");
            return;
        }
        set<ExpansionTraderObjectBase> registry = ExpansionTraderObjectBase.GetAll();
        if (!registry)
        {
            BDE_LM_SendOffers(senderRPC, request, batch, true, "Trader registry is unavailable. Please retry shortly.");
            return;
        }
        for (int traderIndex = 0; traderIndex < registry.Count(); traderIndex++)
        {
            ExpansionTraderObjectBase trader = registry.Get(traderIndex);
            if (!trader || !trader.GetTraderEntity() || trader.GetTraderZone() != zone || !trader.GetTraderMarket())
            {
                continue;
            }
            ExpansionMarketTrader market = trader.GetTraderMarket();
            EntityAI oldItem;
            if (prospectiveType == "")
            {
                oldItem = parent.GetInventory().FindAttachment(slot);
            }
            ExpansionMarketSell sale = BDE_LM_ReplacementSale(player, oldItem, trader);
            BDE_EMU_LoadoutOffer empty = new BDE_EMU_LoadoutOffer;
            empty.Kind = 1;
            empty.TraderName = Widget.TranslateString(market.DisplayName);
            trader.GetTraderEntity().GetNetworkID(empty.TraderLow, empty.TraderHigh);
            empty.CanBuy = true;
            BDE_LM_SetReplacementQuote(empty, trader, oldItem, sale);
            batch.Insert(empty);
            foreach (string className, ExpansionMarketTraderBuySell mode: market.Items)
            {
                ExpansionMarketItem item = ExpansionMarketCategory.GetGlobalItem(className, false);
                if (!item || !item.Category || !BDE_LM_AllowedListing(market, zone, className, slot) || item.MaxStockThreshold == 0)
                {
                    continue;
                }
                string offeredType = className;
                #ifdef BDE_UnderSlungLaunchers
                string standaloneType = BDE_LM_StandaloneWeapon(className);
                if (standaloneType != "" && BDE_EMU_LoadoutUtil.FitsSlot(standaloneType, slot))
                {
                    offeredType = standaloneType;
                }
                #endif
                if (!BDE_EMU_LoadoutUtil.FitsSlot(offeredType, slot))
                {
                    continue;
                }
                BDE_EMU_LoadoutOffer quoted = BDE_LM_Quote(player, trader, item);
                quoted.ClassName = offeredType;
                BDE_LM_SetReplacementQuote(quoted, trader, oldItem, sale);
                batch.Insert(quoted);
                if (batch.Count() >= 24)
                {
                    BDE_LM_SendOffers(senderRPC, request, batch, false, "");
                    batch.Clear();
                }
            }
        }
        BDE_LM_SendOffers(senderRPC, request, batch, true, "");
    }

    protected void BDE_LM_SendOffers(PlayerIdentity identity, int request, array<ref BDE_EMU_LoadoutOffer> batch, bool done, string message)
    {
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMOffersReply");
        rpc.Write(request);
        rpc.Write(batch);
        rpc.Write(done);
        rpc.Write(message);
        rpc.Expansion_Send(null, true, identity);
    }

    private void RPC_BDE_LMOffersReply(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        int request;
        array<ref BDE_EMU_LoadoutOffer> batch;
        bool done;
        string message;
        if (!ctx.Read(request) || !ctx.Read(batch) || !ctx.Read(done) || !ctx.Read(message) || request != m_BDELMClientRequest || !batch)
        {
            return;
        }
        foreach (BDE_EMU_LoadoutOffer offer: batch)
        {
            BDE_LM_ClientOffers.Insert(offer);
        }
        if (done)
        {
            SI_BDELoadoutOffers.Invoke(request, message);
        }
    }

    void BDE_LM_Buy(Object station, EntityAI parent, int slot, BDE_EMU_LoadoutOffer offer)
    {
        int low;
        int high;
        parent.GetNetworkID(low, high);
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMBuy");
        rpc.Write(low);
        rpc.Write(high);
        rpc.Write(slot);
        rpc.Write(offer.TraderLow);
        rpc.Write(offer.TraderHigh);
        rpc.Write(offer.ItemID);
        rpc.Write(offer.Price);
        rpc.Write(offer.SalePrice);
        rpc.Write(offer.ReplacedLow);
        rpc.Write(offer.ReplacedHigh);
        rpc.Expansion_Send(station, true);
    }

    private void RPC_BDE_LMBuy(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        int low;
        int high;
        int slot;
        int traderLow;
        int traderHigh;
        int itemID;
        int agreedPrice;
        int agreedSale;
        int replacedLow;
        int replacedHigh;
        if (!senderRPC || !ctx.Read(low) || !ctx.Read(high) || !ctx.Read(slot) || !ctx.Read(traderLow) || !ctx.Read(traderHigh) || !ctx.Read(itemID) || !ctx.Read(agreedPrice) || !ctx.Read(agreedSale) || !ctx.Read(replacedLow) || !ctx.Read(replacedHigh))
        {
            return;
        }
        PlayerBase player;
        EntityAI parent;
        ExpansionMarketTraderZone zone;
        if (!BDE_LM_Resolve(senderRPC, target, low, high, slot, player, parent, zone))
        {
            BDE_LM_Result(senderRPC, false, "Station or equipment no longer available.");
            return;
        }
        if (!BDE_LM_RateLimit(senderRPC, "buy", 500))
        {
            BDE_LM_Result(senderRPC, false, "Please wait before buying again.");
            return;
        }
        EntityAI oldItem = parent.GetInventory().FindAttachment(slot);
        int currentLow;
        int currentHigh;
        if (oldItem)
        {
            oldItem.GetNetworkID(currentLow, currentHigh);
        }
        if (currentLow != replacedLow || currentHigh != replacedHigh)
        {
            BDE_LM_Result(senderRPC, false, "Your equipment changed. Refresh and review the replacement quote.");
            return;
        }
        ExpansionTraderObjectBase trader = GetTraderFromObject(GetGame().GetObjectByNetworkId(traderLow, traderHigh), false);
        ExpansionMarketItem item = ExpansionMarketCategory.GetGlobalItem(itemID);
        if (!trader || !trader.GetTraderMarket() || trader.GetTraderZone() != zone || !item || !item.Category || item.MaxStockThreshold == 0 || !BDE_LM_AllowedListing(trader.GetTraderMarket(), zone, item.ClassName, slot) || !trader.GetTraderMarket().ItemExists(item.ClassName) || !trader.GetTraderMarket().CanBuyItem(item.ClassName) || !BDE_EMU_LoadoutUtil.FitsSlot(item.ClassName, slot))
        {
            BDE_LM_Result(senderRPC, false, "This offer is no longer available at this station.");
            return;
        }
        if (BDE_LM_StartingAmmoCount(item) > 0)
        {
            BDE_LM_Result(senderRPC, false, "Use Buy & Equip to purchase this magazine with its ammunition.");
            return;
        }
        BDE_EMU_LoadoutOffer quote = BDE_LM_Quote(player, trader, item);
        if (!quote.CanBuy || !CheckCanUseTrader(player, trader))
        {
            BDE_LM_Result(senderRPC, false, "Purchase blocked by stock or trader requirements. Refresh offers.");
            return;
        }
        ExpansionMarketSell sale = BDE_LM_ReplacementSale(player, oldItem, trader);
        BDE_LM_SetReplacementQuote(quote, trader, oldItem, sale);
        if (quote.Price != agreedPrice || quote.SalePrice != agreedSale)
        {
            BDE_LM_Result(senderRPC, false, "The price changed. Refresh and review the new price.");
            return;
        }
        if (sale && !CheckCanUseTrader(player, sale.Trader))
        {
            BDE_LM_Result(senderRPC, false, "The trade-in trader is no longer accessible. Refresh offers.");
            return;
        }
        ExpansionMarketTrader market = trader.GetTraderMarket();
        array<int> monies;
        if (quote.NetPrice > 0 && !FindMoneyAndCountTypes(player, quote.NetPrice, monies, false, null, market))
        {
            BDE_LM_Result(senderRPC, false, "Not enough money in this trader's accepted currencies.");
            return;
        }
        array<ref InventoryLocation> moved = new array<ref InventoryLocation>;
        ExpansionTemporaryOwnedContainer crate;
        if (oldItem)
        {
            crate = BDE_LM_GetCrate(player);
            if (!crate)
            {
                BDE_LM_Result(senderRPC, false, "Cannot create your temporary crate. Nothing was charged.");
                return;
            }
            crate.ExpansionSetCanReceiveItems(true);
            bool preserved = true;
            if (sale)
            {
                preserved = BDE_LM_KeepContents(oldItem, crate, moved);
            }
            if (preserved)
            {
                preserved = BDE_LM_KeepItem(oldItem, crate, moved);
            }
            crate.ExpansionSetCanReceiveItems(false);
            if (!preserved)
            {
                BDE_LM_Restore(moved);
                BDE_LM_Result(senderRPC, false, "Cannot safely move your old equipment. Nothing charged; check your equipment and temporary crate.");
                return;
            }
        }
        // Validate script-level compatibility before creation as well. Some
        // engine creation paths are more permissive than inventory movement.
        EntityAI probe = EntityAI.Cast(GetGame().CreateObjectEx(item.ClassName, vector.Zero, ECE_LOCAL | ECE_NOLIFETIME));
        bool compatible = probe && parent.GetInventory().CanAddAttachmentEx(probe, slot);
        if (probe)
        {
            GetGame().ObjectDelete(probe);
        }
        if (!compatible)
        {
            BDE_LM_Restore(moved);
            BDE_LM_Result(senderRPC, false, "This item is incompatible with your current equipment. Nothing was charged.");
            return;
        }
        // No yields between quote, creation, payment and stock commit.
        EntityAI purchased = parent.GetInventory().CreateAttachmentEx(item.ClassName, slot);
        InventoryLocation location = new InventoryLocation;
        if (!purchased || !purchased.GetInventory().GetCurrentInventoryLocation(location) || location.GetParent() != parent || location.GetSlot() != slot)
        {
            if (purchased)
            {
                GetGame().ObjectDelete(purchased);
            }
            BDE_LM_Restore(moved);
            BDE_LM_Result(senderRPC, false, "This item cannot attach to that slot. Nothing was charged.");
            return;
        }
        BDE_ApplyPurchasedAttachmentQuantity(purchased, item.QuantityPercent);
        if (quote.NetPrice > 0 && !RemoveMoney(quote.NetPrice, player, market.Currencies))
        {
            GetGame().ObjectDelete(purchased);
            BDE_LM_Restore(moved);
            BDE_LM_Result(senderRPC, false, "Payment failed; the new attachment was removed.");
            return;
        }
        if (sale)
        {
            foreach (ExpansionMarketSellItem sold: sale.Sell)
            {
                sold.DestroyItem();
                zone.AddStock(sold.ClassName, sold.AddStockAmount);
            }
        }
        if (quote.NetPrice < 0)
        {
            EntityAI moneyParent = player;
            SpawnMoney(player, moneyParent, -quote.NetPrice, true, item, market);
            CheckSpawn(player, moneyParent);
        }
        zone.RemoveStock(item.ClassName, 1, false);
        zone.Save();
        Print("[BDE Loadout] Purchase player=" + senderRPC.GetId() + " item=" + item.ClassName + " trader=" + market.m_FileName + " price=" + quote.Price.ToString());
        string message = "Purchased and equipped. Select its slots to add attachments.";
        if (oldItem)
        {
            message = "Equipped. Recover retained equipment from your temporary crate within 20 minutes.";
        }
        BDE_LM_Result(senderRPC, true, message);
    }

    protected void BDE_LM_Result(PlayerIdentity identity, bool success, string message)
    {
        if (success)
        {
            ExpansionNotification("Loadout Manager", message).Success(identity);
        }
        else
        {
            ExpansionNotification("Loadout Manager", message).Error(identity);
        }
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMResult");
        rpc.Write(success);
        rpc.Write(message);
        rpc.Expansion_Send(null, true, identity);
    }

    private void RPC_BDE_LMResult(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        bool success;
        string message;
        if (ctx.Read(success) && ctx.Read(message))
        {
            SI_BDELoadoutResult.Invoke(success, message);
        }
    }


    protected bool BDE_LM_SameCurrencies(ExpansionMarketTrader first, ExpansionMarketTrader second)
    {
        if (first.Currencies.Count() != second.Currencies.Count())
        {
            return false;
        }
        foreach (string currency: first.Currencies)
        {
            if (second.Currencies.Find(currency) == -1)
            {
                return false;
            }
        }
        return true;
    }

    protected ExpansionMarketSell BDE_LM_ReplacementSale(PlayerBase player, EntityAI oldItem, ExpansionTraderObjectBase purchaseTrader)
    {
        if (!oldItem || oldItem.IsPendingDeletion() || !oldItem.GetInventory().CanRemoveEntity())
        {
            return null;
        }
#ifdef SERVER
        if (GetExpansionSettings().GetMarket().DisallowUnpersisted && !ExpansionWorld.IsStoreLoaded(oldItem) && !ExpansionWorld.IsStoreSaved(oldItem))
        {
            return null;
        }
#endif
        ExpansionMarketSell best;
        BDE_EMU_LoadoutQuoteContext.Item = oldItem;
        int amount = GetItemAmount(oldItem);
        BDE_EMU_LoadoutQuoteContext.Item = null;
        if (amount <= 0)
        {
            return null;
        }
        array<EntityAI> candidates = {oldItem};
        string type = oldItem.GetType();
        type.ToLower();
        set<ExpansionTraderObjectBase> registry = ExpansionTraderObjectBase.GetAll();
        if (!registry)
        {
            return null;
        }
        for (int traderIndex = 0; traderIndex < registry.Count(); traderIndex++)
        {
            ExpansionTraderObjectBase seller = registry.Get(traderIndex);
            if (!seller || !seller.GetTraderEntity() || seller.GetTraderZone() != purchaseTrader.GetTraderZone() || !seller.GetTraderMarket())
            {
                continue;
            }
            ExpansionMarketTrader market = seller.GetTraderMarket();
            if (!BDE_LM_SameCurrencies(market, purchaseTrader.GetTraderMarket()))
            {
                continue;
            }
            // The server's lower-RP sell access and 25% penalty still apply.
            string access = BDE_LM_Access(player, market, true);
            if (access != "")
            {
                continue;
            }
            string className = GetMarketItemClassName(market, type);
            ExpansionMarketItem item = ExpansionMarketCategory.GetGlobalItem(className, false);
            if (!item || !market.ItemExists(className) || !BDE_LM_ListedCategory(market, className, false) || !market.CanSellItem(className))
            {
                continue;
            }
            ExpansionMarketSell sale = new ExpansionMarketSell(player);
            sale.Trader = seller;
            sale.Item = item;
            ExpansionMarketResult result;
            BDE_EMU_LoadoutQuoteContext.Item = oldItem;
            bool quotedSale = FindSellPrice(player, candidates, seller.GetTraderZone().GetStock(className), amount, sale, false, result);
            BDE_EMU_LoadoutQuoteContext.Item = null;
            if (!quotedSale || result != ExpansionMarketResult.Success || sale.TotalAmount != amount || sale.Sell.Count() != 1)
            {
                continue;
            }
            if (sale.Price > 0 && (!best || sale.Price > best.Price))
            {
                best = sale;
            }
        }
        return best;
    }

    protected void BDE_LM_SetReplacementQuote(BDE_EMU_LoadoutOffer offer, ExpansionTraderObjectBase trader, EntityAI oldItem, ExpansionMarketSell sale)
    {
        offer.NetPrice = offer.Price;
        if (oldItem)
        {
            oldItem.GetNetworkID(offer.ReplacedLow, offer.ReplacedHigh);
            offer.ReplacementText = "Current item will be kept in your temporary crate.";
            if (sale)
            {
                offer.SalePrice = sale.Price;
                offer.NetPrice = offer.Price - sale.Price;
                offer.ReplacementText = "Trade-in: " + trader.GetDisplayPrice(sale.Price, false, true, true) + " at " + Widget.TranslateString(sale.Trader.GetTraderMarket().DisplayName) + ". Contents and attachments go to your temporary crate.";
            }
        }
        if (offer.NetPrice < 0)
        {
            offer.NetPriceText = "Receive " + trader.GetDisplayPrice(-offer.NetPrice, false, true, true);
        }
        else
        {
            offer.NetPriceText = trader.GetDisplayPrice(offer.NetPrice, false, true, true);
        }
    }

    protected ExpansionTemporaryOwnedContainer BDE_LM_GetCrate(PlayerBase player)
    {
        ExpansionTemporaryOwnedContainer crate = player.Expansion_GetTemporaryOwnedContainer();
        if (!crate)
        {
            crate = ExpansionTemporaryOwnedContainer.Cast(GetGame().CreateObjectEx("ExpansionTemporaryOwnedContainer", player.GetPosition(), ECE_KEEPHEIGHT));
            if (crate)
            {
                player.Expansion_SetTemporaryOwnedContainer(crate);
            }
        }
        return crate;
    }

    protected bool BDE_LM_KeepItem(EntityAI item, ExpansionTemporaryOwnedContainer crate, array<ref InventoryLocation> moved)
    {
        if (!item || !crate || !item.GetInventory())
        {
            Print("[BDE Loadout preserve] Missing item, inventory or crate.");
            return false;
        }
        InventoryLocation original = new InventoryLocation;
        InventoryLocation destination = new InventoryLocation;
        BDE_EMU_LoadoutInventoryAccess.Begin(item);
        bool preserved = item.GetInventory().GetCurrentInventoryLocation(original);
        string failure;
        if (!preserved)
        {
            failure = "source location unavailable";
        }
        else if (!item.GetInventory().CanRemoveEntity())
        {
            preserved = false;
            failure = "source removal blocked";
        }
        else if (!crate.GetInventory().FindFreeLocationFor(item, FindInventoryLocationType.CARGO, destination))
        {
            preserved = false;
            failure = "no valid crate cargo location";
        }
        else
        {
            preserved = crate.GetInventory().TakeToDst(InventoryMode.SERVER, original, destination);
            if (!preserved)
            {
                failure = "inventory move rejected";
            }
        }
        BDE_EMU_LoadoutInventoryAccess.End();
        if (!preserved)
        {
            Print("[BDE Loadout preserve] item=" + item.GetType() + " reason=" + failure + " sourceType=" + original.GetType().ToString() + " slot=" + original.GetSlot().ToString());
        }
        if (!preserved)
        {
            return false;
        }
        moved.Insert(original);
        return true;
    }

    protected bool BDE_LM_KeepContents(EntityAI item, ExpansionTemporaryOwnedContainer crate, array<ref InventoryLocation> moved)
    {
        // Move the actual objects, preserving their own cargo and nested attachments.
        for (int index = item.GetInventory().AttachmentCount() - 1; index >= 0; index--)
        {
            if (!BDE_LM_KeepItem(item.GetInventory().GetAttachmentFromIndex(index), crate, moved))
            {
                return false;
            }
        }
        CargoBase cargo = item.GetInventory().GetCargo();
        if (cargo)
        {
            for (int cargoIndex = cargo.GetItemCount() - 1; cargoIndex >= 0; cargoIndex--)
            {
                if (!BDE_LM_KeepItem(cargo.GetItem(cargoIndex), crate, moved))
                {
                    return false;
                }
            }
        }
        return true;
    }

    protected void BDE_LM_Restore(array<ref InventoryLocation> moved)
    {
        for (int index = moved.Count() - 1; index >= 0; index--)
        {
            EntityAI item = moved[index].GetItem();
            InventoryLocation current = new InventoryLocation;
            if (item && item.GetInventory().GetCurrentInventoryLocation(current))
            {
                if (!GameInventory.LocationSyncMoveEntity(current, moved[index]))
                {
                    // Never delete preserved gear if another script blocks restoration.
                    Print("[BDE Loadout] Restore blocked; retained safely in temporary crate: " + item.GetType());
                }
            }
        }
    }

    void BDE_LM_Checkout(Object station, array<ref BDE_EMU_LoadoutChange> changes, array<ref BDE_EMU_AmmoChange> ammunition)
    {
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMCheckout");
        rpc.Write(changes);
        rpc.Write(ammunition);
        rpc.Expansion_Send(station, true);
    }

    private void RPC_BDE_LMCheckout(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        array<ref BDE_EMU_LoadoutChange> changes;
        array<ref BDE_EMU_AmmoChange> ammunition;
        if (!senderRPC || !ctx.Read(changes) || !changes || !ctx.Read(ammunition) || !ammunition)
        {
            return;
        }
        if (changes.Count() + ammunition.Count() < 1 || changes.Count() + ammunition.Count() > 128)
        {
            BDE_LM_Result(senderRPC, false, "A checkout must contain between 1 and 128 slot changes.");
            return;
        }
        if (!BDE_LM_RateLimit(senderRPC, "buy", 500))
        {
            BDE_LM_Result(senderRPC, false, "Please wait before checking out again.");
            return;
        }
        PlayerBase player = PlayerBase.GetPlayerByUID(senderRPC.GetId());
        if (!player)
        {
            return;
        }
        int low;
        int high;
        player.GetNetworkID(low, high);
        EntityAI root;
        ExpansionMarketTraderZone zone;
        if (!BDE_LM_Resolve(senderRPC, target, low, high, InventorySlots.GetSlotIdFromString("Shoulder"), player, root, zone))
        {
            BDE_LM_Result(senderRPC, false, "Station or player is no longer available.");
            return;
        }
        BDE_LM_ReleasePlan(senderRPC.GetId());
        int unavailable;
        changes = BDE_LM_ResolveDraft(player, zone, changes, unavailable);
        ammunition = BDE_LM_NormalizeAmmo(player, zone, changes, ammunition, unavailable);
        array<ref BDE_EMU_LoadoutCommit> commits = new array<ref BDE_EMU_LoadoutCommit>;
        map<int, int> demanded = new map<int, int>;
        map<string, int> returnedStock = new map<string, int>;
        ExpansionMarketTrader currencyMarket;
        ExpansionMarketItem payoutItem;
        int total;
        int lastDepth;
        string failure;
        foreach (BDE_EMU_LoadoutChange change: changes)
        {
            if (!change || !change.Slots || !change.Offer || change.Slots.Count() < 1 || change.Slots.Count() > 12 || change.Slots.Count() < lastDepth)
            {
                failure = "Invalid loadout order. Reopen the manager.";
                break;
            }
            lastDepth = change.Slots.Count();
            bool prospective = false;
            foreach (BDE_EMU_LoadoutCommit prior: commits)
            {
                if (BDE_EMU_LoadoutPaths.Prefix(prior.Change.Slots, change.Slots))
                {
                    if (prior.Change.Slots.Count() == change.Slots.Count() || prior.Change.Offer.Kind != 0)
                    {
                        failure = "Conflicting slot changes. Review your loadout.";
                        break;
                    }
                    prospective = true;
                }
            }
            if (failure != "")
            {
                break;
            }
            BDE_EMU_LoadoutOffer offer = change.Offer;
            if (offer.Kind != 0 && offer.Kind != 1)
            {
                failure = "Invalid loadout operation.";
                break;
            }
            BDE_EMU_LoadoutCommit commit = new BDE_EMU_LoadoutCommit;
            commit.Change = change;
            commit.Trader = GetTraderFromObject(GetGame().GetObjectByNetworkId(offer.TraderLow, offer.TraderHigh), false);
            if (!commit.Trader || commit.Trader.GetTraderZone() != zone || !commit.Trader.GetTraderMarket())
            {
                failure = "A trader is no longer available.";
                break;
            }
            ExpansionMarketTrader market = commit.Trader.GetTraderMarket();
            if (!currencyMarket)
            {
                currencyMarket = market;
            }
            if (!BDE_LM_SameCurrencies(currencyMarket, market))
            {
                failure = "These traders use different currencies. Check out their items separately.";
                break;
            }
            EntityAI parent = BDE_EMU_LoadoutPaths.Resolve(player, change.Slots, change.Slots.Count() - 1);
            int slotId = change.Slots[change.Slots.Count() - 1];
            if (!prospective)
            {
                if (!BDE_EMU_LoadoutUtil.HasSlot(parent, slotId))
                {
                    failure = "Your equipment changed. Reopen the manager.";
                    break;
                }
                commit.OldItem = parent.GetInventory().FindAttachment(slotId);
            }
            int oldLow = 0;
            int oldHigh = 0;
            if (commit.OldItem)
            {
                commit.OldItem.GetNetworkID(oldLow, oldHigh);
            }
            if (oldLow != offer.ReplacedLow || oldHigh != offer.ReplacedHigh)
            {
                failure = "Your equipped items changed. Refresh the loadout.";
                break;
            }
            commit.Sale = BDE_LM_ReplacementSale(player, commit.OldItem, commit.Trader);
            int salePrice = 0;
            if (commit.Sale)
            {
                salePrice = commit.Sale.Price;
                payoutItem = commit.Sale.Item;
                foreach (ExpansionMarketSellItem saleItem: commit.Sale.Sell)
                {
                    int returned = returnedStock.Get(saleItem.ClassName) + saleItem.AddStockAmount;
                    returnedStock.Set(saleItem.ClassName, returned);
                    int existingStock = zone.GetStock(saleItem.ClassName);
                    if (existingStock >= 0 && commit.Sale.Item.MaxStockThreshold > 0 && existingStock + returned > commit.Sale.Item.MaxStockThreshold)
                    {
                        failure = "The trader cannot accept all these trade-ins at once.";
                        break;
                    }
                }
                if (failure != "")
                {
                    break;
                }
                if (!CheckCanUseTrader(player, commit.Sale.Trader))
                {
                    failure = "A trade-in trader is no longer accessible.";
                    break;
                }
            }
            if (salePrice != offer.SalePrice)
            {
                failure = "A trade-in price changed. Reselect the item to refresh its quote.";
                break;
            }
            int purchasePrice = 0;
            if (offer.Kind == 0)
            {
                commit.Item = ExpansionMarketCategory.GetGlobalItem(offer.ItemID);
                if (!commit.Item || !commit.Item.Category || commit.Item.MaxStockThreshold == 0 || !BDE_LM_AllowedListing(market, zone, commit.Item.ClassName, slotId) || !market.ItemExists(commit.Item.ClassName) || !market.CanBuyItem(commit.Item.ClassName) || !CF_String.EqualsIgnoreCase(BDE_LM_MarketType(offer.ClassName), commit.Item.ClassName) || !BDE_EMU_LoadoutUtil.FitsSlot(offer.ClassName, slotId))
                {
                    failure = "A purchase is no longer listed for this slot.";
                    break;
                }
                BDE_EMU_LoadoutOffer quote = BDE_LM_Quote(player, commit.Trader, commit.Item);
                int needed = demanded.Get(offer.ItemID) + 1;
                demanded.Set(offer.ItemID, needed);
                if (!quote.CanBuy || !CheckCanUseTrader(player, commit.Trader) || (quote.Stock >= 0 && needed > quote.Stock))
                {
                    failure = "Not enough stock or trader requirements are not met.";
                    break;
                }
                purchasePrice = quote.Price;
                payoutItem = commit.Item;
            }
            if (purchasePrice != offer.Price)
            {
                failure = "A purchase price changed. Reselect the item to refresh its quote.";
                break;
            }
            float nextEquipmentTotal = total;
            nextEquipmentTotal += purchasePrice;
            nextEquipmentTotal -= salePrice;
            if (nextEquipmentTotal > 2000000000 || nextEquipmentTotal < -2000000000)
            {
                failure = "The loadout exceeds the supported transaction value.";
                break;
            }
            total += purchasePrice - salePrice;
            commits.Insert(commit);
        }
        if (failure != "")
        {
            BDE_LM_Result(senderRPC, false, failure + " Nothing was charged.");
            return;
        }
        foreach (BDE_EMU_LoadoutCommit defaultCommit: commits)
        {
            if (!defaultCommit.Item || !GetGame().IsKindOf(defaultCommit.Item.ClassName, "Magazine_Base") || GetGame().IsKindOf(defaultCommit.Item.ClassName, "Ammunition_Base"))
            {
                continue;
            }
            bool explicitAmmo = false;
            foreach (BDE_EMU_AmmoChange stagedAmmo: ammunition)
            {
                if (stagedAmmo && stagedAmmo.Slots && stagedAmmo.Slots.Count() == defaultCommit.Change.Slots.Count() && BDE_EMU_LoadoutPaths.Prefix(stagedAmmo.Slots, defaultCommit.Change.Slots))
                {
                    explicitAmmo = true;
                    break;
                }
            }
            if (!explicitAmmo)
            {
                ref BDE_EMU_AmmoChange defaultAmmo = new BDE_EMU_AmmoChange;
                defaultAmmo.Slots.Copy(defaultCommit.Change.Slots);
                defaultAmmo.MagazineClassName = defaultCommit.Item.ClassName;
                defaultAmmo.Mode = 3;
                defaultAmmo.PurchaseItemID = defaultCommit.Item.ItemID;
                defaultAmmo.PurchaseTraderLow = defaultCommit.Change.Offer.TraderLow;
                defaultAmmo.PurchaseTraderHigh = defaultCommit.Change.Offer.TraderHigh;
                ammunition.Insert(defaultAmmo);
            }
        }
        ref array<ref BDE_EMU_AmmoCommit> ammoCommits = new array<ref BDE_EMU_AmmoCommit>;
        foreach (BDE_EMU_AmmoChange requestedAmmo: ammunition)
        {
            string ammoError;
            ref BDE_EMU_AmmoChange freshAmmo = BDE_LM_AmmoQuote(player, zone, requestedAmmo, ammoError);
            if (!freshAmmo || freshAmmo.Price != requestedAmmo.Price || freshAmmo.OriginalSignature != requestedAmmo.OriginalSignature)
            {
                BDE_LM_Result(senderRPC, false, "A magazine or ammunition quote changed. Reselect its ammo action. " + ammoError);
                return;
            }
            foreach (BDE_EMU_AmmoCommit previousAmmo: ammoCommits)
            {
                if (previousAmmo.Change.Slots.Count() == freshAmmo.Slots.Count() && BDE_EMU_LoadoutPaths.Prefix(previousAmmo.Change.Slots, freshAmmo.Slots))
                {
                    BDE_LM_Result(senderRPC, false, "Duplicate magazine operation. Nothing charged.");
                    return;
                }
            }
            bool replacedMagazine = false;
            bool replacementAncestor = false;
            foreach (BDE_EMU_LoadoutCommit equipmentCommit: commits)
            {
                if (BDE_EMU_LoadoutPaths.Prefix(equipmentCommit.Change.Slots, freshAmmo.Slots))
                {
                    replacementAncestor = true;
                    if (equipmentCommit.Change.Slots.Count() == freshAmmo.Slots.Count() && equipmentCommit.Item && equipmentCommit.Item.ItemID == freshAmmo.PurchaseItemID && equipmentCommit.Change.Offer.TraderLow == freshAmmo.PurchaseTraderLow && equipmentCommit.Change.Offer.TraderHigh == freshAmmo.PurchaseTraderHigh && CF_String.EqualsIgnoreCase(equipmentCommit.Item.ClassName, freshAmmo.MagazineClassName))
                    {
                        replacedMagazine = true;
                    }
                }
            }
            // Existing rounds cannot also be sold as part of a replaced parent.
            if ((freshAmmo.Low != 0 || freshAmmo.High != 0) && replacementAncestor)
            {
                BDE_LM_Result(senderRPC, false, "The magazine's parent is being replaced. Reapply its ammo action.");
                return;
            }
            if (freshAmmo.Low == 0 && freshAmmo.High == 0 && !replacedMagazine)
            {
                BDE_LM_Result(senderRPC, false, "A new magazine must be included in the loadout purchase.");
                return;
            }
            foreach (BDE_EMU_AmmoLine line: freshAmmo.Lines)
            {
                ExpansionTraderObjectBase ammoTrader = GetTraderFromObject(GetGame().GetObjectByNetworkId(line.TraderLow, line.TraderHigh), false);
                if (!ammoTrader || !ammoTrader.GetTraderMarket())
                {
                    BDE_LM_Result(senderRPC, false, "Ammunition trader unavailable.");
                    return;
                }
                if (!currencyMarket)
                {
                    currencyMarket = ammoTrader.GetTraderMarket();
                }
                if (!BDE_LM_SameCurrencies(currencyMarket, ammoTrader.GetTraderMarket()))
                {
                    BDE_LM_Result(senderRPC, false, "Ammunition and equipment use different currencies.");
                    return;
                }
                payoutItem = ExpansionMarketCategory.GetGlobalItem(line.ItemID);
                if (freshAmmo.Mode != 2)
                {
                    int ammoDemand = demanded.Get(line.ItemID) + line.Count;
                    demanded.Set(line.ItemID, ammoDemand);
                    if (line.Stock >= 0 && ammoDemand > line.Stock)
                    {
                        BDE_LM_Result(senderRPC, false, "Not enough ammunition stock for the full loadout.");
                        return;
                    }
                }
                else
                {
                    int ammoReturned = returnedStock.Get(line.ClassName) + line.Count;
                    returnedStock.Set(line.ClassName, ammoReturned);
                    if (payoutItem && !payoutItem.IsStaticStock() && zone.GetStock(line.ClassName) + ammoReturned > payoutItem.MaxStockThreshold)
                    {
                        BDE_LM_Result(senderRPC, false, "The trader cannot accept all these rounds.");
                        return;
                    }
                }
            }
            ref BDE_EMU_AmmoCommit ammoCommit = new BDE_EMU_AmmoCommit;
            ammoCommit.Change = freshAmmo;
            ammoCommits.Insert(ammoCommit);
            float nextAmmoTotal = total;
            nextAmmoTotal += freshAmmo.Price;
            if (nextAmmoTotal > 2000000000 || nextAmmoTotal < -2000000000)
            {
                BDE_LM_Result(senderRPC, false, "The loadout exceeds the supported transaction value.");
                return;
            }
            total += freshAmmo.Price;
        }
        ref BDE_EMU_CheckoutPlan plan = new BDE_EMU_CheckoutPlan;
        plan.Token = ++m_BDELMToken;
        plan.Expires = GetGame().GetTime() + 10000;
        plan.Station = target;
        plan.Buyer = player;
        plan.Zone = zone;
        plan.Currency = currencyMarket;
        plan.Payout = payoutItem;
        plan.Total = total;
        plan.Equipment = commits;
        plan.Ammunition = ammoCommits;
        plan.Fingerprint = BDE_LM_Fingerprint(player);
        plan.Hold = new ExpansionMarketReserve;
        foreach (int demandID, int demandAmount: demanded)
        {
            ExpansionMarketItem demandItem = ExpansionMarketCategory.GetGlobalItem(demandID);
            if (demandItem && !demandItem.IsStaticStock())
            {
                plan.Hold.AddReservedEx(zone, demandItem, demandAmount, 0);
            }
        }
        if (unavailable > 0)
        {
            m_BDELMPlans.Set(senderRPC.GetId(), plan);
            GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BDE_LM_PlanTick, 250, false, senderRPC.GetId(), plan.Token);
            auto reply = Expansion_CreateRPC("RPC_BDE_LMPrepared");
            reply.Write(plan.Token);
            reply.Write(unavailable);
            reply.Write(total);
            reply.Write(changes);
            reply.Write(ammunition);
            reply.Expansion_Send(null, true, senderRPC);
            ExpansionNotification("Loadout Manager", unavailable.ToString() + " slot items are not available. Press Confirm Purchase to confirm purchase.").Error(senderRPC);
            return;
        }
        plan.Hold.ClearReserved(zone);
        BDE_LM_ExecutePlan(senderRPC, plan);
    }

    protected void BDE_LM_ExecutePlan(PlayerIdentity senderRPC, BDE_EMU_CheckoutPlan plan)
    {
        PlayerBase player = plan.Buyer;
        ExpansionMarketTraderZone zone = plan.Zone;
        ExpansionMarketTrader currencyMarket = plan.Currency;
        ExpansionMarketItem payoutItem = plan.Payout;
        int total = plan.Total;
        string failure;
        array<ref BDE_EMU_LoadoutCommit> commits = plan.Equipment;
        array<ref BDE_EMU_AmmoCommit> ammoCommits = plan.Ammunition;
        array<int> monies;
        if (total > 0 && !FindMoneyAndCountTypes(player, total, monies, false, null, currencyMarket))
        {
            BDE_LM_Result(senderRPC, false, "Not enough money for the complete loadout.");
            return;
        }
        array<ref InventoryLocation> moved = new array<ref InventoryLocation>;
        ExpansionTemporaryOwnedContainer crate = BDE_LM_GetCrate(player);
        if (!crate)
        {
            BDE_LM_Result(senderRPC, false, "Cannot create a temporary crate. Nothing was charged.");
            return;
        }
        crate.ExpansionSetCanReceiveItems(true);
        foreach (BDE_EMU_LoadoutCommit moveCommit: commits)
        {
            if (moveCommit.OldItem)
            {
                if ((moveCommit.Sale && !BDE_LM_KeepContents(moveCommit.OldItem, crate, moved)) || !BDE_LM_KeepItem(moveCommit.OldItem, crate, moved))
                {
                    failure = "Could not safely preserve your equipment.";
                    break;
                }
            }
        }
        crate.ExpansionSetCanReceiveItems(false);
        if (failure == "")
        {
            ref array<ref BDE_EMU_LoadoutCommit> pendingEquipment = new array<ref BDE_EMU_LoadoutCommit>;
            foreach (BDE_EMU_LoadoutCommit pendingCommit: commits)
            {
                if (pendingCommit.Item)
                {
                    pendingEquipment.Insert(pendingCommit);
                }
            }
            for (int equipPass = 0; equipPass <= commits.Count() && pendingEquipment.Count() > 0; equipPass++)
            {
                int equippedCount = 0;
                for (int equipIndex = 0; equipIndex < pendingEquipment.Count();)
                {
                    BDE_EMU_LoadoutCommit createCommit = pendingEquipment[equipIndex];
                    array<int> path = createCommit.Change.Slots;
                    EntityAI destination = BDE_EMU_LoadoutPaths.Resolve(player, path, path.Count() - 1);
                    int destinationSlot = path[path.Count() - 1];
                    if (!destination || !destination.GetInventory() || destination.IsPendingDeletion())
                    {
                        equipIndex++;
                        continue;
                    }
                    if (!createCommit.Purchased)
                    {
                        // Keep native creation separate from member assignment and nested arguments.
                        BDE_EMU_LoadoutChange purchaseChange = createCommit.Change;
                        BDE_EMU_LoadoutOffer purchaseOffer = purchaseChange.Offer;
                        string purchaseClass = purchaseOffer.ClassName;
                        vector purchasePosition = player.GetPosition();
                        bool configured = GetGame().ConfigIsExisting("CfgVehicles " + purchaseClass);
                        if (!configured)
                        {
                            configured = GetGame().ConfigIsExisting("CfgWeapons " + purchaseClass);
                        }
                        if (!configured)
                        {
                            configured = GetGame().ConfigIsExisting("CfgMagazines " + purchaseClass);
                        }
                        if (purchaseClass == "" || !configured)
                        {
                            Print("[BDE Loadout create] Invalid purchase class=" + purchaseClass);
                            failure = "A purchased item class is unavailable.";
                            break;
                        }
                        Print("[BDE Loadout create] Begin class=" + purchaseClass + " slot=" + destinationSlot.ToString());
                        Object createdObject = GetGame().CreateObjectEx(purchaseClass, purchasePosition, ECE_NOLIFETIME);
                        EntityAI createdEntity = EntityAI.Cast(createdObject);
                        createCommit.Purchased = createdEntity;
                        if (createdEntity)
                        {
                            Print("[BDE Loadout create] Created class=" + purchaseClass);
                        }
                    }
                    if (!createCommit.Purchased)
                    {
                        failure = "An item could not be created.";
                        break;
                    }
                    BDE_EMU_LoadoutInventoryAccess.Begin(destination);
                    bool canEquip = createCommit.Purchased.CanPutAsAttachment(destination) && destination.CanReceiveAttachment(createCommit.Purchased, destinationSlot) && destination.GetInventory().CanAddAttachmentEx(createCommit.Purchased, destinationSlot);
                    bool equipped = false;
                    if (canEquip && !destination.GetInventory().FindAttachment(destinationSlot))
                    {
                        InventoryLocation purchaseSource = new InventoryLocation;
                        InventoryLocation purchaseDestination = new InventoryLocation;
                        purchaseDestination.SetAttachment(destination, createCommit.Purchased, destinationSlot);
                        equipped = createCommit.Purchased.GetInventory().GetCurrentInventoryLocation(purchaseSource) && destination.GetInventory().TakeToDst(InventoryMode.SERVER, purchaseSource, purchaseDestination);
                    }
                    BDE_EMU_LoadoutInventoryAccess.End();
                    if (!equipped)
                    {
                        equipIndex++;
                        continue;
                    }
                    InventoryLocation createdLocation = new InventoryLocation;
                    if (!createCommit.Purchased.GetInventory().GetCurrentInventoryLocation(createdLocation) || createdLocation.GetParent() != destination || createdLocation.GetSlot() != destinationSlot)
                    {
                        failure = "An item could not be equipped.";
                        break;
                    }
                    BDE_ApplyPurchasedAttachmentQuantity(createCommit.Purchased, createCommit.Item.QuantityPercent);
                    MagazineStorage purchasedMagazine = MagazineStorage.Cast(createCommit.Purchased);
                    if (purchasedMagazine)
                    {
                        purchasedMagazine.ServerSetAmmoCount(0);
                    }
                    pendingEquipment.Remove(equipIndex);
                    equippedCount++;
                }
                if (failure != "" || equippedCount == 0)
                {
                    break;
                }
            }
            if (failure == "" && pendingEquipment.Count() > 0)
            {
                failure = "An item does not fit the proposed loadout or requires a missing attachment.";
            }
        }
        if (failure == "")
        {
            foreach (BDE_EMU_AmmoCommit applyAmmo: ammoCommits)
            {
                applyAmmo.MagazineEntity = Magazine.Cast(BDE_EMU_LoadoutPaths.Resolve(player, applyAmmo.Change.Slots, applyAmmo.Change.Slots.Count()));
                if (!applyAmmo.MagazineEntity || !CF_String.EqualsIgnoreCase(applyAmmo.MagazineEntity.GetType(), applyAmmo.Change.MagazineClassName))
                {
                    failure = "The magazine could not be equipped for its ammo operation.";
                    break;
                }
                for (int cartridgeIndex = 0; cartridgeIndex < applyAmmo.MagazineEntity.GetAmmoCount(); cartridgeIndex++)
                {
                    float cartridgeDamage;
                    string cartridgeType;
                    applyAmmo.MagazineEntity.GetCartridgeAtIndex(cartridgeIndex, cartridgeDamage, cartridgeType);
                    applyAmmo.Bullets.Insert(cartridgeType);
                    applyAmmo.Damage.Insert(cartridgeDamage);
                }
                applyAmmo.SnapshotTaken = true;
                if (applyAmmo.Change.Mode == 2)
                {
                    applyAmmo.MagazineEntity.ServerSetAmmoCount(0);
                }
                else
                {
                    foreach (BDE_EMU_AmmoLine fillLine: applyAmmo.Change.Lines)
                    {
                        for (int roundIndex = 0; roundIndex < fillLine.Count; roundIndex++)
                        {
                            if (!applyAmmo.MagazineEntity.ServerStoreCartridge(0, fillLine.Bullet))
                            {
                                failure = "The magazine rejected an ammunition round.";
                                break;
                            }
                        }
                    }
                }
                applyAmmo.MagazineEntity.SetSynchDirty();
                if (failure != "")
                {
                    break;
                }
            }
        }
        if (failure == "" && total > 0 && !RemoveMoney(total, player, currencyMarket.Currencies))
        {
            failure = "Payment failed.";
        }
        if (failure != "")
        {
            BDE_LM_RestoreAmmo(ammoCommits);
            for (int rollback = commits.Count() - 1; rollback >= 0; rollback--)
            {
                if (commits[rollback].Purchased)
                {
                    GetGame().ObjectDelete(commits[rollback].Purchased);
                }
            }
            BDE_LM_Restore(moved);
            BDE_LM_Result(senderRPC, false, failure + " Nothing charged; retained gear is in your inventory or temporary crate.");
            return;
        }
        foreach (BDE_EMU_LoadoutCommit finalCommit: commits)
        {
            if (finalCommit.Sale)
            {
                foreach (ExpansionMarketSellItem sold: finalCommit.Sale.Sell)
                {
                    sold.DestroyItem();
                    zone.AddStock(sold.ClassName, sold.AddStockAmount);
                }
            }
            if (finalCommit.Item)
            {
                zone.RemoveStock(finalCommit.Item.ClassName, 1, false);
            }
        }
        foreach (BDE_EMU_AmmoCommit finalAmmo: ammoCommits)
        {
            foreach (BDE_EMU_AmmoLine stockLine: finalAmmo.Change.Lines)
            {
                if (finalAmmo.Change.Mode != 2)
                {
                    zone.RemoveStock(stockLine.ClassName, stockLine.Count, false);
                }
                else
                {
                    zone.AddStock(stockLine.ClassName, stockLine.Count);
                }
            }
        }
        if (total < 0)
        {
            EntityAI moneyParent = player;
            SpawnMoney(player, moneyParent, -total, true, payoutItem, currencyMarket);
            CheckSpawn(player, moneyParent);
        }
        zone.Save();
        BDE_LM_Result(senderRPC, true, "Loadout equipped. Recover retained items and contents from your temporary crate within 20 minutes.");
    }

    protected string BDE_LM_Fingerprint(PlayerBase player)
    {
        array<EntityAI> items = new array<EntityAI>;
        player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);
        string result;
        foreach (EntityAI item: items)
        {
            int low;
            int high;
            item.GetNetworkID(low, high);
            InventoryLocation location = new InventoryLocation;
            item.GetInventory().GetCurrentInventoryLocation(location);
            result += low.ToString() + ":" + high.ToString() + ":" + location.GetSlot().ToString() + ":" + item.GetHealth().ToString() + ";";
            EntityAI locationParent = location.GetParent();
            if (locationParent)
            {
                int parentLow;
                int parentHigh;
                locationParent.GetNetworkID(parentLow, parentHigh);
                result += parentLow.ToString() + ":" + parentHigh.ToString();
            }
            result += ":" + location.GetType().ToString() + ":" + location.GetIdx().ToString() + ":" + location.GetRow().ToString() + ":" + location.GetCol().ToString();
            ItemBase inventoryItem = ItemBase.Cast(item);
            if (inventoryItem)
            {
                result += inventoryItem.GetQuantity().ToString();
            }
            Magazine magazine = Magazine.Cast(item);
            if (magazine)
            {
                result += BDE_EMU_AmmoUtil.Signature(magazine);
            }
        }
        return result;
    }

    void BDE_LM_ReleasePlan(string uid)
    {
        ref BDE_EMU_CheckoutPlan plan = m_BDELMPlans.Get(uid);
        if (plan)
        {
            plan.Hold.ClearReserved(plan.Zone);
            m_BDELMPlans.Remove(uid);
        }
    }

    void BDE_LM_PlanTick(string uid, int token)
    {
        ref BDE_EMU_CheckoutPlan plan = m_BDELMPlans.Get(uid);
        if (!plan || plan.Token != token)
        {
            return;
        }
        PlayerBase player = PlayerBase.GetPlayerByUID(uid);
        if (!player || !player.GetIdentity() || !player.IsAlive() || GetGame().GetTime() >= plan.Expires || !plan.Station || vector.Distance(player.GetPosition(), plan.Station.GetPosition()) > 5)
        {
            BDE_LM_ReleasePlan(uid);
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BDE_LM_PlanTick, 250, false, uid, token);
    }

    void BDE_LM_Confirm(Object station, int token)
    {
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMConfirm");
        rpc.Write(token);
        rpc.Expansion_Send(station, true);
    }

    void BDE_LM_Cancel(Object station, int token)
    {
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMCancel");
        rpc.Write(token);
        rpc.Expansion_Send(station, true);
    }

    private void RPC_BDE_LMCancel(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        int token;
        if (!senderRPC || !ctx.Read(token))
        {
            return;
        }
        BDE_EMU_CheckoutPlan plan = m_BDELMPlans.Get(senderRPC.GetId());
        if (plan && plan.Token == token && plan.Station == target)
        {
            BDE_LM_ReleasePlan(senderRPC.GetId());
        }
    }

    private void RPC_BDE_LMConfirm(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        int token;
        if (!senderRPC || !ctx.Read(token))
        {
            return;
        }
        ref BDE_EMU_CheckoutPlan plan = m_BDELMPlans.Get(senderRPC.GetId());
        PlayerBase player = PlayerBase.GetPlayerByUID(senderRPC.GetId());
        if (!plan || token != plan.Token || target != plan.Station)
        {
            BDE_LM_Result(senderRPC, false, "Confirmation expired. Press Buy & Equip again.");
            return;
        }
        bool valid = player && player == plan.Buyer && player.IsAlive() && GetGame().GetTime() < plan.Expires && target && vector.Distance(player.GetPosition(), target.GetPosition()) <= 5;
        if (valid)
        {
            valid = BDE_LM_Fingerprint(player) == plan.Fingerprint;
        }
        BDE_LM_ReleasePlan(senderRPC.GetId());
        if (!valid)
        {
            BDE_LM_Result(senderRPC, false, "Equipment or station changed, or confirmation expired. Review and retry.");
            return;
        }
        // The held stock and quoted plan are authoritative; do not price or
        // check availability again. Payment and physical equip remain atomic.
        BDE_LM_ExecutePlan(senderRPC, plan);
    }

    private void RPC_BDE_LMPrepared(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        int token;
        int missing;
        int total;
        array<ref BDE_EMU_LoadoutChange> changes;
        array<ref BDE_EMU_AmmoChange> ammunition;
        if (ctx.Read(token) && ctx.Read(missing) && ctx.Read(total) && ctx.Read(changes) && ctx.Read(ammunition))
        {
            SI_BDELoadoutPrepared.Invoke(token, missing, total, changes, ammunition);
        }
    }

    protected array<ref BDE_EMU_AmmoChange> BDE_LM_NormalizeAmmo(PlayerBase player, ExpansionMarketTraderZone zone, array<ref BDE_EMU_LoadoutChange> changes, array<ref BDE_EMU_AmmoChange> requested, inout int unavailable)
    {
        ref array<ref BDE_EMU_AmmoChange> result = new array<ref BDE_EMU_AmmoChange>;
        map<int, int> usage = new map<int, int>;
        foreach (BDE_EMU_LoadoutChange equipment: changes)
        {
            if (equipment.Offer.Kind == 0)
            {
                usage.Set(equipment.Offer.ItemID, usage.Get(equipment.Offer.ItemID) + 1);
                if (equipment.Offer.StartingAmmo)
                {
                    BDE_EMU_AmmoLine initial = equipment.Offer.StartingAmmo;
                    usage.Set(initial.ItemID, usage.Get(initial.ItemID) + initial.Count);
                }
            }
        }
        foreach (BDE_EMU_AmmoChange request: requested)
        {
            if (!request || !request.Slots || request.Slots.Count() == 0 || request.Mode == 3)
            {
                continue;
            }
            bool removedParent = false;
            BDE_EMU_LoadoutOffer purchase = null;
            foreach (BDE_EMU_LoadoutChange change: changes)
            {
                if (BDE_EMU_LoadoutPaths.Prefix(change.Slots, request.Slots))
                {
                    if (change.Offer.Kind == 1)
                    {
                        removedParent = true;
                    }
                    if (change.Slots.Count() == request.Slots.Count() && change.Offer.Kind == 0)
                    {
                        purchase = change.Offer;
                    }
                }
            }
            if (removedParent)
            {
                continue;
            }
            if (purchase)
            {
                request.PurchaseItemID = purchase.ItemID;
                request.PurchaseTraderLow = purchase.TraderLow;
                request.PurchaseTraderHigh = purchase.TraderHigh;
            }
            string error;
            ref BDE_EMU_AmmoChange fresh = BDE_LM_AmmoQuote(player, zone, request, error);
            if (purchase && purchase.StartingAmmo)
            {
                usage.Set(purchase.StartingAmmo.ItemID, usage.Get(purchase.StartingAmmo.ItemID) - purchase.StartingAmmo.Count);
            }
            bool available = fresh != null;
            if (fresh && fresh.Mode != 2)
            {
                foreach (BDE_EMU_AmmoLine line: fresh.Lines)
                {
                    if (line.Stock >= 0 && usage.Get(line.ItemID) + line.Count > line.Stock)
                    {
                        available = false;
                    }
                }
            }
            if (!available)
            {
                unavailable++;
                if (purchase)
                {
                    request.Mode = 2;
                    fresh = BDE_LM_AmmoQuote(player, zone, request, error);
                }
                else
                {
                    fresh = null;
                }
            }
            if (fresh)
            {
                if (fresh.Mode != 2)
                {
                    foreach (BDE_EMU_AmmoLine accepted: fresh.Lines)
                    {
                        usage.Set(accepted.ItemID, usage.Get(accepted.ItemID) + accepted.Count);
                    }
                }
                result.Insert(fresh);
            }
        }
        return result;
    }

    protected array<ref BDE_EMU_LoadoutChange> BDE_LM_ResolveDraft(PlayerBase player, ExpansionMarketTraderZone zone, array<ref BDE_EMU_LoadoutChange> requested, out int unavailable)
    {
        ref array<ref BDE_EMU_LoadoutChange> resolved = new array<ref BDE_EMU_LoadoutChange>;
        ref array<ref BDE_EMU_LoadoutChange> removed = new array<ref BDE_EMU_LoadoutChange>;
        map<int, int> demand = new map<int, int>;
        set<ExpansionTraderObjectBase> registry = ExpansionTraderObjectBase.GetAll();
        if (!registry)
        {
            unavailable = requested.Count();
            return resolved;
        }
        foreach (BDE_EMU_LoadoutChange change: requested)
        {
            if (!change || !change.Offer || !change.Slots || change.Slots.Count() < 1 || change.Slots.Count() > 12)
            {
                unavailable++;
                continue;
            }
            bool blocked = false;
            foreach (BDE_EMU_LoadoutChange removedParent: removed)
            {
                if (BDE_EMU_LoadoutPaths.Prefix(removedParent.Slots, change.Slots))
                {
                    blocked = true;
                    break;
                }
            }
            if (blocked)
            {
                Print("[BDE Loadout checkout] Parent unavailable; skipped item=" + change.Offer.ClassName);
                unavailable++;
                continue;
            }
            bool prospective = false;
            foreach (BDE_EMU_LoadoutChange preceding: resolved)
            {
                if (preceding.Slots.Count() < change.Slots.Count() && BDE_EMU_LoadoutPaths.Prefix(preceding.Slots, change.Slots))
                {
                    prospective = true;
                }
            }
            EntityAI oldItem = null;
            if (!prospective)
            {
                oldItem = BDE_EMU_LoadoutPaths.Resolve(player, change.Slots, change.Slots.Count());
            }
            ref BDE_EMU_LoadoutOffer best;
            ExpansionTraderObjectBase selectedTrader;
            ExpansionTraderObjectBase fallbackTrader;
            for (int i = 0; i < registry.Count(); i++)
            {
                ExpansionTraderObjectBase trader = registry.Get(i);
                if (!trader || trader.GetTraderZone() != zone || !trader.GetTraderEntity() || !trader.GetTraderMarket())
                {
                    continue;
                }
                if (!fallbackTrader)
                {
                    fallbackTrader = trader;
                }
                if (change.Offer.Kind == 1)
                {
                    continue;
                }
                ExpansionMarketItem item = ExpansionMarketCategory.GetGlobalItem(BDE_LM_MarketType(change.Offer.ClassName), false);
                if (!item || item.MaxStockThreshold == 0 || !BDE_LM_AllowedListing(trader.GetTraderMarket(), zone, item.ClassName, change.Slots[change.Slots.Count() - 1]) || !BDE_EMU_LoadoutUtil.FitsSlot(change.Offer.ClassName, change.Slots[change.Slots.Count() - 1]))
                {
                    continue;
                }
                ref BDE_EMU_LoadoutOffer candidate = BDE_LM_Quote(player, trader, item);
                candidate.ClassName = change.Offer.ClassName;
                if (!candidate.CanBuy || !CheckCanUseTrader(player, trader) || (candidate.Stock >= 0 && demand.Get(candidate.ItemID) + 1 > candidate.Stock))
                {
                    continue;
                }
                if (candidate.StartingAmmo && candidate.StartingAmmo.Stock >= 0 && demand.Get(candidate.StartingAmmo.ItemID) + candidate.StartingAmmo.Count > candidate.StartingAmmo.Stock)
                {
                    continue;
                }
                if (!best || candidate.Price < best.Price)
                {
                    best = candidate;
                    selectedTrader = trader;
                }
            }
            if (!best)
            {
                if (change.Offer.Kind != 1)
                {
                    unavailable++;
                    Print("[BDE Loadout checkout] No eligible listing item=" + change.Offer.ClassName + " marketType=" + BDE_LM_MarketType(change.Offer.ClassName) + " slot=" + InventorySlots.GetSlotName(change.Slots[change.Slots.Count() - 1]));
                    removed.Insert(change);
                }
                if (!fallbackTrader)
                {
                    continue;
                }
                best = new BDE_EMU_LoadoutOffer;
                best.Kind = 1;
                best.CanBuy = true;
                fallbackTrader.GetTraderEntity().GetNetworkID(best.TraderLow, best.TraderHigh);
                selectedTrader = fallbackTrader;
            }
            else
            {
                demand.Set(best.ItemID, demand.Get(best.ItemID) + 1);
                if (best.StartingAmmo)
                {
                    demand.Set(best.StartingAmmo.ItemID, demand.Get(best.StartingAmmo.ItemID) + best.StartingAmmo.Count);
                }
            }
            ExpansionMarketSell sale = BDE_LM_ReplacementSale(player, oldItem, selectedTrader);
            BDE_LM_SetReplacementQuote(best, selectedTrader, oldItem, sale);
            ref BDE_EMU_LoadoutChange accepted = new BDE_EMU_LoadoutChange;
            accepted.Slots.Copy(change.Slots);
            accepted.Offer = best;
            resolved.Insert(accepted);
        }
        return resolved;
    }

    // Keep the normal market/currency item rules unchanged outside this exact quote.
    override int GetItemAmount(EntityAI item)
    {
        int amount = super.GetItemAmount(item);
        if (item && item == BDE_EMU_LoadoutQuoteContext.Item && amount < 0 && !item.IsRuined() && item.GetInventory().CanRemoveEntity())
        {
#ifdef EXPANSIONMODQUESTS
            ItemBase questItem = ItemBase.Cast(item);
            if (questItem && questItem.Expansion_IsQuestItem())
            {
                return amount;
            }
#endif
#ifdef EXPANSIONMODVEHICLE
            ExpansionCarKey carKey = ExpansionCarKey.Cast(item);
            if (carKey && carKey.IsMaster())
            {
                return amount;
            }
#endif
            // Checkout preserves cargo/attachments before destroying the sold shell.
            return Math.AbsInt(amount);
        }
        return amount;
    }

    protected bool BDE_LM_IsEquipmentSlot(int slotId)
    {
        string name = InventorySlots.GetSlotName(slotId);
        return name == "Shoulder" || name == "Melee" || name == "Headgear" || name == "Face" || name == "Eyes" || name == "Gloves" || name == "Armband" || name == "Vest" || name == "Body" || name == "Back" || name == "Belt" || name == "Hips" || name == "Legs" || name == "Feet";
    }

    protected bool BDE_LM_AllowedListing(ExpansionMarketTrader market, ExpansionMarketTraderZone zone, string className, int slotId)
    {
        if (!BDE_LM_ListedCategory(market, className) || !market.CanBuyItem(className))
        {
            return false;
        }
        if (!BDE_LM_IsEquipmentSlot(slotId) || !market.IsAttachmentBuySell(className))
        {
            return true;
        }
        set<ExpansionTraderObjectBase> registry = ExpansionTraderObjectBase.GetAll();
        if (!registry)
        {
            return false;
        }
        for (int i = 0; i < registry.Count(); i++)
        {
            ExpansionTraderObjectBase trader = registry.Get(i);
            if (!trader || trader.GetTraderZone() != zone || !trader.GetTraderMarket())
            {
                continue;
            }
            ExpansionMarketTrader directMarket = trader.GetTraderMarket();
            if (directMarket.ItemExists(className) && directMarket.CanBuyItem(className) && !directMarket.IsAttachmentBuySell(className))
            {
                return true;
            }
        }
        return false;
    }

    #ifdef BDE_UnderSlungLaunchers
    protected string BDE_LM_StandaloneWeapon(string mountType)
    {
        string key = mountType;
        key.ToLower();
        switch (key)
        {
            case "bde_m203_standalonemount":
                return "BDE_M203_StandAloneMount_Weapon";
            case "bde_fn40_standalonemount":
                return "BDE_FN40_StandaloneMount_Weapon";
            case "bde_fn40_standalonemount_fde":
                return "BDE_FN40_StandaloneMount_Weapon_FDE";
            case "bde_six12_standalonemount":
                return "BDE_Six12_StandAloneMount_Weapon";
            case "bde_six12_standalonemount_fde":
                return "BDE_Six12_StandAloneMount_Weapon_FDE";
        }
        return "";
    }
    #endif

    protected string BDE_LM_MarketType(string requestedType)
    {
        string key = requestedType;
        key.ToLower();
        #ifdef BDE_UnderSlungLaunchers
        switch (key)
        {
            case "bde_m203_standalonemount_weapon":
                return "bde_m203_standalonemount";
            case "bde_fn40_standalonemount_weapon":
                return "bde_fn40_standalonemount";
            case "bde_fn40_standalonemount_weapon_fde":
                return "bde_fn40_standalonemount_fde";
            case "bde_six12_standalonemount_weapon":
                return "bde_six12_standalonemount";
            case "bde_six12_standalonemount_weapon_fde":
                return "bde_six12_standalonemount_fde";
        }
        #endif
        return key;
    }

    protected ExpansionMarketCategory BDE_LM_ListedCategory(ExpansionMarketTrader market, string className, bool visibleOnly = true)
    {
        if (!market || !market.ItemExists(className))
        {
            return null;
        }
        className.ToLower();
        // Attachment-only listings are allowed here. Destination-slot policy
        // is enforced by BDE_LM_AllowedListing for browsing and checkout.
        foreach (int categoryID: market.m_CategoryIDs)
        {
            ExpansionMarketCategory category = GetExpansionSettings().GetMarket().GetCategory(categoryID);
            if (!category)
            {
                continue;
            }
            foreach (ExpansionMarketItem listed: category.Items)
            {
                if (listed.ClassName == className || (listed.Variants && listed.Variants.Find(className) >= 0))
                {
                    return category;
                }
            }
        }
        // Explicit Items entries can expose a category not listed in Categories.
        // Expansion also displays these under their item's market category.
        ExpansionMarketItem explicitItem = ExpansionMarketCategory.GetGlobalItem(className, false);
        if (explicitItem)
        {
            return explicitItem.Category;
        }
        return null;
    }

    static ref ScriptInvoker SI_BDELoadoutAmmo = new ScriptInvoker;

    void BDE_LM_RequestAmmo(Object station, int request, BDE_EMU_AmmoChange change)
    {
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMAmmo");
        rpc.Write(request);
        rpc.Write(change);
        rpc.Expansion_Send(station, true);
    }

    private void RPC_BDE_LMAmmo(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        int request;
        ref BDE_EMU_AmmoChange change;
        if (!senderRPC || !ctx.Read(request) || !ctx.Read(change) || !change)
        {
            return;
        }
        PlayerBase player = PlayerBase.GetPlayerByUID(senderRPC.GetId());
        EntityAI root;
        ExpansionMarketTraderZone zone;
        int low;
        int high;
        if (!player)
        {
            return;
        }
        player.GetNetworkID(low, high);
        string error;
        ref BDE_EMU_AmmoChange quote;
        if (!BDE_LM_RateLimit(senderRPC, "ammo", 150) || !BDE_LM_Resolve(senderRPC, target, low, high, InventorySlots.GetSlotIdFromString("Shoulder"), player, root, zone))
        {
            error = "Station unavailable. Try again.";
        }
        else
        {
            quote = BDE_LM_AmmoQuote(player, zone, change, error);
        }
        auto rpc = Expansion_CreateRPC("RPC_BDE_LMAmmoReply");
        rpc.Write(request);
        rpc.Write(quote);
        rpc.Write(error);
        rpc.Expansion_Send(null, true, senderRPC);
    }

    private void RPC_BDE_LMAmmoReply(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        int request;
        ref BDE_EMU_AmmoChange quote;
        string error;
        if (ctx.Read(request) && ctx.Read(quote) && ctx.Read(error))
        {
            SI_BDELoadoutAmmo.Invoke(request, quote, error);
        }
    }

    protected int BDE_LM_StartingAmmoCount(ExpansionMarketItem item)
    {
        if (!item || !GetGame().IsKindOf(item.ClassName, "Magazine_Base") || GetGame().IsKindOf(item.ClassName, "Ammunition_Base"))
        {
            return 0;
        }
        int percent = item.QuantityPercent;
        if (percent < 0)
        {
            percent = 100;
        }
        percent = Math.Clamp(percent, 0, 100);
        return Math.Round(GetGame().ConfigGetInt("CfgMagazines " + item.ClassName + " count") * percent / 100.0);
    }

    protected bool BDE_LM_AmmoCurrency(ExpansionTraderObjectBase trader, BDE_EMU_AmmoLine line)
    {
        ExpansionTraderObjectBase ammoTrader = GetTraderFromObject(GetGame().GetObjectByNetworkId(line.TraderLow, line.TraderHigh), false);
        return ammoTrader && BDE_LM_SameCurrencies(trader.GetTraderMarket(), ammoTrader.GetTraderMarket());
    }

    protected BDE_EMU_AmmoLine BDE_LM_StartingAmmo(PlayerBase player, ExpansionMarketTraderZone zone, ExpansionMarketItem item)
    {
        int count = BDE_LM_StartingAmmoCount(item);
        string bullet;
        GetGame().ConfigGetText("CfgMagazines " + item.ClassName + " ammo", bullet);
        TStringArray types = new TStringArray;
        GetGame().ConfigGetTextArray("CfgMagazines " + item.ClassName + " ammoItems", types);
        ref BDE_EMU_AmmoLine best;
        foreach (string ammoType: types)
        {
            string candidateBullet;
            GetGame().ConfigGetText("CfgMagazines " + ammoType + " ammo", candidateBullet);
            if (candidateBullet != bullet)
            {
                continue;
            }
            ref BDE_EMU_AmmoLine candidate = BDE_LM_AmmoPrice(player, zone, ammoType, count, false);
            if (candidate && (!best || candidate.Price < best.Price))
            {
                best = candidate;
            }
        }
        return best;
    }

    protected BDE_EMU_AmmoChange BDE_LM_AmmoQuote(PlayerBase player, ExpansionMarketTraderZone zone, BDE_EMU_AmmoChange requested, out string error)
    {
        if (!requested || !requested.Slots || requested.Slots.Count() < 1 || requested.Slots.Count() > 12 || (requested.Mode != 1 && requested.Mode != 2 && requested.Mode != 3))
        {
            error = "Invalid magazine operation.";
            return null;
        }
        ref BDE_EMU_AmmoChange quote = new BDE_EMU_AmmoChange;
        quote.Slots.Copy(requested.Slots);
        quote.MagazineClassName = requested.MagazineClassName;
        quote.Mode = requested.Mode;
        Magazine ownedMagazine;
        if (requested.Low != 0 || requested.High != 0)
        {
            ownedMagazine = Magazine.Cast(GetGame().GetObjectByNetworkId(requested.Low, requested.High));
            if (!ownedMagazine || !BDE_EMU_LoadoutUtil.IsEquippedParent(player, ownedMagazine) || ownedMagazine != BDE_EMU_LoadoutPaths.Resolve(player, requested.Slots, requested.Slots.Count()) || !CF_String.EqualsIgnoreCase(ownedMagazine.GetType(), requested.MagazineClassName))
            {
                error = "The magazine changed. Refresh your loadout.";
                return null;
            }
            ownedMagazine.GetNetworkID(quote.Low, quote.High);
            quote.OriginalSignature = BDE_EMU_AmmoUtil.Signature(ownedMagazine);
        }
        if (!GetGame().ConfigIsExisting("CfgMagazines " + quote.MagazineClassName) || GetGame().IsKindOf(quote.MagazineClassName, "Ammunition_Base"))
        {
            error = "This item is not a loadable magazine.";
            return null;
        }
        int maximum = GetGame().ConfigGetInt("CfgMagazines " + quote.MagazineClassName + " count");
        int existing;
        if (ownedMagazine)
        {
            existing = ownedMagazine.GetAmmoCount();
        }
        if (!ownedMagazine)
        {
            ExpansionTraderObjectBase purchaseTrader = GetTraderFromObject(GetGame().GetObjectByNetworkId(requested.PurchaseTraderLow, requested.PurchaseTraderHigh), false);
            ExpansionMarketItem purchaseItem = ExpansionMarketCategory.GetGlobalItem(requested.PurchaseItemID);
            if (!purchaseTrader || purchaseTrader.GetTraderZone() != zone || !purchaseItem || !CF_String.EqualsIgnoreCase(purchaseItem.ClassName, quote.MagazineClassName))
            {
                error = "Magazine purchase listing is unavailable.";
                return null;
            }
            BDE_EMU_LoadoutOffer purchaseQuote = BDE_LM_Quote(player, purchaseTrader, purchaseItem);
            if (!purchaseQuote.CanBuy)
            {
                error = purchaseQuote.Reason;
                return null;
            }
            quote.PurchaseItemID = requested.PurchaseItemID;
            quote.PurchaseTraderLow = requested.PurchaseTraderLow;
            quote.PurchaseTraderHigh = requested.PurchaseTraderHigh;
            int startingPrice = 0;
            if (purchaseQuote.StartingAmmo)
            {
                startingPrice = purchaseQuote.StartingAmmo.Price;
            }
            if (quote.Mode == 3)
            {
                if (purchaseQuote.StartingAmmo)
                {
                    quote.Lines.Insert(purchaseQuote.StartingAmmo);
                }
                return quote;
            }
            if (quote.Mode == 2)
            {
                quote.Price = -startingPrice;
                return quote;
            }
            // New magazines are filled from empty at checkout. Charge only the
            // difference from the starting rounds already in the equipment quote.
            quote.Price = -startingPrice;
        }
        else if (quote.Mode == 3)
        {
            error = "Starting ammunition only applies to new magazines.";
            return null;
        }
        TStringArray ammunition = new TStringArray;
        GetGame().ConfigGetTextArray("CfgMagazines " + quote.MagazineClassName + " ammoItems", ammunition);
        if (quote.Mode == 1)
        {
            int count = maximum - existing;
            if (count <= 0)
            {
                error = "The magazine is already full.";
                return null;
            }
            ref BDE_EMU_AmmoLine best;
            foreach (string ammoType: ammunition)
            {
                ref BDE_EMU_AmmoLine candidate = BDE_LM_AmmoPrice(player, zone, ammoType, count, false);
                if (candidate && (!best || candidate.Price < best.Price))
                {
                    best = candidate;
                }
            }
            if (!best)
            {
                error = "No compatible ammunition is available from an accessible trader.";
                return null;
            }
            quote.Lines.Insert(best);
            quote.Price += best.Price;
        }
        else
        {
            if (!ownedMagazine || existing == 0)
            {
                error = "There is no owned ammunition to remove.";
                return null;
            }
            ref map<string, int> counts = new map<string, int>;
            for (int i = 0; i < existing; i++)
            {
                float damage;
                string bullet;
                ownedMagazine.GetCartridgeAtIndex(i, damage, bullet);
                // Do not quote damaged rounds as pristine ammunition.
                if (damage > 0)
                {
                    error = "Damaged ammunition must be unloaded manually.";
                    return null;
                }
                counts.Set(bullet, counts.Get(bullet) + 1);
            }
            foreach (string cartridge, int rounds: counts)
            {
                ref BDE_EMU_AmmoLine bestSale;
                foreach (string sellType: ammunition)
                {
                    string configBullet;
                    GetGame().ConfigGetText("CfgMagazines " + sellType + " ammo", configBullet);
                    if (configBullet != cartridge)
                    {
                        continue;
                    }
                    ref BDE_EMU_AmmoLine sale = BDE_LM_AmmoPrice(player, zone, sellType, rounds, true);
                    if (sale && (!bestSale || sale.Price > bestSale.Price))
                    {
                        bestSale = sale;
                    }
                }
                if (!bestSale)
                {
                    error = "A trader cannot buy these rounds. Unload them manually to keep them.";
                    return null;
                }
                quote.Lines.Insert(bestSale);
                quote.Price -= bestSale.Price;
            }
        }
        return quote;
    }

    protected BDE_EMU_AmmoLine BDE_LM_AmmoPrice(PlayerBase player, ExpansionMarketTraderZone zone, string ammoType, int count, bool selling)
    {
        if (count < 1 || count > 1000 || !GetGame().IsKindOf(ammoType, "Ammunition_Base"))
        {
            return null;
        }
        ammoType.ToLower();
        ref BDE_EMU_AmmoLine best;
        set<ExpansionTraderObjectBase> registry = ExpansionTraderObjectBase.GetAll();
        if (!registry)
        {
            return null;
        }
        for (int i = 0; i < registry.Count(); i++)
        {
            ExpansionTraderObjectBase trader = registry.Get(i);
            if (!trader || trader.GetTraderZone() != zone || !trader.GetTraderEntity() || !trader.GetTraderMarket())
            {
                continue;
            }
            ExpansionMarketTrader market = trader.GetTraderMarket();
            ExpansionMarketItem item = ExpansionMarketCategory.GetGlobalItem(ammoType, false);
            if (!item || !BDE_LM_ListedCategory(market, ammoType, !selling) || BDE_LM_Access(player, market, selling) != "" || !CheckCanUseTrader(player, trader))
            {
                continue;
            }
            int price = 0;
            ExpansionMarketResult result;
            bool valid = false;
            if (!selling && market.CanBuyItem(ammoType))
            {
                valid = FindPriceOfPurchaseEx(item, zone, market, player, count, price, false, result);
            }
            else if (selling && market.CanSellItem(ammoType))
            {
                ref array<EntityAI> candidates = new array<EntityAI>;
                int remaining = count;
                while (remaining > 0)
                {
                    Magazine pile = Magazine.Cast(GetGame().CreateObjectEx(ammoType, vector.Zero, ECE_LOCAL | ECE_NOLIFETIME));
                    if (!pile || pile.GetAmmoMax() < 1)
                    {
                        if (pile)
                        {
                            GetGame().ObjectDelete(pile);
                        }
                        break;
                    }
                    int pileCount = Math.Min(remaining, pile.GetAmmoMax());
                    pile.ServerSetAmmoCount(pileCount);
                    candidates.Insert(pile);
                    remaining -= pileCount;
                }
                if (remaining == 0)
                {
                    ref ExpansionMarketSell sale = new ExpansionMarketSell(player);
                    sale.Trader = trader;
                    sale.Item = item;
                    valid = FindSellPrice(player, candidates, zone.GetStock(ammoType), count, sale, false, result);
                    valid = valid && sale.TotalAmount == count;
                    price = sale.Price;
                }
                foreach (EntityAI quotedPile: candidates)
                {
                    GetGame().ObjectDelete(quotedPile);
                }
            }

            if (!valid || result != ExpansionMarketResult.Success || price < 0)
            {
                continue;
            }
            if (best && ((!selling && best.Price <= price) || (selling && best.Price >= price)))
            {
                continue;
            }
            best = new BDE_EMU_AmmoLine;
            best.ClassName = ammoType;
            best.Count = count;
            best.Price = price;
            best.ItemID = item.ItemID;
            best.Stock = zone.GetStock(ammoType);
            if (item.IsStaticStock())
            {
                best.Stock = -1;
            }
            trader.GetTraderEntity().GetNetworkID(best.TraderLow, best.TraderHigh);
            GetGame().ConfigGetText("CfgMagazines " + ammoType + " ammo", best.Bullet);
        }
        return best;
    }

    protected void BDE_LM_RestoreAmmo(array<ref BDE_EMU_AmmoCommit> commits)
    {
        foreach (BDE_EMU_AmmoCommit commit: commits)
        {
            if (!commit.MagazineEntity || !commit.SnapshotTaken)
            {
                continue;
            }
            commit.MagazineEntity.ServerSetAmmoCount(0);
            for (int i = 0; i < commit.Bullets.Count(); i++)
            {
                commit.MagazineEntity.ServerStoreCartridge(commit.Damage[i], commit.Bullets[i]);
            }
            commit.MagazineEntity.SetSynchDirty();
        }
    }
}
