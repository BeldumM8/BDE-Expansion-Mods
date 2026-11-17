/**
 * Expansion's item manager remains the authoritative attachment logic engine.
 * Normal market items and inventory clothing use the BDE embedded cards, while
 * inventory weapons can temporarily show Expansion's stock item-manager view.
 */
modded class ExpansionMarketMenuItemManager
{
    protected EntityAI m_BDEInventoryTarget;
    protected bool m_BDEVanillaInventoryWeaponMode;

    void BDE_PrepareEmbeddedAttachmentMode(ExpansionMarketItem marketItem)
    {
        m_BDEInventoryTarget = null;
        SetMarketItem(marketItem);
    }

    void BDE_PrepareEmbeddedInventoryAttachmentMode(ExpansionMarketItem marketItem, EntityAI inventoryTarget)
    {
        m_BDEInventoryTarget = inventoryTarget;
        SetMarketItem(marketItem);
    }

    void BDE_PrepareVanillaInventoryWeaponMode(ExpansionMarketItem marketItem)
    {
        // Keep the manager itself completely on Expansion's stock path for
        // weapons. In particular, do not inject the real inventory target into
        // AddCategory: the vanilla manager should build the exact same category
        // and item sources it builds for a weapon being purchased from market.
        m_BDEInventoryTarget = null;
        SetMarketItem(marketItem);
    }

    void BDE_SetVanillaInventoryWeaponMode(bool state)
    {
        m_BDEVanillaInventoryWeaponMode = state;

        if (button_close_label)
        {
            if (state)
                button_close_label.SetText("BACK TO INV ATTACHMENTS");
            else
                button_close_label.SetText("#STR_MOD_EXPANSION_CLOSE");
        }

        if (button_reset_label)
        {
            if (state)
                button_reset_label.SetText("RESET");
            else
                button_reset_label.SetText("#STR_EXPANSION_MARKET_PRESETS_CLEAR_BUTTON_LABEL");
        }

        TextWidget saveLabel = TextWidget.Cast(GetLayoutRoot().FindAnyWidget("button_save_label"));
        if (saveLabel)
        {
            if (state)
                saveLabel.SetText("BUY");
            else
                saveLabel.SetText("#STR_EXPANSION_MARKET_PRESETS_SAVE_BUTTON_LABEL");
        }

        if (button_save)
        {
            button_save.Show(true);
            if (state)
                button_save.Enable(true);
        }
    }

    override void OnCloseButtonClick()
    {
        if (m_BDEVanillaInventoryWeaponMode && m_MarketMenu)
        {
            m_MarketMenu.BDE_CancelVanillaInventoryWeaponAttachmentEdit();
            return;
        }

        super.OnCloseButtonClick();
    }

    override void OnResetButtonClick()
    {
        if (m_BDEVanillaInventoryWeaponMode && m_MarketMenu)
        {
            m_MarketMenu.BDE_ResetVanillaInventoryWeaponAttachments();
            UpdateMenuViews();
            return;
        }

        super.OnResetButtonClick();
    }

    override void OnSavePresetButtonClick()
    {
        if (m_BDEVanillaInventoryWeaponMode && m_MarketMenu)
        {
            m_MarketMenu.BDE_ApplyVanillaInventoryWeaponAttachmentEdit();
            return;
        }

        super.OnSavePresetButtonClick();
    }

    override void AddCategory(string className, string title, array<ExpansionMarketAttachType> attachTypes, string icon)
    {
        TStringArray atts = m_MarketMenu.GetMarketFilters().GetAttachmentsByClassNameAndTypes(className, attachTypes);

        if (m_BDEInventoryTarget)
        {
            ExpansionMarketModule marketModule = ExpansionMarketModule.GetInstance();
            ExpansionTraderObjectBase traderObject = m_MarketMenu.GetTraderObject();
            ExpansionMarketTrader traderMarket;
            if (traderObject)
                traderMarket = traderObject.GetTraderMarket();

            for (int i = 0; i < m_BDEInventoryTarget.GetInventory().AttachmentCount(); i++)
            {
                EntityAI existingAttachment = m_BDEInventoryTarget.GetInventory().GetAttachmentFromIndex(i);
                if (!existingAttachment)
                    continue;

                string existingClass = existingAttachment.GetType();
                if (!m_MarketMenu.GetMarketFilters().BDE_IsAttachmentOfTypes(className, existingClass, attachTypes))
                    continue;

                string existingLower = existingClass;
                existingLower.ToLower();
                if (!ExpansionMarketCategory.GetGlobalItem(existingLower, false))
                    continue;

                string marketClass = existingLower;
                if (marketModule && traderMarket)
                    marketClass = marketModule.GetMarketItemClassName(traderMarket, marketClass);

                if (traderMarket && traderMarket.ItemExists(marketClass) && traderMarket.CanSellItem(marketClass) && !BDE_ContainsAttachmentClass(atts, existingClass))
                    atts.Insert(existingClass);
            }
        }

        AddCategoryEx(title, atts, icon);
    }

    protected bool BDE_ContainsAttachmentClass(TStringArray attachments, string className)
    {
        foreach (string attachment: attachments)
        {
            if (CF_String.EqualsIgnoreCase(attachment, className))
                return true;
        }
        return false;
    }

    void BDE_ClearEmbeddedAttachmentData()
    {
        m_BDEInventoryTarget = null;
        if (m_BDEVanillaInventoryWeaponMode)
            BDE_SetVanillaInventoryWeaponMode(false);

        if (!m_MarketItemManagerController)
            return;

        m_MarketItemManagerController.CategoryItems.Clear();
        m_MarketItemManagerController.ItemCategories.Clear();
    }

    override void SetListView(TStringArray attachments)
    {
        super.SetListView(attachments);

        if (m_MarketMenu)
            m_MarketMenu.BDE_OnAttachmentCategoryItemsChanged();
    }

    override void UpdateListView()
    {
        super.UpdateListView();

        if (m_MarketMenu)
            m_MarketMenu.BDE_OnAttachmentLogicUpdated();
    }
};

modded class ExpansionMarketMenuItemManagerCategory
{
    string BDE_GetCategoryName()
    {
        return Widget.TranslateString(m_CategoryName);
    }

    string BDE_GetCategoryIcon()
    {
        if (m_MarketItemManagerCategoryController)
            return m_MarketItemManagerCategoryController.CategoryIcon;
        return "";
    }

    int BDE_GetAttachmentCount()
    {
        if (!m_ItemAttachments)
            return 0;
        return m_ItemAttachments.Count();
    }
};

modded class ExpansionMarketMenuItemManagerCategoryItem
{
    bool BDE_CanAddAttachment()
    {
        return m_CanBeAttachedOrReplaceConflicting;
    }

    void BDE_RefreshAttachmentCompatibility()
    {
        UpdateView();
    }

    bool BDE_CanStageAnotherAttachment()
    {
        // Expansion's category candidate list already establishes that the
        // class belongs to one of the parent item's configured attachment
        // slots. For the first copy, let Expansion's own increment path do
        // its replacement/conflict handling instead of rejecting the card in
        // a second BDE preflight pass. This matches the working Market Item
        // customiser path for buttstocks, handguards, switchables and similar
        // replacement-style attachments.
        if (GetCount() == 0)
            return true;

        // Once at least one identical class is staged, ADD means an actual
        // additional copy. At that point we still require a genuinely free
        // compatible attachment slot so multi-slot vests/belts can accept
        // several copies without inventing extra capacity.
        return BDE_CanAddAnotherAttachment();
    }

    bool BDE_CanAddAnotherAttachment()
    {
        EntityAI parent;
        if (m_MarketMenu && m_MarketMenu.GetSelectedMarketItemElement())
            parent = EntityAI.Cast(m_MarketMenu.GetSelectedMarketItemElement().GetPreviewObject());
        if (!parent)
            return false;

        // Always test the current preview directly instead of relying only on
        // UpdateView's cached flags. For multiple identical attachments this
        // means ADD is available only while a genuinely free compatible slot
        // still exists.
        bool canAttach = CanAttachAttachment(parent, m_ItemClassName, false);
        int count = GetCount();
        if (canAttach || count > 0)
            return canAttach;

        // Expansion's UpdateView replacement test checks slot conflicts only,
        // while OnItemCountIncrement also checks attachment type for weapons.
        // Modded buttstocks/handguards can use different slot names while still
        // being replacements of the same weapon attachment type, so mirror the
        // actual increment path here and then verify the candidate can attach.
        string conflictingAttachment;
        bool checkSlotOnly = !parent.IsWeapon();
        if (!IsAttachmentConflicting(m_ItemClassName, checkSlotOnly, conflictingAttachment) || conflictingAttachment == string.Empty)
            return false;

        RemoveAttachment(parent, conflictingAttachment);
        bool canReplace = CanAttachAttachment(parent, m_ItemClassName, false);
        ExpansionItemSpawnHelper.SpawnAttachment(conflictingAttachment, parent);
        return canReplace;
    }

    bool BDE_ShouldShowInEmbeddedList()
    {
        EntityAI parent;
        if (m_MarketMenu && m_MarketMenu.GetSelectedMarketItemElement())
            parent = EntityAI.Cast(m_MarketMenu.GetSelectedMarketItemElement().GetPreviewObject());

        return !IsSpecialCase(parent);
    }

    bool BDE_MatchesFilter(string lowerFilter)
    {
        if (lowerFilter == string.Empty)
            return true;

        string className = m_ItemClassName;
        className.ToLower();
        if (className.IndexOf(lowerFilter) > -1)
            return true;

        string displayName = ExpansionStatic.GetItemDisplayNameWithType(m_ItemClassName);
        displayName.ToLower();
        return displayName.IndexOf(lowerFilter) > -1;
    }
};


modded class ExpansionMarketFilters
{
    bool BDE_IsAttachmentOfTypes(string parentClassName, string attachmentClassName, array<ExpansionMarketAttachType> attachTypes)
    {
        ExpansionMarketOutput output;
        if (IsCustomizableClothing(parentClassName))
        {
            GenerateClothingAttachmentDenom(parentClassName);
            output = m_MarketOutputs.Clothing.Get(parentClassName);
        }
        else
        {
            GenerateWeaponAttachmentDenom(parentClassName);
            output = m_MarketOutputs.Weapons.Get(parentClassName);
        }

        if (!output)
            return false;

        foreach (ExpansionMarketAttachType attachType: attachTypes)
        {
            TStringArray candidates = GetAttachmentsByType(output, attachType);
            foreach (string candidate: candidates)
            {
                if (CF_String.EqualsIgnoreCase(candidate, attachmentClassName))
                    return true;
            }
        }

        return false;
    }
};
