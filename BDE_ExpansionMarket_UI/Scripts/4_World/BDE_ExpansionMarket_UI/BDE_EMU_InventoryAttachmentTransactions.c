class BDE_EMU_InventoryAttachmentRemoval
{
    EntityAI Item;
    ref InventoryLocation Location;
    ref ExpansionMarketSell Sell;
    bool Detached;
};

/**
 * Server-authoritative transaction path for the player Item Editor.
 *
 * The client only sends the desired direct attachment delta. The server
 * re-resolves the player, trader, target entity, market permissions, stock,
 * reputation and prices before it mutates anything. Removed attachments are
 * temporarily detached and can be restored if any later validation fails.
 */
modded class ExpansionMarketModule
{
    static ref ScriptInvoker SI_BDEInventoryAttachmentEditResult = new ScriptInvoker;

    override void OnInit()
    {
        super.OnInit();
        Expansion_RegisterServerRPC("RPC_BDE_RequestInventoryAttachmentEdit");
        Expansion_RegisterClientRPC("RPC_BDE_InventoryAttachmentEditResult");
    }

    bool BDE_EMU_IsNestedInstalledItem(EntityAI item)
    {
        if (!item || !item.GetInventory())
            return false;

        InventoryLocation location = new InventoryLocation();
        if (!item.GetInventory().GetCurrentInventoryLocation(location) || !location.IsValid())
            return false;

        if (location.GetType() != InventoryLocationType.ATTACHMENT)
            return false;

        EntityAI parent = location.GetParent();
        if (!parent || parent.IsMan())
            return false;

        return true;
    }

    bool BDE_EMU_CanSellPrimaryEntity(EntityAI item)
    {
        if (!item || !item.GetInventory())
        {
            return false;
        }

        // Loadout quotes explicitly target one owned attachment. Ordinary market
        // sales still exclude installed children to prevent double-counting.
        if (item == BDE_EMU_LoadoutQuoteContext.Item)
        {
            return true;
        }
        return !BDE_EMU_IsNestedInstalledItem(item);
    }

    array<EntityAI> BDE_EMU_GetPrimarySellItems(array<EntityAI> items)
    {
        array<EntityAI> filteredItems = new array<EntityAI>;

        if (!items)
            return filteredItems;

        foreach (EntityAI item: items)
        {
            if (BDE_EMU_CanSellPrimaryEntity(item))
                filteredItems.Insert(item);
        }

        return filteredItems;
    }

    int BDE_EMU_GetSellablePrimaryAmount(ExpansionMarketItem marketItem, ExpansionMarketTrader traderMarket, array<EntityAI> items)
    {
        if (!marketItem || !traderMarket || !items)
            return 0;

        string targetClassName = marketItem.ClassName;
        targetClassName.ToLower();

        int amount = 0;

        foreach (EntityAI item: items)
        {
            if (!BDE_EMU_CanSellPrimaryEntity(item))
                continue;

            string itemClassName = item.GetType();
            itemClassName.ToLower();
            itemClassName = GetMarketItemClassName(traderMarket, itemClassName);

            if (itemClassName != targetClassName)
                continue;

            int itemAmount = GetItemAmount(item);
            if (itemAmount > 0)
                amount += itemAmount;
        }

        return amount;
    }

    override bool FindSellPrice(notnull PlayerBase player, array<EntityAI> items, int stock, int amountWanted, ExpansionMarketSell sell, bool includeAttachments = true, out ExpansionMarketResult result = ExpansionMarketResult.Success, out string failedClassName = "")
    {
        array<EntityAI> filteredItems = BDE_EMU_GetPrimarySellItems(items);
        return super.FindSellPrice(player, filteredItems, stock, amountWanted, sell, includeAttachments, result, failedClassName);
    }

    void BDE_RequestInventoryAttachmentEdit(int targetLow, int targetHigh, TStringArray originalAttachments, TStringArray additions, TStringArray removals, int clientBuyTotal, int clientSellTotal, ExpansionTraderObjectBase trader)
    {
        if (g_Game.IsDedicatedServer() || !trader || !trader.GetTraderEntity())
            return;

        auto rpc = Expansion_CreateRPC("RPC_BDE_RequestInventoryAttachmentEdit");
        rpc.Write(targetLow);
        rpc.Write(targetHigh);
        rpc.Write(originalAttachments);
        rpc.Write(additions);
        rpc.Write(removals);
        rpc.Write(clientBuyTotal);
        rpc.Write(clientSellTotal);
        rpc.Expansion_Send(trader.GetTraderEntity(), true);
    }

    private void RPC_BDE_RequestInventoryAttachmentEdit(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        if (!senderRPC || !GetExpansionSettings().GetMarket().MarketSystemEnabled)
            return;

        int targetLow;
        int targetHigh;
        TStringArray originalAttachments;
        TStringArray additions;
        TStringArray removals;
        int clientBuyTotal;
        int clientSellTotal;

        if (!ctx.Read(targetLow) || !ctx.Read(targetHigh) || !ctx.Read(originalAttachments) || !ctx.Read(additions) || !ctx.Read(removals) || !ctx.Read(clientBuyTotal) || !ctx.Read(clientSellTotal))
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The Item Editor request was incomplete.");
            return;
        }

        ExpansionTraderObjectBase trader = GetTraderFromObject(target);
        if (!trader || !trader.GetTraderMarket() || !trader.GetTraderZone() || !trader.GetTraderEntity())
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The trader is no longer available.");
            return;
        }

        PlayerBase player = PlayerBase.GetPlayerByUID(senderRPC.GetId());
        if (!player || !player.GetIdentity())
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The player could not be resolved.");
            return;
        }

        if (!CheckCanUseTrader(player, trader))
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "You cannot use this trader right now.");
            return;
        }

        if (vector.Distance(player.GetPosition(), trader.GetTraderEntity().GetPosition()) > MAX_TRADER_INTERACTION_DISTANCE)
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "You are too far away from the trader.");
            return;
        }

        EntityAI targetItem = EntityAI.Cast(g_Game.GetObjectByNetworkId(targetLow, targetHigh));
        if (!targetItem || targetItem.IsPendingDeletion() || targetItem.GetHierarchyRootPlayer() != player)
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The selected inventory item is no longer in your possession.");
            return;
        }

        if (!targetItem.IsWeapon() && !targetItem.IsClothing())
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "That item cannot be configured by the Item Editor.");
            return;
        }

        if (!originalAttachments)
            originalAttachments = new TStringArray;
        if (!additions)
            additions = new TStringArray;
        if (!removals)
            removals = new TStringArray;

        if (additions.Count() > 0 && BDE_EMU_ReputationAccess.IsBelowRequirement(player, trader.GetTraderMarket()))
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "You must meet this trader's reputation requirement to buy attachments. You can still sell items at 25% less.");
            return;
        }

        if (originalAttachments.Count() > 30 || additions.Count() > 30 || removals.Count() > 30)
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The attachment request exceeded the Item Editor limit.");
            return;
        }

        BDE_NormalizeInventoryAttachmentClasses(originalAttachments);
        BDE_NormalizeInventoryAttachmentClasses(additions);
        BDE_NormalizeInventoryAttachmentClasses(removals);

        TStringArray currentAttachments = new TStringArray;
        BDE_GetDirectAttachmentClasses(targetItem, currentAttachments);
        if (!BDE_AttachmentClassListsMatch(currentAttachments, originalAttachments))
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The item's attachments changed while you were editing it. Re-open the editor and try again.");
            return;
        }

        TStringArray finalAttachments = new TStringArray;
        finalAttachments.InsertAll(originalAttachments);
        foreach (string removalClass: removals)
        {
            int removalIndex = finalAttachments.Find(removalClass);
            if (removalIndex < 0)
            {
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The requested removal no longer exists on the item.");
                return;
            }
            finalAttachments.RemoveOrdered(removalIndex);
        }
        finalAttachments.InsertAll(additions);

        ExpansionMarketTrader traderMarket = trader.GetTraderMarket();
        ExpansionMarketTraderZone zone = trader.GetTraderZone();

        map<string, int> purchaseStock = new map<string, int>;
        map<string, int> additionQuantityPercents = new map<string, int>;
        TStringArray additionMarketClasses = new TStringArray;
        int buyTotal = 0;
        foreach (string additionClass: additions)
        {
            string additionMarketClass = additionClass;
            additionMarketClass.ToLower();
            additionMarketClass = GetMarketItemClassName(traderMarket, additionMarketClass);

            if (!traderMarket.ItemExists(additionMarketClass) || !traderMarket.CanBuyItem(additionMarketClass))
            {
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "One of the selected attachments cannot be bought from this trader.");
                return;
            }

            ExpansionMarketItem additionMarketItem = ExpansionMarketCategory.GetGlobalItem(additionMarketClass, false);
            if (!additionMarketItem)
            {
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "One of the selected attachments is missing from the market database.");
                return;
            }

            int additionPrice = 0;
            ExpansionMarketResult additionResult;
            ExpansionMarketReserve additionReserve;
            if (!FindPriceOfPurchaseEx(additionMarketItem, zone, traderMarket, player, 1, additionPrice, false, additionResult, additionReserve, purchaseStock) || additionResult != ExpansionMarketResult.Success)
            {
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "One of the selected attachments is no longer available to buy.");
                return;
            }

            if (ExpansionMath.TestAdditionOverflow(buyTotal, additionPrice))
            {
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The attachment purchase total is too large.");
                return;
            }

            buyTotal += additionPrice;
            additionMarketClasses.Insert(additionMarketClass);
            additionQuantityPercents.Set(additionClass, additionMarketItem.QuantityPercent);
        }

        string compatibilityError;
        if (!BDE_ValidateFinalInventoryAttachmentSet(targetItem.GetType(), finalAttachments, compatibilityError))
        {
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, compatibilityError);
            return;
        }

        ref array<ref BDE_EMU_InventoryAttachmentRemoval> removalRecords = new array<ref BDE_EMU_InventoryAttachmentRemoval>;
        ref array<EntityAI> usedRemovalItems = new array<EntityAI>;
        foreach (string requestedRemovalClass: removals)
        {
            EntityAI removalItem = BDE_FindUnusedDirectInventoryAttachment(targetItem, requestedRemovalClass, usedRemovalItems);
            if (!removalItem)
            {
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "An attachment selected for removal could not be found.");
                return;
            }

            usedRemovalItems.Insert(removalItem);
            BDE_EMU_InventoryAttachmentRemoval removalRecord = new BDE_EMU_InventoryAttachmentRemoval;
            removalRecord.Item = removalItem;
            removalRecord.Location = new InventoryLocation;
            removalItem.GetInventory().GetCurrentInventoryLocation(removalRecord.Location);
            removalRecords.Insert(removalRecord);
        }

        // Detach first so Expansion's standard sell-pricing path sees each
        // attachment as a sellable loose entity. Nothing has been destroyed or
        // charged yet, so every failure below can still be rolled back.
        foreach (BDE_EMU_InventoryAttachmentRemoval detachRecord: removalRecords)
        {
            if (!GameInventory.LocationRemoveEntity(detachRecord.Location))
            {
                BDE_RestoreDetachedInventoryAttachments(removalRecords);
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "An attachment could not be detached from the item.");
                return;
            }
            detachRecord.Detached = true;
        }

        int sellTotal = 0;
        foreach (BDE_EMU_InventoryAttachmentRemoval priceRecord: removalRecords)
        {
            string sellMarketClass = priceRecord.Item.GetType();
            sellMarketClass.ToLower();
            sellMarketClass = GetMarketItemClassName(traderMarket, sellMarketClass);

            if (!traderMarket.ItemExists(sellMarketClass) || !traderMarket.CanSellItem(sellMarketClass))
            {
                BDE_RestoreDetachedInventoryAttachments(removalRecords);
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "One of the removed attachments cannot be sold to this trader.");
                return;
            }

            ExpansionMarketItem sellMarketItem = ExpansionMarketCategory.GetGlobalItem(sellMarketClass, false);
            if (!sellMarketItem)
            {
                BDE_RestoreDetachedInventoryAttachments(removalRecords);
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "One of the removed attachments is missing from the market database.");
                return;
            }

            int sellStock = 1;
            if (!sellMarketItem.IsStaticStock())
                sellStock = zone.GetStock(sellMarketClass);

            priceRecord.Sell = new ExpansionMarketSell(player);
            priceRecord.Sell.Item = sellMarketItem;
            priceRecord.Sell.Trader = trader;
            array<EntityAI> sellItems = {priceRecord.Item};
            ExpansionMarketResult sellResult;
            string failedClassName;
            if (!FindSellPrice(player, sellItems, sellStock, 1, priceRecord.Sell, true, sellResult, failedClassName) || sellResult != ExpansionMarketResult.Success)
            {
                BDE_RestoreDetachedInventoryAttachments(removalRecords);
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "One of the removed attachments can no longer be sold.");
                return;
            }

#ifdef SERVER
            if (GetExpansionSettings().GetMarket().DisallowUnpersisted)
            {
                foreach (ExpansionMarketSellItem persistCheck: priceRecord.Sell.Sell)
                {
                    if (persistCheck.IsEntity && persistCheck.ItemRep && !ExpansionWorld.IsStoreLoaded(persistCheck.ItemRep) && !ExpansionWorld.IsStoreSaved(persistCheck.ItemRep))
                    {
                        BDE_RestoreDetachedInventoryAttachments(removalRecords);
                        BDE_SendInventoryAttachmentEditResult(senderRPC, false, "One of the removed attachments has not been persisted yet and cannot be sold.");
                        return;
                    }
                }
            }
#endif

            if (ExpansionMath.TestAdditionOverflow(sellTotal, priceRecord.Sell.Price))
            {
                BDE_RestoreDetachedInventoryAttachments(removalRecords);
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The attachment sale total is too large.");
                return;
            }
            sellTotal += priceRecord.Sell.Price;
        }

        int netCost = buyTotal - sellTotal;
        if (netCost > 0)
        {
            array<int> monies;
            if (!FindMoneyAndCountTypes(player, netCost, monies, false, null, traderMarket))
            {
                BDE_RestoreDetachedInventoryAttachments(removalRecords);
                BDE_SendInventoryAttachmentEditResult(senderRPC, false, "You no longer have enough money for these attachment changes.");
                return;
            }
        }

        ref array<EntityAI> spawnedAdditions = new array<EntityAI>;
        if (!BDE_SpawnInventoryAttachmentAdditions(targetItem, additions, spawnedAdditions, additionQuantityPercents))
        {
            BDE_DeleteInventoryAttachmentAdditions(spawnedAdditions);
            BDE_RestoreDetachedInventoryAttachments(removalRecords);
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "An attachment could not be attached to the item. No changes were charged.");
            return;
        }

        if (netCost > 0 && !RemoveMoney(netCost, player, traderMarket.Currencies))
        {
            BDE_DeleteInventoryAttachmentAdditions(spawnedAdditions);
            BDE_RestoreDetachedInventoryAttachments(removalRecords);
            BDE_SendInventoryAttachmentEditResult(senderRPC, false, "The payment could not be completed. No attachment changes were kept.");
            return;
        }

        // Commit removed items and their nested market-sellable contents.
        foreach (BDE_EMU_InventoryAttachmentRemoval commitRemoval: removalRecords)
        {
            for (int sellIndex = commitRemoval.Sell.Sell.Count() - 1; sellIndex >= 0; sellIndex--)
            {
                ExpansionMarketSellItem sellItem = commitRemoval.Sell.Sell[sellIndex];
                if (sellItem.IsEntity && sellItem.ItemRep && !sellItem.ItemRep.IsPendingDeletion())
                    sellItem.DestroyItem();
                zone.AddStock(sellItem.ClassName, sellItem.AddStockAmount);
            }
            commitRemoval.Detached = false;
        }

        // The selected additions are purchased as the attachment itself; this
        // mirrors what the Item Editor visibly stages and avoids silently adding
        // unrelated default children that the player did not select.
        foreach (string boughtMarketClass: additionMarketClasses)
            zone.RemoveStock(boughtMarketClass, 1, false);

        if (netCost < 0)
        {
            EntityAI moneyParent = player;
            SpawnMoney(player, moneyParent, -netCost, true, null, traderMarket);
            CheckSpawn(player, moneyParent);
        }

        zone.Save();

        string resultMessage;
        if (buyTotal > 0 && sellTotal > 0)
            resultMessage = string.Format("Attachments updated. Bought %1, sold %2, net %3.", buyTotal, sellTotal, netCost);
        else if (buyTotal > 0)
            resultMessage = string.Format("Attachments updated. Purchased for %1.", buyTotal);
        else if (sellTotal > 0)
            resultMessage = string.Format("Attachments updated. Sold for %1.", sellTotal);
        else
            resultMessage = "Attachments updated.";

        BDE_SendInventoryAttachmentEditResult(senderRPC, true, resultMessage);
    }

    private void RPC_BDE_InventoryAttachmentEditResult(PlayerIdentity senderRPC, Object target, ParamsReadContext ctx)
    {
        bool success;
        string message;
        if (!ctx.Read(success) || !ctx.Read(message))
            return;

        SI_BDEInventoryAttachmentEditResult.Invoke(success, message);
    }

    protected void BDE_SendInventoryAttachmentEditResult(PlayerIdentity identity, bool success, string message)
    {
        if (!identity)
            return;

        auto rpc = Expansion_CreateRPC("RPC_BDE_InventoryAttachmentEditResult");
        rpc.Write(success);
        rpc.Write(message);
        rpc.Expansion_Send(true, identity);
    }

    protected void BDE_NormalizeInventoryAttachmentClasses(TStringArray classNames)
    {
        if (!classNames)
            return;

        for (int i = 0; i < classNames.Count(); i++)
        {
            string className = classNames[i];
            className.ToLower();
            classNames[i] = className;
        }
    }

    protected void BDE_GetDirectAttachmentClasses(EntityAI item, TStringArray classNames)
    {
        classNames.Clear();
        for (int i = 0; i < item.GetInventory().AttachmentCount(); i++)
        {
            EntityAI attachment = item.GetInventory().GetAttachmentFromIndex(i);
            if (!attachment)
                continue;

            string className = attachment.GetType();
            className.ToLower();
            classNames.Insert(className);
        }
    }

    protected bool BDE_AttachmentClassListsMatch(TStringArray a, TStringArray b)
    {
        if (!a || !b || a.Count() != b.Count())
            return false;

        TStringArray sortedA = new TStringArray;
        TStringArray sortedB = new TStringArray;
        sortedA.InsertAll(a);
        sortedB.InsertAll(b);
        sortedA.Sort();
        sortedB.Sort();

        for (int i = 0; i < sortedA.Count(); i++)
        {
            if (sortedA[i] != sortedB[i])
                return false;
        }
        return true;
    }

    protected bool BDE_ValidateFinalInventoryAttachmentSet(string parentClass, TStringArray finalAttachments, out string errorMessage)
    {
        errorMessage = "The selected attachment combination is not compatible with this item.";

        EntityAI testParent = EntityAI.Cast(g_Game.CreateObjectEx(parentClass, vector.Zero, ECE_LOCAL|ECE_NOLIFETIME));
        if (!testParent)
        {
            errorMessage = "The Item Editor could not validate the selected item type.";
            return false;
        }

        ref array<EntityAI> spawnedAttachments = new array<EntityAI>;
        bool valid = BDE_SpawnInventoryAttachmentAdditions(testParent, finalAttachments, spawnedAttachments);

        g_Game.ObjectDelete(testParent);
        return valid;
    }

    protected bool BDE_SpawnInventoryAttachmentAdditions(EntityAI parent, TStringArray attachmentClasses, array<EntityAI> spawnedAttachments, map<string, int> quantityPercents = null)
    {
        if (!parent || !attachmentClasses || !spawnedAttachments)
            return false;

        TStringArray remaining = new TStringArray;
        remaining.InsertAll(attachmentClasses);

        while (remaining.Count() > 0)
        {
            bool madeProgress;
            for (int i = remaining.Count() - 1; i >= 0; i--)
            {
                EntityAI spawnedAttachment = ExpansionItemSpawnHelper.SpawnAttachment(remaining[i], parent);
                if (!spawnedAttachment)
                    continue;

                if (quantityPercents)
                {
                    int quantityPercent;
                    if (quantityPercents.Find(remaining[i], quantityPercent))
                        BDE_ApplyPurchasedAttachmentQuantity(spawnedAttachment, quantityPercent);
                }

                spawnedAttachments.Insert(spawnedAttachment);
                remaining.RemoveOrdered(i);
                madeProgress = true;
            }

            if (!madeProgress)
                return false;
        }

        return true;
    }

    // Expansion's normal market purchase path applies the market item's
    // QuantityPercent after spawning. Item Editor additions are forced directly
    // into attachment slots, so mirror that part of the standard purchase here.
    protected void BDE_ApplyPurchasedAttachmentQuantity(EntityAI entity, int quantityPercent)
    {
        ItemBase item = ItemBase.Cast(entity);
        if (!item)
            return;

        if (quantityPercent < 0)
            quantityPercent = 100;

        if (item.IsAmmoPile())
        {
            Ammunition_Base ammo = Ammunition_Base.Cast(item);
            if (ammo)
                ammo.ServerSetAmmoCount(Math.Min(1, ammo.GetAmmoMax()));
        }
        else if (item.IsMagazine())
        {
            Magazine mag = Magazine.Cast(item);
            if (mag)
                mag.ServerSetAmmoCount(Math.Round(mag.GetAmmoMax() * quantityPercent / 100));
        }
        else if (item.HasQuantity())
        {
            float itemQuantity = item.GetQuantityMax() * quantityPercent / 100;
            string stackedUnit = item.ConfigGetString("stackedUnit");
            if (item.Expansion_IsStackable() || stackedUnit == "pc." || stackedUnit == "pills")
                itemQuantity = Math.Round(itemQuantity);
            item.SetQuantity(itemQuantity);

            if (item.GetCompEM())
                item.GetCompEM().SetEnergy0To1(quantityPercent / 100);
        }

        if (item.CanBeDisinfected())
            item.SetCleanness(1);
    }

    protected EntityAI BDE_FindUnusedDirectInventoryAttachment(EntityAI parent, string className, array<EntityAI> usedItems)
    {
        for (int i = 0; i < parent.GetInventory().AttachmentCount(); i++)
        {
            EntityAI attachment = parent.GetInventory().GetAttachmentFromIndex(i);
            if (!attachment || usedItems.Find(attachment) > -1)
                continue;

            if (CF_String.EqualsIgnoreCase(attachment.GetType(), className))
                return attachment;
        }
        return null;
    }

    protected void BDE_DeleteInventoryAttachmentAdditions(array<EntityAI> additions)
    {
        if (!additions)
            return;

        for (int i = additions.Count() - 1; i >= 0; i--)
        {
            EntityAI attachment = additions[i];
            if (attachment && !attachment.IsPendingDeletion())
                g_Game.ObjectDelete(attachment);
        }
    }

    protected void BDE_RestoreDetachedInventoryAttachments(array<ref BDE_EMU_InventoryAttachmentRemoval> removals)
    {
        if (!removals)
            return;

        for (int i = removals.Count() - 1; i >= 0; i--)
        {
            BDE_EMU_InventoryAttachmentRemoval removal = removals[i];
            if (!removal || !removal.Detached || !removal.Item || removal.Item.IsPendingDeletion() || !removal.Location)
                continue;

            if (!GameInventory.LocationAddEntity(removal.Location))
                Error("[BDE Market UI] Failed to restore detached Item Editor attachment " + removal.Item.GetType());
            else
                removal.Detached = false;
        }
    }
};
