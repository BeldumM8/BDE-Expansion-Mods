class BDE_EMU_ClientSortSettings
{
    int Version = 1;
    int SortMode = 2;
};

class BDE_EMU_MarketWidgetEventHandler: ScriptedWidgetEventHandler
{
    protected ExpansionMarketMenu m_Menu;

    void BDE_EMU_MarketWidgetEventHandler(ExpansionMarketMenu menu)
    {
        m_Menu = menu;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (m_Menu)
            return m_Menu.OnClick(w, x, y, button);
        return false;
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (m_Menu)
            return m_Menu.OnChange(w, x, y, finished);
        return false;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (m_Menu)
            return m_Menu.OnMouseEnter(w, x, y);
        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (m_Menu)
            return m_Menu.OnMouseLeave(w, enterW, x, y);
        return false;
    }

    override bool OnMouseWheel(Widget w, int x, int y, int wheel)
    {
        if (m_Menu)
            return m_Menu.OnMouseWheel(w, x, y, wheel);
        return false;
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (m_Menu)
            return m_Menu.OnMouseButtonDown(w, x, y, button);
        return false;
    }

    override bool OnMouseButtonUp(Widget w, int x, int y, int button)
    {
        if (m_Menu)
            return m_Menu.OnMouseButtonUp(w, x, y, button);
        return false;
    }

    override bool OnItemSelected(Widget w, int x, int y, int row, int column, int oldRow, int oldColumn)
    {
        if (m_Menu)
            return m_Menu.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);
        return false;
    }
};

modded class ExpansionMarketMenuController
{
    ref ObservableCollection<ref BDE_EMU_CategoryEntry> BDECategories = new ObservableCollection<ref BDE_EMU_CategoryEntry>(this);
    ref ObservableCollection<ref BDE_EMU_ItemCard> BDEItems = new ObservableCollection<ref BDE_EMU_ItemCard>(this);
    string BDESelectedCategoryName = "SELECT A CATEGORY";
    string BDEVisibleItemCount = "0 ITEMS";
};

modded class ExpansionMarketMenu
{
    override void UpdateItemFieldFromBasicNetSync()
    {
        super.UpdateItemFieldFromBasicNetSync();

        ExpansionMarketMenuItem selectedItemElement = GetSelectedMarketItemElement();
        ExpansionMarketItem selectedMarketItem = GetSelectedMarketItem();
        if (!selectedItemElement || !selectedMarketItem || !m_MarketMenuController)
            return;

        int requiredRep = BDE_EMU_ReputationAccess.GetDisplayRequirement(selectedMarketItem, GetMarketTrader());
        if (requiredRep > 0 && !selectedItemElement.HasRepBuy())
        {
            m_MarketMenuController.MarketItemStockTrader = string.Format("%1RP", ExpansionStatic.FormatInt(requiredRep));
            m_MarketMenuController.NotifyPropertyChanged("MarketItemStockTrader");
        }

        // The buy button remains gated, but its price is always the normal cash quote.
        if (selectedItemElement.CanBuy() && !selectedItemElement.HasRepBuy())
        {
            int price = 0;
            ExpansionMarketResult result = ExpansionMarketResult.Success;
            m_MarketModule.FindPriceOfPurchaseEx(selectedMarketItem, m_MarketModule.GetClientZone(), GetMarketTrader(), PlayerBase.Cast(g_Game.GetPlayer()), m_Quantity, price, selectedItemElement.GetIncludeAttachments(), result);
            if (result != ExpansionMarketResult.IntegerOverflow)
            {
                m_BuyPrice = price;
                m_MarketMenuController.MarketItemTotalBuyPrice = GetDisplayPrice(price);
                m_MarketMenuController.CurrencyBuyIcon = GetExpansionSettings().GetMarket().CurrencyIcon;
                m_MarketMenuController.NotifyPropertiesChanged({"MarketItemTotalBuyPrice", "CurrencyBuyIcon"});
                market_item_buy_price_icon.Show(true);
            }
        }
    }

    protected ExpansionMarketMenuCategory m_BDESelectedCategory;
    protected ref BDE_EMU_VehiclePreviewRenderer m_BDEVehiclePreview;
    protected bool m_BDEInterfaceInitialized;
    protected string m_BDECategorySignature;
    protected string m_BDEItemSignature;
    protected float m_BDESyncAccumulator;
    protected bool m_BDEClosing;
    protected ref BDE_EMU_MarketWidgetEventHandler m_BDEWidgetEventHandler;
    protected Widget m_BDEMenuInfoBackground;
    protected Widget m_BDEMarketItemHeaderPanel;
    protected bool m_BDEAttachmentMode;
    protected string m_BDESavedMarketSearch;
    protected ExpansionItemTooltip m_BDEItemTooltip;
    protected Widget m_BDEBuyActionArea;
    protected Widget m_BDESellActionArea;
    protected TextWidget m_BDEBuyLabel;
    protected TextWidget m_BDESellLabel;
    protected int m_BDESortMode = 2;
    protected bool m_BDESortPreferenceLoaded;

    // Embedded attachment mode reuses the exact normal BDE category/item
    // collections and card classes. Expansion's item manager is only the data
    // and compatibility engine and remains hidden.
    protected ExpansionMarketMenuCategory m_BDESavedSelectedCategory;
    protected ExpansionMarketMenuItem m_BDEAttachmentParentItemElement;
    protected ExpansionMarketMenuItemManagerCategory m_BDESelectedAttachmentCategory;
    protected ref array<ref ExpansionMarketMenuItem> m_BDEAttachmentTemporaryItems = new array<ref ExpansionMarketMenuItem>;
    protected TextWidget m_BDESidebarHeaderText;
    protected TextWidget m_BDEAttachmentSetupLabel;
    protected bool m_BDEAttachmentRebuildGuard;

    // Player item editor mode. Category 0 lists configurable inventory items;
    // category 1 lists market-available magazines/ammunition for carried weapons.
    protected static const int BDE_EDITOR_CATEGORY_INVENTORY = 0;
    protected static const int BDE_EDITOR_CATEGORY_AMMO = 1;
    protected bool m_BDEItemEditorMode;
    protected int m_BDEItemEditorCategory = BDE_EDITOR_CATEGORY_INVENTORY;
    protected bool m_BDEAttachmentFromItemEditor;
    // Inventory weapons use Expansion's actual item-manager view instead of the
    // embedded BDE attachment cards. Clothing/container items keep the BDE
    // multi-slot editor because it supports several copies of the same class.
    protected bool m_BDEVanillaInventoryWeaponManager;
    protected string m_BDEItemEditorSavedMarketSearch;
    protected ExpansionMarketMenuCategory m_BDEItemEditorSavedSelectedMarketCategory;
    protected string m_BDESavedItemEditorSearch;
    protected EntityAI m_BDEInventoryTarget;
    protected ref TStringArray m_BDEInventoryOriginalAttachments = new TStringArray;
    protected ref array<ref ExpansionMarketMenuItem> m_BDEItemEditorTemporaryItems = new array<ref ExpansionMarketMenuItem>;
    protected ref ExpansionMarketMenuItem m_BDEInventorySelectedItemElement;
    protected ref ExpansionMarketItem m_BDEInventorySelectedMarketItem;
    protected bool m_BDEInventoryAttachmentRequestPending;
    protected ButtonWidget m_BDEItemEditorButton;
    protected TextWidget m_BDEItemEditorButtonText;
    protected Widget m_BDESellablesPanel;
    protected Widget m_BDEPurchasablesPanel;
    protected ScrollWidget m_BDEItemsScroller;

    // Concrete handlers for the replacement layout. Expansion normally uses
    // ViewBinding relay commands; these references let the BDE layout dispatch
    // the critical controls directly as well, so interaction does not depend on
    // relay ownership after the original category tree is hidden offscreen.
    protected ButtonWidget m_BDEBuyButton;
    protected ButtonWidget m_BDESellButton;
    protected ButtonWidget m_BDEQuantityButton;
    protected ButtonWidget m_BDEQuantityDecreaseButton;
    protected ButtonWidget m_BDEQuantityIncreaseButton;

    override string GetLayoutFile()
    {
        return "BDE_ExpansionMarket_UI/gui/layouts/BDE_EMU_Market.layout";
    }

    void ExpansionMarketMenu()
    {
#ifdef BDE_HelicopterSling
        BDE_EMU_SlingInfoModule.Changed.Insert(BDE_OnSlingInfo);
#endif
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_InitializeInterface, 0, false, 0);
    }

#ifdef BDE_HelicopterSling
    void ~ExpansionMarketMenu()
    {
        BDE_EMU_SlingInfoModule.Changed.Remove(BDE_OnSlingInfo);
    }
#endif

    protected void BDE_InitializeInterface(int attempt)
    {
        if (m_BDEInterfaceInitialized)
            return;

        if (!GetLayoutRoot() || !m_MarketMenuController || !market_menu_content || !market_menu_info)
        {
            if (attempt < 30)
                g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_InitializeInterface, 50, false, attempt + 1);
            return;
        }

        Widget vehicleViewport;
        if (market_item_preview)
            vehicleViewport = market_item_preview.GetParent();
        if (vehicleViewport)
            m_BDEVehiclePreview = new BDE_EMU_VehiclePreviewRenderer(vehicleViewport);

        Widget categoryToggle = GetLayoutRoot().FindAnyWidget("market_footer_categories_toggle");
        if (categoryToggle)
            categoryToggle.Show(false);

        BDE_InstallConcreteMenuHandlers();

        m_BDEMenuInfoBackground = GetLayoutRoot().FindAnyWidget("menu_info_background");
        m_BDEMarketItemHeaderPanel = GetLayoutRoot().FindAnyWidget("market_item_header_panel");
        m_BDESidebarHeaderText = TextWidget.Cast(GetLayoutRoot().FindAnyWidget("bde_sidebar_header_text"));
        m_BDEAttachmentSetupLabel = TextWidget.Cast(GetLayoutRoot().FindAnyWidget("market_item_info_attachments_setup_lable"));
        m_BDESellablesPanel = GetLayoutRoot().FindAnyWidget("market_item_sellables_panel");
        m_BDEPurchasablesPanel = GetLayoutRoot().FindAnyWidget("market_item_purchasables_panel");
        m_BDEItemsScroller = ScrollWidget.Cast(GetLayoutRoot().FindAnyWidget("bde_items_scroller"));
        m_BDEAttachmentMode = false;
        m_BDEItemEditorMode = false;
        ExpansionMarketModule.SI_BDEInventoryAttachmentEditResult.Insert(BDE_OnInventoryAttachmentEditResult);

        BDE_LoadSortPreference();
        BDE_UpdateTransactionLabels();

        m_BDEClosing = false;
        m_BDEInterfaceInitialized = true;
        Print("[BDE Market UI] v1.9.0 item editor and transaction-state pricing initialized");
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_ForceSynchronizeInterface, 100, false);
    }

    protected void BDE_InstallConcreteMenuHandlers()
    {
        if (!m_BDEWidgetEventHandler)
            m_BDEWidgetEventHandler = new BDE_EMU_MarketWidgetEventHandler(this);

        if (market_filter_box)
            market_filter_box.SetHandler(m_BDEWidgetEventHandler);
        if (market_filter_clear)
            market_filter_clear.SetHandler(m_BDEWidgetEventHandler);
        if (sort_item_name_button)
            sort_item_name_button.SetHandler(m_BDEWidgetEventHandler);
        if (sort_item_price_button)
            sort_item_price_button.SetHandler(m_BDEWidgetEventHandler);
        m_BDEItemEditorButton = ButtonWidget.Cast(GetLayoutRoot().FindAnyWidget("bde_item_editor_button"));
        m_BDEItemEditorButtonText = TextWidget.Cast(GetLayoutRoot().FindAnyWidget("bde_item_editor_button_text"));
        if (m_BDEItemEditorButton)
            m_BDEItemEditorButton.SetHandler(m_BDEWidgetEventHandler);
        if (market_footer_close_menu_button)
            market_footer_close_menu_button.SetHandler(m_BDEWidgetEventHandler);
        if (market_item_info_quantity_button)
            market_item_info_quantity_button.SetHandler(m_BDEWidgetEventHandler);
        if (market_item_info_attachments_setup_button)
            market_item_info_attachments_setup_button.SetHandler(m_BDEWidgetEventHandler);
        if (dropdown_selector)
            dropdown_selector.SetHandler(m_BDEWidgetEventHandler);
        if (skin_selector)
            skin_selector.SetHandler(m_BDEWidgetEventHandler);
        if (market_item_preview)
            market_item_preview.SetHandler(m_BDEWidgetEventHandler);
        if (market_player_preview)
            market_player_preview.SetHandler(m_BDEWidgetEventHandler);

        m_BDEBuyButton = ButtonWidget.Cast(GetLayoutRoot().FindAnyWidget("market_item_buy_button"));
        m_BDESellButton = ButtonWidget.Cast(GetLayoutRoot().FindAnyWidget("market_item_sell_button"));
        m_BDEBuyActionArea = GetLayoutRoot().FindAnyWidget("market_item_buy");
        m_BDESellActionArea = GetLayoutRoot().FindAnyWidget("market_item_sell");
        m_BDEBuyLabel = TextWidget.Cast(GetLayoutRoot().FindAnyWidget("market_item_buy_text"));
        m_BDESellLabel = TextWidget.Cast(GetLayoutRoot().FindAnyWidget("market_item_sell_text"));
        if (m_BDEBuyLabel)
            m_BDEBuyLabel.SetSort(10000);
        if (m_BDESellLabel)
            m_BDESellLabel.SetSort(10000);
        m_BDEQuantityButton = ButtonWidget.Cast(GetLayoutRoot().FindAnyWidget("market_item_amount_button"));
        m_BDEQuantityDecreaseButton = ButtonWidget.Cast(GetLayoutRoot().FindAnyWidget("market_item_amount_button_left"));
        m_BDEQuantityIncreaseButton = ButtonWidget.Cast(GetLayoutRoot().FindAnyWidget("market_item_amount_button_right"));

        if (m_BDEBuyButton)
        {
            m_BDEBuyButton.SetHandler(m_BDEWidgetEventHandler);
            m_BDEBuyButton.SetText("BUY");
        }
        if (m_BDESellButton)
        {
            m_BDESellButton.SetHandler(m_BDEWidgetEventHandler);
            m_BDESellButton.SetText("SELL");
        }
        if (m_BDEQuantityButton)
            m_BDEQuantityButton.SetHandler(m_BDEWidgetEventHandler);
        if (m_BDEQuantityDecreaseButton)
            m_BDEQuantityDecreaseButton.SetHandler(m_BDEWidgetEventHandler);
        if (m_BDEQuantityIncreaseButton)
            m_BDEQuantityIncreaseButton.SetHandler(m_BDEWidgetEventHandler);

        BDE_UpdateTransactionLabels();
    }

    protected void BDE_UpdateTransactionLabels()
    {
        // These labels are intentionally independent of Expansion's action-panel
        // visibility. Expansion hides the actual button when the player cannot
        // currently afford/buy/sell, but the UI should still identify the two
        // price/action columns as BUY and SELL.
        if (m_BDEBuyLabel)
        {
            m_BDEBuyLabel.SetText("BUY");
            m_BDEBuyLabel.SetSort(10000);
            m_BDEBuyLabel.SetAlpha(1.0);
            m_BDEBuyLabel.Show(true);
        }

        if (m_BDESellLabel)
        {
            m_BDESellLabel.SetText("SELL");
            m_BDESellLabel.SetSort(10000);
            m_BDESellLabel.SetAlpha(1.0);
            m_BDESellLabel.Show(true);
        }
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (button != MouseState.LEFT)
            return super.OnClick(w, x, y, button);

        if (w == market_filter_clear)
        {
            if (m_BDEAttachmentMode)
            {
                market_filter_box.SetText("");
                BDE_RebuildAttachmentItems();
            }
            else if (m_BDEItemEditorMode)
            {
                market_filter_box.SetText("");
                BDE_RebuildItemEditorItems();
            }
            else
            {
                OnFilterButtonClick();
            }
            return true;
        }
        if (w == sort_item_name_button)
        {
            OnNameSortClick();
            return true;
        }
        if (w == sort_item_price_button)
        {
            OnPriceSortClick();
            return true;
        }
        if (w == m_BDEItemEditorButton)
        {
            if (m_BDEItemEditorMode)
                BDE_ExitItemEditorMode();
            else
                BDE_EnterItemEditorMode();
            return true;
        }
        if (w == market_footer_close_menu_button)
        {
            OnCloseMenuButtonClick();
            return true;
        }
        if (w == m_BDEBuyButton)
        {
            if ((m_BDEAttachmentMode && m_BDEAttachmentFromItemEditor) || BDE_HasPendingVanillaInventoryWeaponEdit())
                BDE_ApplyInventoryAttachmentEdit();
            else
                BDE_BuyWithoutConfirmation();
            return true;
        }
        if (w == m_BDESellButton)
        {
            if (!(m_BDEAttachmentMode && m_BDEAttachmentFromItemEditor))
                BDE_SellWithoutConfirmation();
            return true;
        }
        if (w == m_BDEQuantityButton || w == market_item_info_quantity_button)
        {
            OnQuantityButtonClick();
            return true;
        }
        if (w == m_BDEQuantityDecreaseButton)
        {
            OnDecrementQuantityButtonClick();
            return true;
        }
        if (w == m_BDEQuantityIncreaseButton)
        {
            OnIncrementQuantityButtonClick();
            return true;
        }
        if (w == market_item_info_attachments_setup_button)
        {
            OnOpenItemManagerButtonClick();
            return true;
        }

        return super.OnClick(w, x, y, button);
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (w == market_filter_box && m_BDEAttachmentMode)
        {
            BDE_RebuildAttachmentItems();
            return true;
        }

        if (w == market_filter_box && m_BDEItemEditorMode)
        {
            BDE_RebuildItemEditorItems();
            return true;
        }

        return super.OnChange(w, x, y, finished);
    }

    override void CreateMenuCategories()
    {
        super.CreateMenuCategories();
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_ForceSynchronizeInterface, 50, false);
    }

    override void UpdateMarketCategories(bool updateItemViews = false)
    {
        super.UpdateMarketCategories(updateItemViews);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_ForceSynchronizeInterface, 25, false);
    }

    protected void BDE_ForceSynchronizeInterface()
    {
        m_BDECategorySignature = "#FORCE#";
        m_BDEItemSignature = "#FORCE#";
        BDE_SynchronizeCategories();
        BDE_SynchronizeItems();
    }

    protected string BDE_BuildCategorySignature()
    {
        string signature;
        for (int i = 0; i < m_MarketMenuController.MarketCategories.Count(); i++)
        {
            ExpansionMarketMenuCategory category = m_MarketMenuController.MarketCategories[i];
            if (!category || !category.IsVisible())
                continue;

            ExpansionMarketCategory categoryData = category.GetCategory();
            if (!categoryData)
                continue;
            signature += categoryData.CategoryID.ToString() + ":" + category.GetShowItemsCount().ToString() + ";";
        }
        return signature;
    }

    protected void BDE_SynchronizeCategories()
    {
        if (!m_BDEInterfaceInitialized || !m_MarketMenuController)
            return;

        if (m_BDEAttachmentMode)
        {
            BDE_RefreshAttachmentCategoryEntries();
            return;
        }

        if (m_BDEItemEditorMode)
        {
            BDE_RefreshItemEditorCategoryEntries();
            return;
        }

        string signature = BDE_BuildCategorySignature();
        if (signature == m_BDECategorySignature)
        {
            BDE_RefreshCategoryEntries();
            return;
        }

        int selectedCategoryID = -1;
        if (m_BDESelectedCategory && m_BDESelectedCategory.GetCategory())
            selectedCategoryID = m_BDESelectedCategory.GetCategory().CategoryID;

        BDE_DestroyCategoryEntries();
        m_BDECategorySignature = signature;

        ExpansionMarketMenuCategory firstVisible;
        ExpansionMarketMenuCategory selectedCategory;

        for (int i = 0; i < m_MarketMenuController.MarketCategories.Count(); i++)
        {
            ExpansionMarketMenuCategory category = m_MarketMenuController.MarketCategories[i];
            if (!category || !category.IsVisible())
                continue;

            if (!firstVisible)
                firstVisible = category;
            if (category.GetCategory().CategoryID == selectedCategoryID)
                selectedCategory = category;

            BDE_EMU_CategoryEntry entry = new BDE_EMU_CategoryEntry(this, category);
            m_MarketMenuController.BDECategories.Insert(entry);
        }

        if (!selectedCategory)
            selectedCategory = firstVisible;

        if (selectedCategory)
            BDE_SelectCategorySource(selectedCategory);
        else
        {
            m_BDESelectedCategory = null;
            BDE_SetSelectedCategoryText(string.Empty);
            BDE_DestroyItemCards();
        }
    }

    protected void BDE_RefreshCategoryEntries()
    {
        for (int i = 0; i < m_MarketMenuController.BDECategories.Count(); i++)
        {
            BDE_EMU_CategoryEntry entry = m_MarketMenuController.BDECategories[i];
            if (!entry)
                continue;
            entry.Refresh();
            entry.SetSelected(entry.GetSourceCategory() == m_BDESelectedCategory);
        }
    }

    protected void BDE_ScrollItemsToTop()
    {
        if (m_BDEItemsScroller)
            m_BDEItemsScroller.VScrollToPos(0);
    }

    protected void BDE_SelectFirstItemCard()
    {
        if (!m_MarketMenuController || m_BDEAttachmentMode || m_MarketMenuController.BDEItems.Count() == 0)
            return;

        BDE_EMU_ItemCard firstCard = m_MarketMenuController.BDEItems[0];
        if (firstCard)
            firstCard.SelectItem();
    }

    void BDE_SelectCategorySource(ExpansionMarketMenuCategory category)
    {
        if (!category)
            return;

        bool categoryChanged = m_BDESelectedCategory != category;
        if (categoryChanged)
        {
            BDE_ScrollItemsToTop();
            m_BDEItemSignature = "#FORCE#";
            BDE_DestroyItemCards();
            BDE_SetVisibleItemCount(0);
        }

        for (int i = 0; i < m_MarketMenuController.MarketCategories.Count(); i++)
        {
            ExpansionMarketMenuCategory current = m_MarketMenuController.MarketCategories[i];
            if (!current)
                continue;

            if (current == category)
            {
                if (!current.IsExpanded())
                    current.ToggleCategory(true, true);
            }
            else if (current.IsExpanded())
            {
                current.ToggleCategory(false);
            }
        }

        m_BDESelectedCategory = category;
        BDE_SetSelectedCategoryText(category.GetCategoryController().CategoryName);
        BDE_RefreshCategoryEntries();
        BDE_SynchronizeItems();
        if (categoryChanged)
            BDE_SelectFirstItemCard();
    }

    protected void BDE_SetSelectedCategoryText(string categoryName)
    {
        m_MarketMenuController.BDESelectedCategoryName = categoryName;
        m_MarketMenuController.NotifyPropertyChanged("BDESelectedCategoryName");
    }

    protected string BDE_BuildItemSignature()
    {
        if (!m_BDESelectedCategory)
            return string.Empty;

        string signature;
        auto items = m_BDESelectedCategory.GetItems();
        for (int i = 0; i < items.Count(); i++)
        {
            ExpansionMarketMenuItem item = items[i];
            if (!item || !item.IsVisible())
                continue;

            ExpansionMarketItem baseItem = item.GetBaseItem();
            ExpansionMarketItem marketItem = item.GetMarketItem();
            if (!baseItem || !baseItem.m_ShowInMenu || !marketItem)
                continue;

            signature += marketItem.ClassName + ":" + item.GetBuyPrice().ToString() + ";";
        }
        return signature;
    }

    protected void BDE_SynchronizeItems()
    {
        if (!m_BDEInterfaceInitialized || !m_MarketMenuController)
            return;

        if (m_BDEAttachmentMode)
        {
            BDE_RefreshAttachmentCards();
            return;
        }

        if (m_BDEItemEditorMode)
        {
            BDE_RefreshItemEditorCards();
            return;
        }

        string signature = BDE_BuildItemSignature();
        if (signature == m_BDEItemSignature)
        {
            BDE_RefreshItemCards();
            return;
        }

        BDE_DestroyItemCards();
        m_BDEItemSignature = signature;

        if (!m_BDESelectedCategory)
        {
            BDE_SetVisibleItemCount(0);
            return;
        }

        int visibleCount;
        auto items = m_BDESelectedCategory.GetItems();
        for (int i = 0; i < items.Count(); i++)
        {
            ExpansionMarketMenuItem item = items[i];
            if (!item || !item.IsVisible())
                continue;

            ExpansionMarketItem baseItem = item.GetBaseItem();
            if (!baseItem || !baseItem.m_ShowInMenu)
                continue;

            BDE_EMU_ItemCard card = new BDE_EMU_ItemCard(this, item);
            m_MarketMenuController.BDEItems.Insert(card);
            visibleCount++;
        }

        BDE_SetVisibleItemCount(visibleCount);
        BDE_UpdateSelectedCardState();
    }

    protected void BDE_RefreshItemCards()
    {
        for (int i = 0; i < m_MarketMenuController.BDEItems.Count(); i++)
        {
            BDE_EMU_ItemCard card = m_MarketMenuController.BDEItems[i];
            if (card)
                card.Refresh();
        }
        BDE_UpdateSelectedCardState();
    }

    protected void BDE_SetVisibleItemCount(int count)
    {
        m_MarketMenuController.BDEVisibleItemCount = count.ToString() + " ITEMS";
        m_MarketMenuController.NotifyPropertyChanged("BDEVisibleItemCount");
    }

    void BDE_SelectMarketItem(ExpansionMarketMenuItem item)
    {
        BDE_HideItemCardTooltip();

        if (m_BDEItemEditorMode && !m_BDEAttachmentMode)
            BDE_ClearInventoryEditorSelection();

        if (!item)
            return;

        ExpansionMarketItem marketItem = item.GetMarketItem();
        if (!marketItem)
            return;

        SetItemInfo(item);
        if (!IsLoading())
            RequestSelectedItem(ExpansionMarketMenuState.REQUESTING_SELECTED_ITEM, marketItem.ClassName);
        BDE_UpdateSelectedCardState();
    }

    void BDE_BuyWithoutConfirmation()
    {
        BDE_HideItemCardTooltip();
        OnConfirmBuyButtonClick();
    }

    void BDE_SellWithoutConfirmation()
    {
        BDE_HideItemCardTooltip();

        ExpansionMarketMenuItem selectedItem = GetSelectedMarketItemElement();
        if (!selectedItem || !selectedItem.BDE_CanFastSell())
            return;

        OnConfirmSellButtonClick();
    }

    void BDE_BuyMarketItemFromCard(ExpansionMarketMenuItem item)
    {
        BDE_HideItemCardTooltip();

        if (!item || !item.BDE_CanFastBuy())
            return;

        SetItemInfo(item);
        BDE_BuyWithoutConfirmation();
    }

    void BDE_SellMarketItemFromCard(ExpansionMarketMenuItem item)
    {
        BDE_HideItemCardTooltip();

        if (!item || !item.BDE_CanFastSell())
            return;

        SetItemInfo(item);
        BDE_SellWithoutConfirmation();
    }

    protected void BDE_UpdateSelectedCardState()
    {
        for (int i = 0; i < m_MarketMenuController.BDEItems.Count(); i++)
        {
            BDE_EMU_ItemCard card = m_MarketMenuController.BDEItems[i];
            if (!card)
                continue;

            if (m_BDEItemEditorMode && m_BDEInventoryTarget && card.BDE_IsInventoryItemCard())
                card.SetSelected(card.BDE_GetInventoryItem() == m_BDEInventoryTarget);
            else
                card.SetSelected(card.GetSourceItem() == m_SelectedMarketItemElement);
        }
    }

    override void UpdateItemSkins()
    {
        // Inventory editor entries are synthetic market items that represent a
        // specific live entity, not a purchasable skin/variant selection. Running
        // Expansion's skin dropdown on them can look up a class that is not a
        // global market item and pass NULL into CreatePreviewObjectEx.
        if (m_BDEItemEditorMode && m_BDEInventorySelectedItemElement && GetSelectedMarketItemElement() == m_BDEInventorySelectedItemElement)
        {
            ClearSkins();
            if (skin_selector)
                skin_selector.Show(false);
            return;
        }

        super.UpdateItemSkins();
    }

    override void SetItemInfo(ExpansionMarketMenuItem itemElement, bool resetQuantity = true)
    {
        super.SetItemInfo(itemElement, resetQuantity);
        BDE_UpdateSelectedCardState();

        bool isVehicle;
        if (m_SelectedMarketItem)
            isVehicle = m_SelectedMarketItem.IsVehicle();

        BDE_SetVehicleInfoTransparency(isVehicle);

        if (isVehicle && m_SelectedMarketItem)
        {
            EntityAI vehiclePreview;
            if (m_BDEVehiclePreview)
                vehiclePreview = m_BDEVehiclePreview.GetPreviewEntity();
            if (!vehiclePreview && itemElement)
                vehiclePreview = EntityAI.Cast(itemElement.GetPreviewObject());

            string vehicleInfo = BDE_BuildVehicleInformation(m_SelectedMarketItem.ClassName, vehiclePreview);
            if (vehicleInfo != string.Empty)
            {
                m_MarketMenuController.MarketItemDesc = vehicleInfo;
                m_MarketMenuController.NotifyPropertyChanged("MarketItemDesc");
                UpdateItemDescriptionPanel();
            }
        }

        BDE_UpdateTransactionLabels();

        if (m_BDEItemEditorMode && !m_BDEAttachmentMode && market_item_info_attachments_setup_button)
            market_item_info_attachments_setup_button.Enable(m_BDEInventoryTarget != null);

        if (m_MarketMenuItemManager && m_MarketMenuItemManager.IsVisible() && m_SelectedMarketItem)
        {
            m_SelectedMarketItem.m_PreviewEntity = null;
            m_MarketMenuItemManager.SetMarketItem(m_SelectedMarketItem);
        }
    }

    override void UpdatePreview()
    {
        if (m_SelectedMarketItem && m_SelectedMarketItem.IsVehicle() && m_BDEVehiclePreview)
        {
            if (market_item_preview)
                market_item_preview.Show(false);
            if (market_player_preview)
                market_player_preview.Show(false);

            int skinIndex = -1;
            if (m_SelectedMarketItemElement)
                skinIndex = m_SelectedMarketItemElement.GetCurrentSelectedSkinIndex();

            m_BDEVehiclePreview.Build(m_SelectedMarketItem, GetPreviewClassName(m_SelectedMarketItem.ClassName), skinIndex);
            g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_ReassertMenuControlLock, 0, false);
            return;
        }

        if (m_BDEVehiclePreview)
            m_BDEVehiclePreview.ClearVehicle(true);

        super.UpdatePreview();
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_ReassertMenuControlLock, 0, false);
    }

    protected void BDE_ReassertMenuControlLock()
    {
        if (!IsVisible())
            return;
        LockControls();
        SetFocus(GetLayoutRoot());
    }

    void BDE_RefreshVehiclePreview()
    {
        if (m_SelectedMarketItem && m_SelectedMarketItem.IsVehicle())
            UpdatePreview();
    }

    protected void BDE_EnterItemEditorMode()
    {
        if (m_BDEAttachmentMode || !m_MarketMenuController || !m_MarketModule || !GetTraderObject())
            return;

        BDE_HideItemCardTooltip();
        m_BDEItemEditorSavedMarketSearch = market_filter_box.GetText();
        m_BDEItemEditorSavedSelectedMarketCategory = m_BDESelectedCategory;
        m_BDEItemEditorCategory = BDE_EDITOR_CATEGORY_INVENTORY;
        m_BDEItemEditorMode = true;
        m_BDESelectedCategory = null;
        m_BDEInventoryAttachmentRequestPending = false;

        market_filter_box.SetText("");
        if (m_BDESidebarHeaderText)
            m_BDESidebarHeaderText.SetText("ITEM EDITOR");
        if (m_BDEItemEditorButtonText)
            m_BDEItemEditorButtonText.SetText("Back To Market");
        if (m_BDEAttachmentSetupLabel)
            m_BDEAttachmentSetupLabel.SetText("CONFIGURE ATTACHMENTS");
        if (market_item_info_attachments_setup_button)
            market_item_info_attachments_setup_button.Enable(false);
        if (m_BDESellablesPanel)
            m_BDESellablesPanel.Show(false);
        if (m_BDEPurchasablesPanel)
            m_BDEPurchasablesPanel.Show(false);

        BDE_DestroyItemCards();
        BDE_DestroyCategoryEntries();
        BDE_DestroyItemEditorTemporaryItems();
        BDE_ClearInventoryEditorSelection();
        BDE_RebuildItemEditorCategories();
    }

    protected void BDE_ExitItemEditorMode()
    {
        if (!m_BDEItemEditorMode)
            return;

        BDE_HideItemCardTooltip();

        if (m_BDEVanillaInventoryWeaponManager && m_MarketMenuItemManager)
        {
            m_BDEVanillaInventoryWeaponManager = false;
            m_MarketMenuItemManager.BDE_SetVanillaInventoryWeaponMode(false);
            if (m_MarketMenuItemManager.IsVisible())
                m_MarketMenuItemManager.Hide();
        }

        if (m_BDEAttachmentMode && m_BDEAttachmentFromItemEditor)
        {
            m_BDEAttachmentRebuildGuard = true;
            BDE_DestroyItemCards();
            BDE_DestroyCategoryEntries();
            BDE_DestroyAttachmentTemporaryItems();
            if (m_MarketMenuItemManager)
                m_MarketMenuItemManager.BDE_ClearEmbeddedAttachmentData();
            m_BDEAttachmentRebuildGuard = false;
            m_BDEAttachmentMode = false;
            m_BDEAttachmentFromItemEditor = false;
            m_BDESelectedAttachmentCategory = null;
            m_BDEAttachmentParentItemElement = null;
        }

        BDE_DestroyItemCards();
        BDE_DestroyCategoryEntries();
        BDE_DestroyItemEditorTemporaryItems();
        BDE_ClearInventoryEditorSelection();

        m_BDEItemEditorMode = false;
        m_BDESelectedCategory = m_BDEItemEditorSavedSelectedMarketCategory;
        m_BDEItemEditorSavedSelectedMarketCategory = null;
        market_filter_box.SetText(m_BDEItemEditorSavedMarketSearch);

        if (m_BDESidebarHeaderText)
            m_BDESidebarHeaderText.SetText("MARKET CATEGORIES");
        if (m_BDEItemEditorButtonText)
            m_BDEItemEditorButtonText.SetText("Inv Item Attachments");
        if (m_BDEAttachmentSetupLabel)
            m_BDEAttachmentSetupLabel.SetText("CONFIGURE ATTACHMENTS");
        if (market_item_info_attachments_setup_button)
            market_item_info_attachments_setup_button.Enable(true);
        if (m_BDESellablesPanel)
            m_BDESellablesPanel.Show(true);
        if (m_BDEPurchasablesPanel)
            m_BDEPurchasablesPanel.Show(true);

        market_menu_content.Show(true);
        market_menu_info.Show(true);
        market_item_preview.Show(true);
        BDE_ForceSynchronizeInterface();
    }

    protected void BDE_RebuildItemEditorCategories()
    {
        if (!m_BDEItemEditorMode || m_BDEAttachmentMode || !m_MarketMenuController)
            return;

        BDE_DestroyItemCards();
        BDE_DestroyCategoryEntries();
        BDE_DestroyItemEditorTemporaryItems();
        BDE_ClearInventoryEditorSelection();

        int inventoryCount = BDE_CountEditableInventoryItems();
        TStringArray ammoClasses = new TStringArray;
        BDE_CollectAvailableAmmoClasses(ammoClasses);

        BDE_EMU_CategoryEntry inventoryCategory = new BDE_EMU_CategoryEntry(this, null);
        inventoryCategory.BDE_SetEditorCategorySource(BDE_EDITOR_CATEGORY_INVENTORY, "INV ITEMS", "set:dayz_inventory image:shoulderright", inventoryCount);
        m_MarketMenuController.BDECategories.Insert(inventoryCategory);

        BDE_EMU_CategoryEntry ammoCategory = new BDE_EMU_CategoryEntry(this, null);
        ammoCategory.BDE_SetEditorCategorySource(BDE_EDITOR_CATEGORY_AMMO, "AMMO", "set:dayz_inventory image:magazine", ammoClasses.Count());
        m_MarketMenuController.BDECategories.Insert(ammoCategory);

        BDE_SelectEditorCategorySource(m_BDEItemEditorCategory);
    }

    void BDE_SelectEditorCategorySource(int categoryID)
    {
        if (!m_BDEItemEditorMode || m_BDEAttachmentMode)
            return;

        if (categoryID != BDE_EDITOR_CATEGORY_INVENTORY && categoryID != BDE_EDITOR_CATEGORY_AMMO)
            categoryID = BDE_EDITOR_CATEGORY_INVENTORY;

        bool editorCategoryChanged = m_BDEItemEditorCategory != categoryID;
        if (editorCategoryChanged)
            BDE_ScrollItemsToTop();

        m_BDEItemEditorCategory = categoryID;
        BDE_ClearInventoryEditorSelection();
        if (market_item_info_attachments_setup_button)
            market_item_info_attachments_setup_button.Enable(false);

        if (categoryID == BDE_EDITOR_CATEGORY_INVENTORY)
            BDE_SetSelectedCategoryText("INV ITEMS");
        else
            BDE_SetSelectedCategoryText("AMMO");

        BDE_RefreshItemEditorCategoryEntries();
        BDE_RebuildItemEditorItems();
        BDE_SelectFirstItemCard();
    }

    protected void BDE_RefreshItemEditorCategoryEntries()
    {
        if (!m_MarketMenuController)
            return;

        for (int i = 0; i < m_MarketMenuController.BDECategories.Count(); i++)
        {
            BDE_EMU_CategoryEntry entry = m_MarketMenuController.BDECategories[i];
            if (!entry || !entry.BDE_IsEditorCategory())
                continue;

            entry.Refresh();
            entry.SetSelected(entry.BDE_GetEditorCategoryID() == m_BDEItemEditorCategory);
        }
    }

    protected void BDE_RebuildItemEditorItems()
    {
        if (!m_BDEItemEditorMode || m_BDEAttachmentMode || !m_MarketMenuController)
            return;

        BDE_DestroyItemCards();
        BDE_DestroyItemEditorTemporaryItems();
        BDE_ClearInventoryEditorSelection();

        if (m_BDEItemEditorCategory == BDE_EDITOR_CATEGORY_AMMO)
            BDE_RebuildItemEditorAmmoItems();
        else
            BDE_RebuildItemEditorInventoryItems();
    }

    protected void BDE_RebuildItemEditorInventoryItems()
    {
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player || !m_MarketModule)
        {
            BDE_SetVisibleItemCount(0);
            return;
        }

        string filterText = market_filter_box.GetText();
        filterText.ToLower();

        ref array<EntityAI> sortedItems = new array<EntityAI>;
        array<EntityAI> inventory = m_MarketModule.LocalGetEntityInventory();
        foreach (EntityAI item: inventory)
        {
            if (!BDE_IsEditableInventoryItem(item))
                continue;

            string typeLower = item.GetType();
            typeLower.ToLower();
            string displayLower = ExpansionStatic.GetItemDisplayNameWithType(item.GetType());
            displayLower.ToLower();
            if (filterText != string.Empty && typeLower.IndexOf(filterText) == -1 && displayLower.IndexOf(filterText) == -1)
                continue;

            BDE_InsertInventoryItemSorted(sortedItems, item);
        }

        int visibleCount;
        foreach (EntityAI inventoryItem: sortedItems)
        {
            ExpansionMarketMenuItem sourceItem = BDE_CreateItemEditorMarketItemElement(inventoryItem.GetType());
            if (!sourceItem)
                continue;

            BDE_EMU_ItemCard card = new BDE_EMU_ItemCard(this, sourceItem);
            card.BDE_SetInventoryItemSource(inventoryItem);
            m_MarketMenuController.BDEItems.Insert(card);
            visibleCount++;
        }

        m_MarketMenuController.BDEVisibleItemCount = visibleCount.ToString() + " INVENTORY ITEMS";
        m_MarketMenuController.NotifyPropertyChanged("BDEVisibleItemCount");
        BDE_UpdateSelectedCardState();
    }

    protected void BDE_RebuildItemEditorAmmoItems()
    {
        TStringArray ammoClasses = new TStringArray;
        BDE_CollectAvailableAmmoClasses(ammoClasses);

        string filterText = market_filter_box.GetText();
        filterText.ToLower();

        ref array<string> sortedClasses = new array<string>;
        foreach (string ammoClass: ammoClasses)
        {
            string typeLower = ammoClass;
            typeLower.ToLower();
            string displayLower = ExpansionStatic.GetItemDisplayNameWithType(ammoClass);
            displayLower.ToLower();
            if (filterText != string.Empty && typeLower.IndexOf(filterText) == -1 && displayLower.IndexOf(filterText) == -1)
                continue;

            BDE_InsertAmmoClassSorted(sortedClasses, ammoClass);
        }

        int visibleCount;
        foreach (string sortedClass: sortedClasses)
        {
            ExpansionMarketMenuItem sourceItem = BDE_CreateItemEditorMarketItemElement(sortedClass, false);
            if (!sourceItem)
                continue;

            BDE_EMU_ItemCard card = new BDE_EMU_ItemCard(this, sourceItem);
            m_MarketMenuController.BDEItems.Insert(card);
            visibleCount++;
        }

        m_MarketMenuController.BDEVisibleItemCount = visibleCount.ToString() + " AMMO ITEMS";
        m_MarketMenuController.NotifyPropertyChanged("BDEVisibleItemCount");
        BDE_UpdateSelectedCardState();
    }

    protected void BDE_RefreshItemEditorCards()
    {
        if (!m_MarketMenuController)
            return;

        for (int i = 0; i < m_MarketMenuController.BDEItems.Count(); i++)
        {
            BDE_EMU_ItemCard card = m_MarketMenuController.BDEItems[i];
            if (card)
                card.Refresh();
        }

        BDE_UpdateSelectedCardState();
    }

    protected bool BDE_IsEditableInventoryItem(EntityAI item)
    {
        if (!item || item.IsPendingDeletion())
            return false;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player || item.GetHierarchyRootPlayer() != player)
            return false;

        int low, high;
        item.GetNetworkID(low, high);
        if (low == 0 && high == 0)
            return false;

        ExpansionMarketFilters filters = GetMarketFilters();
        if (!filters)
            return false;

        ref array<ExpansionMarketAttachType> attachTypes = new array<ExpansionMarketAttachType>;
        if (item.IsWeapon())
        {
            filters.GenerateWeaponAttachmentDenom(item.GetType());
            attachTypes.Insert(ExpansionMarketAttachType.MAGAZINE);
            attachTypes.Insert(ExpansionMarketAttachType.OPTIC);
            attachTypes.Insert(ExpansionMarketAttachType.SUPPRESSOR);
            attachTypes.Insert(ExpansionMarketAttachType.WRAP);
            attachTypes.Insert(ExpansionMarketAttachType.SWITCH);
            attachTypes.Insert(ExpansionMarketAttachType.BUTTSTOCK);
            attachTypes.Insert(ExpansionMarketAttachType.HANDGUARD);
            attachTypes.Insert(ExpansionMarketAttachType.BAYONET);
            attachTypes.Insert(ExpansionMarketAttachType.RAIL);
            attachTypes.Insert(ExpansionMarketAttachType.LIGHT);
            attachTypes.Insert(ExpansionMarketAttachType.OTHER);
        }
        else if (item.IsClothing() && ExpansionMarketFilters.IsCustomizableClothing(item.GetType()))
        {
            filters.GenerateClothingAttachmentDenom(item.GetType());
            attachTypes.Insert(ExpansionMarketAttachType.CANTEENS);
            attachTypes.Insert(ExpansionMarketAttachType.GRENADES);
            attachTypes.Insert(ExpansionMarketAttachType.HOLSTERS);
            attachTypes.Insert(ExpansionMarketAttachType.POUCHES);
            attachTypes.Insert(ExpansionMarketAttachType.PATCHES);
            attachTypes.Insert(ExpansionMarketAttachType.OTHER);
        }
        else
        {
            return false;
        }

        TStringArray availableAttachments = filters.GetAttachmentsByClassNameAndTypes(item.GetType(), attachTypes);
        if (availableAttachments.Count() > 0)
            return true;

        ExpansionTraderObjectBase traderObject = GetTraderObject();
        ExpansionMarketTrader traderMarket;
        if (traderObject)
            traderMarket = traderObject.GetTraderMarket();
        if (!traderMarket)
            return false;

        for (int i = 0; i < item.GetInventory().AttachmentCount(); i++)
        {
            EntityAI attachment = item.GetInventory().GetAttachmentFromIndex(i);
            if (!attachment)
                continue;

            string actualClass = attachment.GetType();
            actualClass.ToLower();
            if (!ExpansionMarketCategory.GetGlobalItem(actualClass, false))
                continue;

            string marketClass = m_MarketModule.GetMarketItemClassName(traderMarket, actualClass);
            if (traderMarket.ItemExists(marketClass) && traderMarket.CanSellItem(marketClass))
                return true;
        }

        return false;
    }

    protected int BDE_CountEditableInventoryItems()
    {
        if (!m_MarketModule)
            return 0;

        int count;
        array<EntityAI> inventory = m_MarketModule.LocalGetEntityInventory();
        foreach (EntityAI item: inventory)
        {
            if (BDE_IsEditableInventoryItem(item))
                count++;
        }

        return count;
    }

    protected void BDE_CollectAvailableAmmoClasses(TStringArray ammoClasses)
    {
        if (!ammoClasses || !m_MarketModule || !GetMarketFilters() || !GetTraderObject())
            return;

        ExpansionMarketTrader traderMarket = GetTraderObject().GetTraderMarket();
        ExpansionMarketTraderZone zone = m_MarketModule.GetClientZone();
        if (!traderMarket || !zone)
            return;

        array<EntityAI> inventory = m_MarketModule.LocalGetEntityInventory();
        foreach (EntityAI item: inventory)
        {
            if (!item || !item.IsWeapon())
                continue;

            GetMarketFilters().GenerateWeaponAttachmentDenom(item.GetType());
            ExpansionMarketWeapon marketWeapon = GetMarketFilters().GetMarketWeapon(item);
            if (!marketWeapon)
                continue;

            // Read the weapon's raw compatibility lists here instead of the
            // normal market filter so attachment-only magazines are still valid
            // Ammo entries when this trader can actually sell them.
            foreach (string magazineClass: marketWeapon.magazines)
                BDE_AddAvailableAmmoClass(ammoClasses, magazineClass, traderMarket, zone);

            foreach (string bulletClass: marketWeapon.bullets)
            {
                BDE_AddAvailableAmmoClass(ammoClasses, bulletClass, traderMarket, zone);
                string boxClass = GetDayZGame().Expansion_GetAmmoBoxByAmmoName(bulletClass);
                BDE_AddAvailableAmmoClass(ammoClasses, boxClass, traderMarket, zone);
            }
        }
    }

    protected void BDE_AddAvailableAmmoClass(TStringArray ammoClasses, string className, ExpansionMarketTrader traderMarket, ExpansionMarketTraderZone zone)
    {
        if (className == string.Empty)
            return;

        string marketClass = className;
        marketClass.ToLower();
        marketClass = m_MarketModule.GetMarketItemClassName(traderMarket, marketClass);

        ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(marketClass, false);
        if (!marketItem || !traderMarket.ItemExists(marketClass) || !traderMarket.CanBuyItem(marketClass))
            return;

        int stock = 1;
        if (!marketItem.IsStaticStock())
            stock = zone.GetStock(marketClass);
        if (stock <= 0)
            return;

        if (ammoClasses.Find(marketClass) == -1)
            ammoClasses.Insert(marketClass);
    }

    protected void BDE_InsertInventoryItemSorted(array<EntityAI> sortedItems, EntityAI item)
    {
        int insertIndex;
        for (insertIndex = 0; insertIndex < sortedItems.Count(); insertIndex++)
        {
            if (BDE_InventoryItemComesBefore(item, sortedItems[insertIndex]))
                break;
        }
        sortedItems.InsertAt(item, insertIndex);
    }

    protected bool BDE_InventoryItemComesBefore(EntityAI a, EntityAI b)
    {
        string nameA = Widget.TranslateString(ExpansionStatic.GetItemDisplayNameWithType(a.GetType()));
        string nameB = Widget.TranslateString(ExpansionStatic.GetItemDisplayNameWithType(b.GetType()));
        nameA.ToLower();
        nameB.ToLower();

        if (m_BDESortMode == 2 || m_BDESortMode == 3)
        {
            int countA = a.GetInventory().AttachmentCount();
            int countB = b.GetInventory().AttachmentCount();
            if (countA != countB)
            {
                if (m_BDESortMode == 3)
                    return countA > countB;
                return countA < countB;
            }
        }

        int comparison = ExpansionString.StrCmp(nameA, nameB);
        if (m_BDESortMode == 1)
            return comparison > 0;
        return comparison < 0;
    }

    protected void BDE_InsertAmmoClassSorted(array<string> sortedClasses, string className)
    {
        int insertIndex;
        for (insertIndex = 0; insertIndex < sortedClasses.Count(); insertIndex++)
        {
            if (BDE_AmmoClassComesBefore(className, sortedClasses[insertIndex]))
                break;
        }
        sortedClasses.InsertAt(className, insertIndex);
    }

    protected bool BDE_AmmoClassComesBefore(string classA, string classB)
    {
        string nameA = Widget.TranslateString(ExpansionStatic.GetItemDisplayNameWithType(classA));
        string nameB = Widget.TranslateString(ExpansionStatic.GetItemDisplayNameWithType(classB));
        nameA.ToLower();
        nameB.ToLower();

        int priceA = BDE_GetItemEditorBuyPrice(classA);
        int priceB = BDE_GetItemEditorBuyPrice(classB);
        int comparison;

        if (m_BDESortMode == 0 || m_BDESortMode == 1)
        {
            comparison = ExpansionString.StrCmp(nameA, nameB);
            if (m_BDESortMode == 1)
                return comparison > 0;
            return comparison < 0;
        }

        if (priceA == priceB)
            comparison = ExpansionString.StrCmp(nameA, nameB);
        else if (priceA < priceB)
            comparison = -1;
        else
            comparison = 1;

        if (m_BDESortMode == 3)
            return comparison > 0;
        return comparison < 0;
    }

    protected int BDE_GetItemEditorBuyPrice(string className)
    {
        if (!m_MarketModule || !m_TraderMarket)
            return 0;

        string lower = className;
        lower.ToLower();
        ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(lower, false);
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        int price = 0;
        if (marketItem && player)
            m_MarketModule.FindPriceOfPurchaseEx(marketItem, m_MarketModule.GetClientZone(), m_TraderMarket, player, 1, price, false);
        return price;
    }

    protected ExpansionMarketMenuItem BDE_CreateItemEditorMarketItemElement(string className, bool allowSynthetic = true)
    {
        string lower = className;
        lower.ToLower();
        ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(lower, false);

        if (!marketItem && allowSynthetic)
            marketItem = new ExpansionMarketItem(-1, lower, 0, 0, 1, 1, null, null, -1, 100);
        if (!marketItem)
            return null;

        ExpansionMarketMenuItem itemElement = new ExpansionMarketMenuItem(this, marketItem);
        if (!itemElement)
            return null;

        Widget hiddenSourceContainer = GetLayoutRoot().FindAnyWidget("bde_attachment_source_host");
        if (hiddenSourceContainer)
            hiddenSourceContainer.AddChild(itemElement.GetLayoutRoot());

        m_BDEItemEditorTemporaryItems.Insert(itemElement);
        return itemElement;
    }

    protected void BDE_DestroyItemEditorTemporaryItems()
    {
        if (!m_BDEItemEditorTemporaryItems)
            return;

        for (int i = m_BDEItemEditorTemporaryItems.Count() - 1; i >= 0; i--)
        {
            ExpansionMarketMenuItem itemElement = m_BDEItemEditorTemporaryItems[i];
            if (itemElement)
                itemElement.Destroy();
        }
        m_BDEItemEditorTemporaryItems.Clear();
    }

    void BDE_SelectInventoryEditorItem(EntityAI inventoryItem)
    {
        BDE_HideItemCardTooltip();
        if (!m_BDEItemEditorMode || m_BDEAttachmentMode || !BDE_IsEditableInventoryItem(inventoryItem))
            return;

        BDE_ClearInventoryEditorSelection();
        m_BDEInventoryTarget = inventoryItem;

        for (int i = 0; i < inventoryItem.GetInventory().AttachmentCount(); i++)
        {
            EntityAI attachment = inventoryItem.GetInventory().GetAttachmentFromIndex(i);
            if (!attachment)
                continue;
            string attachmentClass = attachment.GetType();
            attachmentClass.ToLower();
            m_BDEInventoryOriginalAttachments.Insert(attachmentClass);
        }

        string parentClass = inventoryItem.GetType();
        parentClass.ToLower();
        TStringArray editorAttachments = new TStringArray;
        editorAttachments.InsertAll(m_BDEInventoryOriginalAttachments);
        TStringArray editorVariants = new TStringArray;

        ExpansionMarketItem baseItem = ExpansionMarketCategory.GetGlobalItem(parentClass, false);
        if (baseItem)
        {
            m_BDEInventorySelectedMarketItem = new ExpansionMarketItem(baseItem.CategoryID, parentClass, baseItem.MinPriceThreshold, baseItem.MaxPriceThreshold, baseItem.MinStockThreshold, baseItem.MaxStockThreshold, editorAttachments, editorVariants, baseItem.SellPricePercent, baseItem.QuantityPercent, baseItem.ItemID);
            m_BDEInventorySelectedMarketItem.m_SellPricePercent = baseItem.m_SellPricePercent;
            m_BDEInventorySelectedMarketItem.m_IsMagazine = baseItem.m_IsMagazine;
        }
        else
        {
            m_BDEInventorySelectedMarketItem = new ExpansionMarketItem(-1, parentClass, 0, 0, 1, 1, editorAttachments, editorVariants, -1, 100);
        }

        m_BDEInventorySelectedItemElement = new ExpansionMarketMenuItem(this, m_BDEInventorySelectedMarketItem);
        Widget hiddenSourceContainer = GetLayoutRoot().FindAnyWidget("bde_attachment_source_host");
        if (hiddenSourceContainer && m_BDEInventorySelectedItemElement)
            hiddenSourceContainer.AddChild(m_BDEInventorySelectedItemElement.GetLayoutRoot());

        if (m_BDEInventorySelectedItemElement)
        {
            SetItemInfo(m_BDEInventorySelectedItemElement);
            if (market_item_info_attachments_setup_button)
                market_item_info_attachments_setup_button.Enable(true);
        }

        BDE_UpdateSelectedCardState();
    }

    protected void BDE_ClearInventoryEditorSelection()
    {
        if (m_BDEInventorySelectedItemElement)
        {
            if (m_SelectedMarketItemElement == m_BDEInventorySelectedItemElement)
            {
                m_SelectedMarketItemElement = null;
                m_SelectedMarketItem = null;
            }
            m_BDEInventorySelectedItemElement.Destroy();
        }

        m_BDEInventorySelectedItemElement = null;
        m_BDEInventorySelectedMarketItem = null;
        m_BDEInventoryTarget = null;
        if (m_BDEInventoryOriginalAttachments)
            m_BDEInventoryOriginalAttachments.Clear();
    }

    void BDE_ShowInventoryItemCardTooltip(EntityAI item, Widget sourceWidget)
    {
        if (!item || !sourceWidget || !InventoryItem.Cast(item))
            return;

        BDE_HideItemCardTooltip();
        m_BDEItemTooltip = MissionGameplay.SetItemTooltip(item);
    }

    protected void BDE_BuildInventoryAttachmentDiff(out TStringArray additions, out TStringArray removals)
    {
        additions = new TStringArray;
        removals = new TStringArray;
        if (!m_BDEInventorySelectedMarketItem || !m_BDEInventoryOriginalAttachments)
            return;

        TStringArray finalRemaining = new TStringArray;
        finalRemaining.InsertAll(m_BDEInventorySelectedMarketItem.SpawnAttachments);

        foreach (string originalAttachment: m_BDEInventoryOriginalAttachments)
        {
            int finalIndex = finalRemaining.Find(originalAttachment);
            if (finalIndex > -1)
                finalRemaining.RemoveOrdered(finalIndex);
            else
                removals.Insert(originalAttachment);
        }

        additions.InsertAll(finalRemaining);
    }

    protected EntityAI BDE_FindUnusedDirectAttachment(string className, array<EntityAI> usedItems)
    {
        if (!m_BDEInventoryTarget)
            return null;

        for (int i = 0; i < m_BDEInventoryTarget.GetInventory().AttachmentCount(); i++)
        {
            EntityAI attachment = m_BDEInventoryTarget.GetInventory().GetAttachmentFromIndex(i);
            if (!attachment || usedItems.Find(attachment) > -1)
                continue;

            if (CF_String.EqualsIgnoreCase(attachment.GetType(), className))
                return attachment;
        }

        return null;
    }

    protected bool BDE_CalculateInventoryAttachmentTotals(out int buyTotal, out int sellTotal)
    {
        buyTotal = 0;
        sellTotal = 0;
        if (!m_BDEInventoryTarget || !m_MarketModule || !m_TraderMarket || !GetTraderObject())
            return false;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return false;

        TStringArray additions;
        TStringArray removals;
        BDE_BuildInventoryAttachmentDiff(additions, removals);

        map<string, int> removedStock = new map<string, int>;
        foreach (string additionClass: additions)
        {
            string marketClass = additionClass;
            marketClass.ToLower();
            marketClass = m_MarketModule.GetMarketItemClassName(m_TraderMarket, marketClass);
            ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(marketClass, false);
            if (!marketItem)
                return false;

            int price = 0;
            ExpansionMarketResult result;
            ExpansionMarketReserve reserve;
            if (!m_MarketModule.FindPriceOfPurchaseEx(marketItem, m_MarketModule.GetClientZone(), m_TraderMarket, player, 1, price, false, result, reserve, removedStock) || result != ExpansionMarketResult.Success)
                return false;
            buyTotal += price;
        }

        ref array<EntityAI> usedItems = new array<EntityAI>;
        map<string, int> addedStock = new map<string, int>;
        foreach (string removalClass: removals)
        {
            EntityAI attachment = BDE_FindUnusedDirectAttachment(removalClass, usedItems);
            if (!attachment)
                return false;
            usedItems.Insert(attachment);

            string removalMarketClass = removalClass;
            removalMarketClass.ToLower();
            removalMarketClass = m_MarketModule.GetMarketItemClassName(m_TraderMarket, removalMarketClass);
            ExpansionMarketItem removalMarketItem = ExpansionMarketCategory.GetGlobalItem(removalMarketClass, false);
            if (!removalMarketItem || !m_TraderMarket.ItemExists(removalMarketClass) || !m_TraderMarket.CanSellItem(removalMarketClass))
                return false;

            int stock = 1;
            if (!removalMarketItem.IsStaticStock())
            {
                stock = m_MarketModule.GetClientZone().GetStock(removalMarketClass);
                int stockBonus;
                if (addedStock.Find(removalMarketClass, stockBonus))
                    stock += stockBonus;
            }

            ExpansionMarketSell sell = new ExpansionMarketSell(player);
            sell.Item = removalMarketItem;
            sell.Trader = GetTraderObject();
            array<EntityAI> sellItems = {attachment};
            ExpansionMarketResult sellResult;
            string failedClassName;
            bool sellPriceResult = m_MarketModule.FindSellPrice(player, sellItems, stock, 1, sell, true, sellResult, failedClassName);
            // Direct attachments are not "loose" while they are still on the
            // player's item, so Expansion reports FailedCannotSell even though
            // it still calculates the exact condition-aware value in sell.Price.
            // The server detaches them transactionally before the real sale.
            if ((!sellPriceResult && sellResult != ExpansionMarketResult.FailedCannotSell) || sell.Price <= 0)
                return false;

            sellTotal += sell.Price;
            foreach (ExpansionMarketSellItem sellItem: sell.Sell)
            {
                int existingBonus;
                int stockAmount = Math.Round(sellItem.AddStockAmount);
                if (addedStock.Find(sellItem.ClassName, existingBonus))
                    addedStock.Set(sellItem.ClassName, existingBonus + stockAmount);
                else
                    addedStock.Insert(sellItem.ClassName, stockAmount);
            }
        }

        return true;
    }

    protected void BDE_UpdateInventoryAttachmentTransactionSummary()
    {
        if (!m_BDEAttachmentMode || !m_BDEAttachmentFromItemEditor || !m_MarketMenuController)
            return;

        TStringArray additions;
        TStringArray removals;
        BDE_BuildInventoryAttachmentDiff(additions, removals);

        int buyTotal = 0;
        int sellTotal = 0;
        bool validPrices = BDE_CalculateInventoryAttachmentTotals(buyTotal, sellTotal);
        bool hasChanges = additions.Count() > 0 || removals.Count() > 0;
        bool canAfford = true;
        int netCost = buyTotal - sellTotal;
        if (validPrices && netCost > 0)
        {
            array<int> monies;
            PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
            canAfford = m_MarketModule.FindMoneyAndCountTypes(player, netCost, monies, false, null, m_TraderMarket);
        }

        m_MarketMenuController.MarketItemTotalBuyPrice = GetDisplayPrice(buyTotal, true);
        m_MarketMenuController.MarketItemTotalSellPrice = GetDisplayPrice(sellTotal, true);
        m_MarketMenuController.CurrencyBuyIcon = GetExpansionSettings().GetMarket().CurrencyIcon;
        m_MarketMenuController.CurrencySellIcon = GetExpansionSettings().GetMarket().CurrencyIcon;
        m_MarketMenuController.NotifyPropertiesChanged({"MarketItemTotalBuyPrice", "MarketItemTotalSellPrice", "CurrencyBuyIcon", "CurrencySellIcon"});

        if (m_BDEBuyActionArea)
            m_BDEBuyActionArea.Show(true);
        if (m_BDESellActionArea)
            m_BDESellActionArea.Show(true);
        if (m_BDEBuyButton)
            m_BDEBuyButton.Enable(hasChanges && validPrices && canAfford && !m_BDEInventoryAttachmentRequestPending);
        if (m_BDESellButton)
            m_BDESellButton.Enable(false);
    }

    protected int BDE_GetInventoryAttachmentNextBuyPrice(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!source || !m_BDEInventoryTarget || !m_MarketModule || !m_TraderMarket || !GetTraderObject())
            return 0;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return 0;

        TStringArray stagedAdditions;
        TStringArray stagedRemovals;
        BDE_BuildInventoryAttachmentDiff(stagedAdditions, stagedRemovals);

        map<string, int> purchaseStock = new map<string, int>;
        foreach (string stagedAdditionClass: stagedAdditions)
        {
            string stagedMarketClass = stagedAdditionClass;
            stagedMarketClass.ToLower();
            stagedMarketClass = m_MarketModule.GetMarketItemClassName(m_TraderMarket, stagedMarketClass);
            if (!m_TraderMarket.ItemExists(stagedMarketClass) || !m_TraderMarket.CanBuyItem(stagedMarketClass))
                return 0;

            ExpansionMarketItem stagedMarketItem = ExpansionMarketCategory.GetGlobalItem(stagedMarketClass, false);
            if (!stagedMarketItem)
                return 0;

            int stagedPrice = 0;
            ExpansionMarketResult stagedResult;
            ExpansionMarketReserve stagedReserve;
            if (!m_MarketModule.FindPriceOfPurchaseEx(stagedMarketItem, m_MarketModule.GetClientZone(), m_TraderMarket, player, 1, stagedPrice, false, stagedResult, stagedReserve, purchaseStock) || stagedResult != ExpansionMarketResult.Success)
                return 0;
        }

        string candidateMarketClass = source.GetItemClassName();
        candidateMarketClass.ToLower();
        candidateMarketClass = m_MarketModule.GetMarketItemClassName(m_TraderMarket, candidateMarketClass);
        if (!m_TraderMarket.ItemExists(candidateMarketClass) || !m_TraderMarket.CanBuyItem(candidateMarketClass))
            return 0;

        ExpansionMarketItem candidateMarketItem = ExpansionMarketCategory.GetGlobalItem(candidateMarketClass, false);
        if (!candidateMarketItem)
            return 0;

        int candidatePrice = 0;
        ExpansionMarketResult candidateResult;
        ExpansionMarketReserve candidateReserve;
        if (!m_MarketModule.FindPriceOfPurchaseEx(candidateMarketItem, m_MarketModule.GetClientZone(), m_TraderMarket, player, 1, candidatePrice, false, candidateResult, candidateReserve, purchaseStock) || candidateResult != ExpansionMarketResult.Success)
            return 0;

        return candidatePrice;
    }

    protected EntityAI BDE_FindDirectAttachmentByClassOrdinal(string className, int ordinal)
    {
        if (!m_BDEInventoryTarget || ordinal < 0)
            return null;

        int foundIndex;
        for (int i = 0; i < m_BDEInventoryTarget.GetInventory().AttachmentCount(); i++)
        {
            EntityAI attachment = m_BDEInventoryTarget.GetInventory().GetAttachmentFromIndex(i);
            if (!attachment || !CF_String.EqualsIgnoreCase(attachment.GetType(), className))
                continue;

            if (foundIndex == ordinal)
                return attachment;
            foundIndex++;
        }

        return null;
    }

    protected int BDE_GetInventoryAttachmentSellPriceForEntity(EntityAI attachment)
    {
        if (!attachment || !m_MarketModule || !m_TraderMarket || !GetTraderObject())
            return 0;

        string marketClass = attachment.GetType();
        marketClass.ToLower();
        marketClass = m_MarketModule.GetMarketItemClassName(m_TraderMarket, marketClass);
        ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(marketClass, false);
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!marketItem || !player || !m_TraderMarket.ItemExists(marketClass) || !m_TraderMarket.CanSellItem(marketClass))
            return 0;

        int stock = 1;
        if (!marketItem.IsStaticStock())
            stock = m_MarketModule.GetClientZone().GetStock(marketClass);

        ExpansionMarketSell sell = new ExpansionMarketSell(player);
        sell.Item = marketItem;
        sell.Trader = GetTraderObject();
        array<EntityAI> sellItems = {attachment};
        ExpansionMarketResult result;
        string failedClassName;
        bool sellPriceResult = m_MarketModule.FindSellPrice(player, sellItems, stock, 1, sell, true, result, failedClassName);
        if ((sellPriceResult && result == ExpansionMarketResult.Success) || (result == ExpansionMarketResult.FailedCannotSell && sell.Price > 0))
            return sell.Price;
        return 0;
    }

    protected int BDE_GetInventoryAttachmentNextSellPrice(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!source)
            return 0;

        int currentCount = source.GetCount();
        int originalCount = BDE_GetInventoryAttachmentOriginalCount(source.GetItemClassName());
        if (currentCount <= 0 || currentCount > originalCount)
            return 0;

        int alreadyRemovedCount = originalCount - currentCount;
        EntityAI attachment = BDE_FindDirectAttachmentByClassOrdinal(source.GetItemClassName(), alreadyRemovedCount);
        return BDE_GetInventoryAttachmentSellPriceForEntity(attachment);
    }

    protected int BDE_GetInventoryAttachmentSellPrice(string className)
    {
        EntityAI attachment = BDE_FindDirectAttachmentByClassOrdinal(className, 0);
        return BDE_GetInventoryAttachmentSellPriceForEntity(attachment);
    }

    protected void BDE_ApplyInventoryAttachmentEdit()
    {
        if ((!m_BDEAttachmentMode || !m_BDEAttachmentFromItemEditor) && !BDE_HasPendingVanillaInventoryWeaponEdit())
            return;
        if (m_BDEInventoryAttachmentRequestPending || !m_BDEInventoryTarget)
            return;

        TStringArray additions;
        TStringArray removals;
        BDE_BuildInventoryAttachmentDiff(additions, removals);
        if (additions.Count() == 0 && removals.Count() == 0)
        {
            BDE_ReturnToItemEditorAfterAttachmentEdit();
            return;
        }

        int buyTotal = 0;
        int sellTotal = 0;
        if (!BDE_CalculateInventoryAttachmentTotals(buyTotal, sellTotal))
        {
            ExpansionNotification("ITEM EDITOR", "One or more attachment prices are no longer valid. Refresh the editor and try again.").Error();
            return;
        }

        int low, high;
        m_BDEInventoryTarget.GetNetworkID(low, high);
        if (low == 0 && high == 0)
        {
            ExpansionNotification("ITEM EDITOR", "The selected inventory item is not network synchronized.").Error();
            return;
        }

        m_BDEInventoryAttachmentRequestPending = true;
        BDE_UpdateInventoryAttachmentTransactionSummary();
        m_MarketModule.BDE_RequestInventoryAttachmentEdit(low, high, m_BDEInventoryOriginalAttachments, additions, removals, buyTotal, sellTotal, GetTraderObject());
    }

    bool BDE_IsVanillaInventoryWeaponManagerActive()
    {
        return m_BDEVanillaInventoryWeaponManager && m_BDEAttachmentMode && m_BDEAttachmentFromItemEditor;
    }

    protected bool BDE_HasPendingVanillaInventoryWeaponEdit()
    {
        if (!m_BDEItemEditorMode || !m_BDEInventoryTarget || !m_BDEInventoryTarget.IsWeapon() || !m_BDEInventorySelectedMarketItem)
            return false;

        TStringArray additions;
        TStringArray removals;
        BDE_BuildInventoryAttachmentDiff(additions, removals);
        return additions.Count() > 0 || removals.Count() > 0;
    }

    void BDE_ApplyVanillaInventoryWeaponAttachmentEdit()
    {
        if (!BDE_IsVanillaInventoryWeaponManagerActive())
            return;

        BDE_ApplyInventoryAttachmentEdit();
    }

    void BDE_ResetVanillaInventoryWeaponAttachments()
    {
        if (!BDE_IsVanillaInventoryWeaponManagerActive() || !m_BDEInventorySelectedMarketItem || !m_BDEInventoryOriginalAttachments)
            return;

        m_BDEInventorySelectedMarketItem.SpawnAttachments.Clear();
        m_BDEInventorySelectedMarketItem.SpawnAttachments.InsertAll(m_BDEInventoryOriginalAttachments);
    }

    void BDE_CancelVanillaInventoryWeaponAttachmentEdit()
    {
        if (!BDE_IsVanillaInventoryWeaponManagerActive())
            return;

        BDE_ResetVanillaInventoryWeaponAttachments();

        m_BDEVanillaInventoryWeaponManager = false;
        m_BDEAttachmentMode = false;
        m_BDEAttachmentFromItemEditor = false;
        m_BDESelectedAttachmentCategory = null;
        m_BDEAttachmentParentItemElement = null;

        if (m_MarketMenuItemManager)
        {
            m_MarketMenuItemManager.BDE_SetVanillaInventoryWeaponMode(false);
            if (m_MarketMenuItemManager.IsVisible())
                m_MarketMenuItemManager.Hide();
        }

        market_menu_content.Show(true);
        market_menu_info.Show(true);
        market_item_preview.Show(true);
        market_filter_box.SetText(m_BDESavedItemEditorSearch);

        if (m_BDESidebarHeaderText)
            m_BDESidebarHeaderText.SetText("ITEM EDITOR");
        if (m_BDEAttachmentSetupLabel)
            m_BDEAttachmentSetupLabel.SetText("CONFIGURE ATTACHMENTS");

        if (m_BDEInventorySelectedItemElement)
            SetItemInfo(m_BDEInventorySelectedItemElement, false);
        BDE_UpdateSelectedCardState();
        BDE_ForceSynchronizeInterface();
    }

    void BDE_OnInventoryAttachmentEditResult(bool success, string message)
    {
        if (!m_BDEItemEditorMode)
            return;

        m_BDEInventoryAttachmentRequestPending = false;
        if (!success)
        {
            ExpansionNotification("ITEM EDITOR", message).Error();
            BDE_UpdateInventoryAttachmentTransactionSummary();
            return;
        }

        ExpansionNotification("ITEM EDITOR", message).Success();
        if (m_MarketModule && GetTraderObject())
            m_MarketModule.RequestTraderItems(GetTraderObject(), 0, GetTraderObject().GetTraderMarket().m_StockOnly);
        BDE_ReturnToItemEditorAfterAttachmentEdit();
    }

    protected void BDE_ReturnToItemEditorAfterAttachmentEdit()
    {
        if (!m_BDEItemEditorMode)
            return;

        if (m_BDEVanillaInventoryWeaponManager && m_MarketMenuItemManager)
        {
            m_BDEVanillaInventoryWeaponManager = false;
            m_MarketMenuItemManager.BDE_SetVanillaInventoryWeaponMode(false);
            if (m_MarketMenuItemManager.IsVisible())
                m_MarketMenuItemManager.Hide();
        }

        m_BDEAttachmentRebuildGuard = true;
        BDE_DestroyItemCards();
        BDE_DestroyCategoryEntries();
        BDE_DestroyAttachmentTemporaryItems();
        if (m_MarketMenuItemManager)
            m_MarketMenuItemManager.BDE_ClearEmbeddedAttachmentData();
        m_BDEAttachmentRebuildGuard = false;

        m_BDEAttachmentMode = false;
        m_BDEAttachmentFromItemEditor = false;
        m_BDEVanillaInventoryWeaponManager = false;
        m_BDESelectedAttachmentCategory = null;
        m_BDEAttachmentParentItemElement = null;
        BDE_ClearInventoryEditorSelection();

        market_filter_box.SetText(m_BDESavedItemEditorSearch);
        if (m_BDESidebarHeaderText)
            m_BDESidebarHeaderText.SetText("ITEM EDITOR");
        if (m_BDEItemEditorButtonText)
            m_BDEItemEditorButtonText.SetText("Back To Market");
        if (m_BDEAttachmentSetupLabel)
            m_BDEAttachmentSetupLabel.SetText("CONFIGURE ATTACHMENTS");
        if (market_item_info_attachments_setup_button)
            market_item_info_attachments_setup_button.Enable(false);
        if (m_BDESellablesPanel)
            m_BDESellablesPanel.Show(false);
        if (m_BDEPurchasablesPanel)
            m_BDEPurchasablesPanel.Show(false);

        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_RebuildItemEditorCategories, 250, false);
    }

    override void OnOpenItemManagerButtonClick()
    {
        if (m_BDEAttachmentMode)
        {
            BDE_ExitEmbeddedAttachmentMode();
            return;
        }

        BDE_EnterEmbeddedAttachmentMode();
    }

    protected void BDE_EnterEmbeddedAttachmentMode()
    {
        if (!m_MarketMenuItemManager || !m_SelectedMarketItem || !m_SelectedMarketItemElement)
            return;

        BDE_HideItemCardTooltip();

        if (m_BDEItemEditorMode)
        {
            if (!m_BDEInventoryTarget || !m_BDEInventorySelectedMarketItem || m_SelectedMarketItemElement != m_BDEInventorySelectedItemElement)
                return;

            m_BDEAttachmentFromItemEditor = true;
            m_BDESavedItemEditorSearch = market_filter_box.GetText();
            m_BDEAttachmentParentItemElement = m_SelectedMarketItemElement;
            m_BDESelectedAttachmentCategory = null;
            m_BDEAttachmentMode = true;

            // Inventory weapons use the exact same embedded BDE attachment
            // customiser as normal Market weapons.  The only difference is that
            // the temporary MarketItem starts with the real weapon's current
            // attachments and BUY commits the resulting diff back to that weapon.
            // Do not show Expansion's standalone item-manager window here.
            m_BDEVanillaInventoryWeaponManager = false;
            BDE_DestroyItemCards();
            BDE_DestroyCategoryEntries();
            BDE_DestroyItemEditorTemporaryItems();
            market_filter_box.SetText("");

            if (m_BDESidebarHeaderText)
                m_BDESidebarHeaderText.SetText("ATTACHMENT CATEGORIES");
            if (m_BDEAttachmentSetupLabel)
                m_BDEAttachmentSetupLabel.SetText("Back to Inv Attachments");

            m_BDEAttachmentRebuildGuard = true;
            if (m_BDEInventoryTarget.IsWeapon())
            {
                // Match the working Market-weapon Configure Attachments path
                // exactly.  Expansion builds categories and replacement rules
                // from the synthetic weapon MarketItem just as it would for a
                // weapon being purchased from the trader.
                m_MarketMenuItemManager.BDE_PrepareEmbeddedAttachmentMode(m_SelectedMarketItem);
            }
            else
            {
                // Clothing/container items keep the inventory-aware extension
                // so multiple identical attachments can be staged per free slot.
                m_MarketMenuItemManager.BDE_PrepareEmbeddedInventoryAttachmentMode(m_SelectedMarketItem, m_BDEInventoryTarget);
            }
            m_BDEAttachmentRebuildGuard = false;
        }
        else
        {
            m_BDEAttachmentFromItemEditor = false;
            m_BDESavedMarketSearch = market_filter_box.GetText();
            m_BDESavedSelectedCategory = m_BDESelectedCategory;
            m_BDEAttachmentParentItemElement = m_SelectedMarketItemElement;
            m_BDESelectedAttachmentCategory = null;
            m_BDEAttachmentMode = true;
            m_BDESelectedCategory = null;

            market_filter_box.SetText("");
            if (m_BDESidebarHeaderText)
                m_BDESidebarHeaderText.SetText("ATTACHMENT CATEGORIES");
            if (m_BDEAttachmentSetupLabel)
                m_BDEAttachmentSetupLabel.SetText("BACK TO MARKET");

            // Build Expansion's compatibility data while its own view remains
            // hidden. The visible sidebar/grid below are still the normal market
            // BDECategories and BDEItems collections.
            m_BDEAttachmentRebuildGuard = true;
            m_MarketMenuItemManager.BDE_PrepareEmbeddedAttachmentMode(m_SelectedMarketItem);
            m_BDEAttachmentRebuildGuard = false;
        }

        market_menu_content.Show(true);
        market_menu_info.Show(true);
        market_item_preview.Show(true);

        BDE_RebuildAttachmentCategories();
        UpdatePreview();
        BDE_UpdateInventoryAttachmentTransactionSummary();
    }

    protected void BDE_ExitEmbeddedAttachmentMode()
    {
        if (!m_BDEAttachmentMode)
            return;

        // In the player Item Editor this button returns to the inventory
        // attachment list without applying any of the staged attachment changes.
        if (m_BDEAttachmentFromItemEditor)
        {
            BDE_ReturnToItemEditorAfterAttachmentEdit();
            return;
        }

        m_BDEAttachmentRebuildGuard = true;
        BDE_DestroyItemCards();
        BDE_DestroyCategoryEntries();
        BDE_DestroyAttachmentTemporaryItems();
        if (m_MarketMenuItemManager)
            m_MarketMenuItemManager.BDE_ClearEmbeddedAttachmentData();
        m_BDEAttachmentRebuildGuard = false;

        m_BDEAttachmentMode = false;
        m_BDESelectedAttachmentCategory = null;
        m_BDESelectedCategory = m_BDESavedSelectedCategory;
        m_BDESavedSelectedCategory = null;
        m_BDEAttachmentParentItemElement = null;

        market_filter_box.SetText(m_BDESavedMarketSearch);
        if (m_BDESidebarHeaderText)
            m_BDESidebarHeaderText.SetText("MARKET CATEGORIES");
        if (m_BDEAttachmentSetupLabel)
            m_BDEAttachmentSetupLabel.SetText("CONFIGURE ATTACHMENTS");

        market_menu_content.Show(true);
        market_menu_info.Show(true);
        market_item_preview.Show(true);
        UpdatePreview();
        BDE_ForceSynchronizeInterface();
    }

    override void HideItemManager()
    {
        if (m_BDEClosing)
            return;

        if (m_BDEVanillaInventoryWeaponManager)
        {
            // The vanilla weapon manager's Close/Buy paths clear this flag
            // before hiding it. If the view is hidden externally, treat it as
            // a cancelled inventory edit rather than falling through into the
            // embedded BDE attachment-mode teardown.
            BDE_CancelVanillaInventoryWeaponAttachmentEdit();
            return;
        }

        if (m_BDEAttachmentMode)
            BDE_ExitEmbeddedAttachmentMode();
        else
            super.HideItemManager();
    }

    protected void BDE_RebuildAttachmentCategories()
    {
        if (!m_BDEAttachmentMode || !m_MarketMenuItemManager || !m_MarketMenuController)
            return;

        BDE_DestroyItemCards();
        BDE_DestroyCategoryEntries();
        BDE_DestroyAttachmentTemporaryItems();

        ExpansionMarketMenuItemManagerController managerController = m_MarketMenuItemManager.GetMarketItemManagerController();
        if (!managerController)
            return;

        ExpansionMarketMenuItemManagerCategory firstCategory;
        for (int i = 0; i < managerController.ItemCategories.Count(); i++)
        {
            ExpansionMarketMenuItemManagerCategory sourceCategory = managerController.ItemCategories[i];
            if (!sourceCategory)
                continue;

            if (!firstCategory)
                firstCategory = sourceCategory;

            BDE_EMU_CategoryEntry entry = new BDE_EMU_CategoryEntry(this, null);
            entry.BDE_SetAttachmentCategorySource(sourceCategory);
            m_MarketMenuController.BDECategories.Insert(entry);
        }

        if (firstCategory)
            BDE_SelectAttachmentCategorySource(firstCategory);
        else
        {
            m_BDESelectedAttachmentCategory = null;
            BDE_SetSelectedCategoryText("NO COMPATIBLE ATTACHMENTS");
            m_MarketMenuController.BDEVisibleItemCount = "0 ATTACHMENTS";
            m_MarketMenuController.NotifyPropertyChanged("BDEVisibleItemCount");
        }
    }

    void BDE_SelectAttachmentCategorySource(ExpansionMarketMenuItemManagerCategory category)
    {
        if (!m_BDEAttachmentMode || !category)
            return;

        if (m_BDESelectedAttachmentCategory != category)
            BDE_ScrollItemsToTop();

        m_BDESelectedAttachmentCategory = category;
        BDE_SetSelectedCategoryText(category.BDE_GetCategoryName());
        BDE_RefreshAttachmentCategoryEntries();

        m_BDEAttachmentRebuildGuard = true;
        category.OnCategoryButtonClick();
        m_BDEAttachmentRebuildGuard = false;

        BDE_RebuildAttachmentItems();
    }

    void BDE_OnAttachmentCategoryItemsChanged()
    {
        if (m_BDEVanillaInventoryWeaponManager)
            return;
        if (!m_BDEAttachmentMode || m_BDEAttachmentRebuildGuard)
            return;

        BDE_RebuildAttachmentItems();
    }

    void BDE_OnAttachmentLogicUpdated()
    {
        if (m_BDEVanillaInventoryWeaponManager)
            return;
        if (!m_BDEAttachmentMode || m_BDEAttachmentRebuildGuard)
            return;

        BDE_RefreshAttachmentCards();
        UpdatePreview();
        BDE_UpdateInventoryAttachmentTransactionSummary();
    }

    void BDE_ToggleAttachmentSource(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!m_BDEAttachmentMode || !source)
            return;

        if (source.GetCount() > 0)
            source.OnItemCountDecrement();
        else if (source.BDE_CanAddAttachment())
            source.OnItemCountIncrement();

        BDE_RefreshAttachmentCards();
        UpdatePreview();
        BDE_UpdateInventoryAttachmentTransactionSummary();
    }

    bool BDE_IsInventoryAttachmentEditorActive()
    {
        return m_BDEAttachmentMode && m_BDEAttachmentFromItemEditor;
    }

    bool BDE_UseMarketStyleInventoryWeaponEditor()
    {
        return BDE_IsInventoryAttachmentEditorActive() && m_BDEInventoryTarget && m_BDEInventoryTarget.IsWeapon();
    }

    int BDE_GetInventoryAttachmentOriginalCount(string className)
    {
        if (!m_BDEInventoryOriginalAttachments)
            return 0;

        string classNameLower = className;
        classNameLower.ToLower();
        int count;
        foreach (string originalClass: m_BDEInventoryOriginalAttachments)
        {
            if (originalClass == classNameLower)
                count++;
        }
        return count;
    }

    bool BDE_InventoryAttachmentRemovalCancelsAddition(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_IsInventoryAttachmentEditorActive() || !source)
            return false;

        return source.GetCount() > BDE_GetInventoryAttachmentOriginalCount(source.GetItemClassName());
    }

    bool BDE_ShouldShowInventoryAttachmentAdd(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_IsInventoryAttachmentEditorActive() || !source)
            return false;

        source.BDE_RefreshAttachmentCompatibility();
        return source.BDE_CanStageAnotherAttachment();
    }

    bool BDE_ShouldShowInventoryAttachmentRemove(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_IsInventoryAttachmentEditorActive() || !source)
            return false;

        return source.GetCount() > 0;
    }

    bool BDE_CanAddInventoryAttachment(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_ShouldShowInventoryAttachmentAdd(source))
            return false;

        return BDE_GetInventoryAttachmentNextBuyPrice(source) > 0;
    }

    bool BDE_CanRemoveInventoryAttachment(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_ShouldShowInventoryAttachmentRemove(source))
            return false;

        if (BDE_InventoryAttachmentRemovalCancelsAddition(source))
            return true;

        return BDE_GetInventoryAttachmentNextSellPrice(source) > 0;
    }

    protected int BDE_GetInventoryAttachmentNominalBuyPrice(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!source || !m_MarketModule || !m_TraderMarket)
            return 0;

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!player)
            return 0;

        string marketClass = source.GetItemClassName();
        marketClass.ToLower();
        marketClass = m_MarketModule.GetMarketItemClassName(m_TraderMarket, marketClass);

        ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(marketClass, false);
        if (!marketItem)
            return 0;

        ExpansionMarketTraderZone zone = m_MarketModule.GetClientZone();
        if (!zone)
            return 0;

        int stock = 1;
        if (!marketItem.IsStaticStock())
            stock = zone.GetStock(marketClass);

        int price = marketItem.CalculatePrice(stock, 1.0, true);
        float modifier = m_MarketModule.GetBuyPricePercent(marketItem, zone, m_TraderMarket, player) / 100.0;
        return Math.Round(price * modifier);
    }

    string BDE_GetInventoryAttachmentBuyDisplayPrice(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_ShouldShowInventoryAttachmentAdd(source))
            return "";

        int price = BDE_GetInventoryAttachmentNextBuyPrice(source);
        if (price <= 0)
            price = BDE_GetInventoryAttachmentNominalBuyPrice(source);
        if (price <= 0)
            return "N/A";
        return GetDisplayPrice(price, true);
    }

    string BDE_GetInventoryAttachmentRemoveDisplayPrice(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_ShouldShowInventoryAttachmentRemove(source))
            return "";

        if (BDE_InventoryAttachmentRemovalCancelsAddition(source))
            return "0";

        int price = BDE_GetInventoryAttachmentNextSellPrice(source);
        if (price <= 0)
            return "N/A";
        return GetDisplayPrice(price, true);
    }

    string BDE_GetMarketCurrencyIcon()
    {
        return GetExpansionSettings().GetMarket().CurrencyIcon;
    }

    void BDE_AddInventoryAttachment(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_CanAddInventoryAttachment(source))
            return;

        source.OnItemCountIncrement();
        BDE_RefreshAttachmentCards();
        UpdatePreview();
        BDE_UpdateInventoryAttachmentTransactionSummary();
    }

    void BDE_RemoveInventoryAttachment(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!BDE_CanRemoveInventoryAttachment(source))
            return;

        source.OnItemCountDecrement();
        BDE_RefreshAttachmentCards();
        UpdatePreview();
        BDE_UpdateInventoryAttachmentTransactionSummary();
    }

    protected void BDE_RefreshAttachmentCategoryEntries()
    {
        if (!m_MarketMenuController)
            return;

        for (int i = 0; i < m_MarketMenuController.BDECategories.Count(); i++)
        {
            BDE_EMU_CategoryEntry entry = m_MarketMenuController.BDECategories[i];
            if (!entry)
                continue;

            entry.Refresh();
            entry.SetSelected(entry.BDE_GetAttachmentCategorySource() == m_BDESelectedAttachmentCategory);
        }
    }

    protected void BDE_RebuildAttachmentItems()
    {
        if (!m_BDEAttachmentMode || !m_MarketMenuItemManager || !m_MarketMenuController)
            return;

        BDE_DestroyItemCards();
        BDE_DestroyAttachmentTemporaryItems();

        ExpansionMarketMenuItemManagerController managerController = m_MarketMenuItemManager.GetMarketItemManagerController();
        if (!managerController)
            return;

        string filterText = market_filter_box.GetText();
        filterText.ToLower();

        ref array<ref ExpansionMarketMenuItemManagerCategoryItem> sortedItems = new array<ref ExpansionMarketMenuItemManagerCategoryItem>;
        for (int i = 0; i < managerController.CategoryItems.Count(); i++)
        {
            ExpansionMarketMenuItemManagerCategoryItem source = managerController.CategoryItems[i];
            if (!source || !source.BDE_ShouldShowInEmbeddedList() || !source.BDE_MatchesFilter(filterText))
                continue;

            source.BDE_RefreshAttachmentCompatibility();
            BDE_InsertAttachmentSourceSorted(sortedItems, source);
        }

        int visibleCount;
        for (int j = 0; j < sortedItems.Count(); j++)
        {
            ExpansionMarketMenuItemManagerCategoryItem attachmentSource = sortedItems[j];
            ExpansionMarketMenuItem sourceItem = BDE_FindOrCreateMarketItemElement(attachmentSource.GetItemClassName());
            if (!sourceItem)
            {
                Print("[BDE Market UI] Could not create normal market card source for attachment " + attachmentSource.GetItemClassName());
                continue;
            }

            if (!sourceItem.GetPreviewObject())
                sourceItem.UpdatePreviewObject();

            BDE_EMU_ItemCard card = new BDE_EMU_ItemCard(this, sourceItem);
            card.BDE_SetAttachmentSource(attachmentSource);
            m_MarketMenuController.BDEItems.Insert(card);
            visibleCount++;
        }

        m_MarketMenuController.BDEVisibleItemCount = visibleCount.ToString() + " ATTACHMENTS";
        m_MarketMenuController.NotifyPropertyChanged("BDEVisibleItemCount");
        BDE_UpdateInventoryAttachmentTransactionSummary();
    }

    protected void BDE_RefreshAttachmentCards()
    {
        if (!m_MarketMenuController)
            return;

        int visibleCount;
        for (int i = 0; i < m_MarketMenuController.BDEItems.Count(); i++)
        {
            BDE_EMU_ItemCard card = m_MarketMenuController.BDEItems[i];
            if (!card || !card.BDE_IsAttachmentCard())
                continue;

            card.Refresh();
            if (card.IsVisible())
                visibleCount++;
        }

        m_MarketMenuController.BDEVisibleItemCount = visibleCount.ToString() + " ATTACHMENTS";
        m_MarketMenuController.NotifyPropertyChanged("BDEVisibleItemCount");
    }

    protected void BDE_InsertAttachmentSourceSorted(array<ref ExpansionMarketMenuItemManagerCategoryItem> sortedItems, ExpansionMarketMenuItemManagerCategoryItem source)
    {
        int insertIndex;
        for (insertIndex = 0; insertIndex < sortedItems.Count(); insertIndex++)
        {
            if (BDE_AttachmentSourceComesBefore(source, sortedItems[insertIndex]))
                break;
        }

        sortedItems.InsertAt(source, insertIndex);
    }

    protected bool BDE_AttachmentSourceComesBefore(ExpansionMarketMenuItemManagerCategoryItem a, ExpansionMarketMenuItemManagerCategoryItem b)
    {
        string nameA = Widget.TranslateString(ExpansionStatic.GetItemDisplayNameWithType(a.GetItemClassName()));
        string nameB = Widget.TranslateString(ExpansionStatic.GetItemDisplayNameWithType(b.GetItemClassName()));
        nameA.ToLower();
        nameB.ToLower();

        int priceA = BDE_GetAttachmentUnitPrice(a);
        int priceB = BDE_GetAttachmentUnitPrice(b);
        int comparison;

        if (m_BDESortMode == 0 || m_BDESortMode == 1)
        {
            comparison = ExpansionString.StrCmp(nameA, nameB);
            if (comparison == 0)
            {
                if (priceA < priceB)
                    comparison = -1;
                else if (priceA > priceB)
                    comparison = 1;
            }

            if (m_BDESortMode == 1)
                return comparison > 0;
            return comparison < 0;
        }

        if (priceA == priceB)
            comparison = ExpansionString.StrCmp(nameA, nameB);
        else if (priceA < priceB)
            comparison = -1;
        else
            comparison = 1;

        if (m_BDESortMode == 3)
            return comparison > 0;
        return comparison < 0;
    }

    protected ExpansionMarketMenuItem BDE_FindOrCreateMarketItemElement(string className)
    {
        string classNameLower = className;
        classNameLower.ToLower();

        ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(classNameLower, false);
        if (!marketItem)
            return null;

        // Create the exact same ExpansionMarketMenuItem source used by a normal
        // trader category, then host its normal source view offscreen. The
        // visible card is also the exact normal BDE_EMU_ItemCard.
        ExpansionMarketMenuItem itemElement = new ExpansionMarketMenuItem(this, marketItem);
        if (!itemElement)
            return null;

        Widget hiddenSourceContainer = GetLayoutRoot().FindAnyWidget("bde_attachment_source_host");
        if (hiddenSourceContainer)
            hiddenSourceContainer.AddChild(itemElement.GetLayoutRoot());

        m_BDEAttachmentTemporaryItems.Insert(itemElement);
        return itemElement;
    }

    protected void BDE_DestroyAttachmentTemporaryItems()
    {
        if (!m_BDEAttachmentTemporaryItems)
            return;

        for (int i = m_BDEAttachmentTemporaryItems.Count() - 1; i >= 0; i--)
        {
            ExpansionMarketMenuItem itemElement = m_BDEAttachmentTemporaryItems[i];
            if (itemElement)
                itemElement.Destroy();
        }

        m_BDEAttachmentTemporaryItems.Clear();
    }

    int BDE_GetAttachmentUnitPrice(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        if (!source)
            return 0;

        string classNameLower = source.GetItemClassName();
        classNameLower.ToLower();

        ExpansionMarketItem marketItem = ExpansionMarketCategory.GetGlobalItem(classNameLower, false);
        ExpansionMarketModule marketModule = ExpansionMarketModule.GetInstance();
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        int price = 0;

        if (marketItem && marketModule && m_TraderMarket && player)
        {
            bool includeAttachments = !m_BDEAttachmentFromItemEditor;
            marketModule.FindPriceOfPurchaseEx(marketItem, marketModule.GetClientZone(), m_TraderMarket, player, 1, price, includeAttachments);
        }

        return price;
    }

    string BDE_GetAttachmentPriceDelta(ExpansionMarketMenuItemManagerCategoryItem source, bool removing)
    {
        int price = 0;
        if (m_BDEAttachmentFromItemEditor && removing)
            price = BDE_GetInventoryAttachmentSellPrice(source.GetItemClassName());
        else
            price = BDE_GetAttachmentUnitPrice(source);

        if (price <= 0)
            return "";

        string result = "";
        if (removing)
            result += "-";
        else
            result += "+";

        result += GetDisplayPrice(price, true);
        return result;
    }

    protected void BDE_SetVehicleInfoTransparency(bool isVehicle)
    {
        if (m_BDEMenuInfoBackground)
        {
            if (isVehicle)
                m_BDEMenuInfoBackground.SetColor(ARGB(42, 3, 8, 14));
            else
                m_BDEMenuInfoBackground.SetColor(ARGB(252, 3, 8, 14));
        }

        if (m_BDEMarketItemHeaderPanel)
        {
            if (isVehicle)
                m_BDEMarketItemHeaderPanel.SetColor(ARGB(180, 4, 6, 10));
            else
                m_BDEMarketItemHeaderPanel.SetColor(ARGB(250, 4, 6, 10));
        }
    }

    void BDE_ShowItemCardTooltip(ExpansionMarketMenuItem sourceItem, Widget sourceWidget)
    {
        if (!sourceItem || !sourceWidget)
            return;

        ExpansionMarketItem marketItem = sourceItem.GetMarketItem();
        if (marketItem && marketItem.IsVehicle())
            return;

        EntityAI item = EntityAI.Cast(sourceItem.GetPreviewObject());
        if (!item || !InventoryItem.Cast(item))
            return;

        // Use Expansion's native tooltip instead of constructing a vanilla
        // inventory tooltip. Third-party mods that extend
        // ExpansionItemTooltip.Show(), such as full item-stat displays, are
        // therefore invoked exactly as they are in Expansion's stock market.
        BDE_HideItemCardTooltip();
        m_BDEItemTooltip = MissionGameplay.SetItemTooltip(item);
    }

    void BDE_HideItemCardTooltip()
    {
        if (!m_BDEItemTooltip)
            return;

        MissionGameplay.Expansion_DestroyItemTooltip();
        m_BDEItemTooltip = null;
    }

#ifdef BDE_HelicopterSling
    protected void BDE_OnSlingInfo(string className)
    {
        if (m_BDEClosing || !m_SelectedMarketItem || !m_SelectedMarketItem.IsVehicle() || !m_MarketMenuController)
            return;
        EntityAI preview;
        if (m_BDEVehiclePreview)
            preview = m_BDEVehiclePreview.GetPreviewEntity();
        string selectedClass = m_SelectedMarketItem.ClassName;
        if (preview)
            selectedClass = preview.GetType();
        if (selectedClass != className)
            return;
        m_MarketMenuController.MarketItemDesc = BDE_BuildVehicleInformation(selectedClass, preview);
        m_MarketMenuController.NotifyPropertyChanged("MarketItemDesc");
        UpdateItemDescriptionPanel();
    }
#endif

    protected string BDE_BuildVehicleInformation(string className, EntityAI vehicle)
    {
        if (vehicle)
            className = vehicle.GetType();
        if (className == string.Empty)
            return string.Empty;

        string configPath = "CfgVehicles " + className;
        if (!g_Game.ConfigIsExisting(configPath))
            return string.Empty;

        string information;

        int health;
        if (g_Game.ConfigIsExisting(configPath + " DamageSystem GlobalHealth Health hitpoints"))
            health = g_Game.ConfigGetInt(configPath + " DamageSystem GlobalHealth Health hitpoints");
        if (health > 0)
            information = BDE_AppendVehicleInfoLine(information, "HEALTH: " + health.ToString() + " HP");

        float fuelCapacity;
        if (g_Game.ConfigIsExisting(configPath + " fuelCapacity"))
            fuelCapacity = g_Game.ConfigGetFloat(configPath + " fuelCapacity");
        if (fuelCapacity > 0)
            information = BDE_AppendVehicleInfoLine(information, "FUEL: " + Math.Round(fuelCapacity).ToString() + " L");

        string driveType;
        if (g_Game.ConfigGetText(configPath + " SimulationModule drive", driveType) && driveType != string.Empty)
        {
            driveType.Replace("DRIVE_", "");
            information = BDE_AppendVehicleInfoLine(information, "DRIVE: " + driveType);
        }

        int seats = g_Game.ConfigGetChildrenCount(configPath + " Crew");
        if (seats > 0)
            information = BDE_AppendVehicleInfoLine(information, "SEATS: " + seats.ToString());

        float massKg;
        if (vehicle && dBodyIsSet(vehicle))
            massKg = dBodyGetMass(vehicle);
        if (massKg <= 0 && g_Game.ConfigIsExisting(configPath + " weight"))
            massKg = g_Game.ConfigGetFloat(configPath + " weight") / 1000.0;
#ifdef BDE_HelicopterSling
        BDE_EMU_SlingInfo slingInfo = BDE_EMU_SlingInfoModule.Lookup(className);
        if (slingInfo && slingInfo.WeightKg > 0)
            massKg = slingInfo.WeightKg;
#endif
        if (massKg > 0)
            information = BDE_AppendVehicleInfoLine(information, "WEIGHT: " + Math.Round(massKg).ToString() + " kg");
#ifdef BDE_HelicopterSling
        if (slingInfo && slingInfo.CapacityKg >= 0)
            information = BDE_AppendVehicleInfoLine(information, "MAX SLING LOAD: " + Math.Round(slingInfo.CapacityKg).ToString() + " kg");
#endif

        TFloatArray torqueCurve = new TFloatArray;
        if (g_Game.ConfigIsExisting(configPath + " SimulationModule Engine torqueCurve"))
            g_Game.ConfigGetFloatArray(configPath + " SimulationModule Engine torqueCurve", torqueCurve);

        float peakPowerKW;
        for (int i = 0; i + 1 < torqueCurve.Count(); i += 2)
        {
            float rpm = torqueCurve[i];
            float torque = torqueCurve[i + 1];
            float powerKW = (rpm * torque) / 9549.0;
            if (powerKW > peakPowerKW)
                peakPowerKW = powerKW;
        }

        if (peakPowerKW > 0)
        {
            string powerLine = "EST. POWER: " + Math.Round(peakPowerKW).ToString() + " kW";
            if (massKg > 0)
            {
                float powerToWeight = peakPowerKW / (massKg / 1000.0);
                powerLine += "  |  " + Math.Round(powerToWeight).ToString() + " kW/t";
            }
            information = BDE_AppendVehicleInfoLine(information, powerLine);
        }

        string weapons = BDE_BuildVehicleWeaponInformation(configPath);
        if (weapons != string.Empty)
            information = BDE_AppendVehicleInfoLine(information, "WEAPONS: " + weapons);

        TIntArray cargoSize = new TIntArray;
        if (g_Game.ConfigIsExisting(configPath + " Cargo itemsCargoSize"))
            g_Game.ConfigGetIntArray(configPath + " Cargo itemsCargoSize", cargoSize);
        if (cargoSize.Count() >= 2)
        {
            int cargoSlots = cargoSize[0] * cargoSize[1];
            information = BDE_AppendVehicleInfoLine(information, "CARGO: " + cargoSlots.ToString() + " slots");
        }

        string cargoMounts = BDE_BuildVehicleCargoMountInformation(configPath);
        if (cargoMounts != string.Empty)
            information = BDE_AppendVehicleInfoLine(information, "CARGO MOUNTS: " + cargoMounts);

        return information;
    }

    protected string BDE_BuildVehicleWeaponInformation(string configPath)
    {
        TStringArray slots = new TStringArray;
        if (!g_Game.ConfigIsExisting(configPath + " attachments"))
            return string.Empty;

        g_Game.ConfigGetTextArray(configPath + " attachments", slots);
        TStringArray weapons = new TStringArray;

        foreach (string slotName : slots)
        {
            string weaponName = BDE_GetVehicleWeaponName(slotName);
            if (weaponName != string.Empty && weapons.Find(weaponName) == -1)
                weapons.Insert(weaponName);
        }

        string result;
        for (int i = 0; i < weapons.Count(); i++)
        {
            if (result != string.Empty)
                result += "  |  ";
            result += weapons[i];
        }
        return result;
    }

    protected string BDE_GetVehicleWeaponName(string slotName)
    {
        string lower = slotName;
        lower.ToLower();

        if (lower == "fyg_120mm_slot" || lower == "van_m1a2_120mm")
            return "120mm Cannon";
        if (lower == "fyg_100mm_slot")
            return "100mm Cannon";
        if (lower == "fyg_30mm_slot")
            return "30mm Auto-Cannon";
        if (lower == "fyg_145mm_slot")
            return "14.5mm HMG";
        if (lower == "fyg_127mm_slot")
            return "12.7mm HMG";
        if (lower == "fyg_762mm_slot")
            return "7.62mm HMG";

        return string.Empty;
    }

    protected string BDE_BuildVehicleCargoMountInformation(string configPath)
    {
        TStringArray slots = new TStringArray;
        if (!g_Game.ConfigIsExisting(configPath + " attachments"))
            return string.Empty;

        g_Game.ConfigGetTextArray(configPath + " attachments", slots);

        int barrels;
        int crates;
        int backpacks;
        int cargoWeapons;
        int fuelCans;
        int weaponMounts;

        foreach (string slotName : slots)
        {
            if (BDE_GetVehicleWeaponName(slotName) != string.Empty)
                continue;

            string lower = slotName;
            lower.ToLower();

            if (lower.IndexOf("barrel") > -1 || lower.IndexOf("drum") > -1)
            {
                barrels++;
            }
            else if (lower.IndexOf("crate") > -1 || lower.IndexOf("case") > -1 || lower.IndexOf("seachest") > -1)
            {
                crates++;
            }
            else if (lower == "back" || lower.IndexOf("backpack") > -1)
            {
                backpacks++;
            }
            else if (lower == "shoulder" || lower.IndexOf("shoulder") > -1 || lower.IndexOf("melee") > -1)
            {
                cargoWeapons++;
            }
            else if (lower.IndexOf("canistergasoline") > -1 || lower.IndexOf("gascan") > -1 || lower.IndexOf("fuelcan") > -1)
            {
                fuelCans++;
            }
            else if (lower.IndexOf("weapon") > -1 || lower.IndexOf("rifle") > -1 || lower.IndexOf("gun") > -1 || lower.IndexOf("turret") > -1)
            {
                weaponMounts++;
            }
        }

        string result;
        result = BDE_AppendVehicleMount(result, barrels, "barrel", "barrels");
        result = BDE_AppendVehicleMount(result, crates, "crate", "crates");
        result = BDE_AppendVehicleMount(result, backpacks, "backpack", "backpacks");
        result = BDE_AppendVehicleMount(result, cargoWeapons, "weapon", "weapons");
        result = BDE_AppendVehicleMount(result, fuelCans, "fuel can", "fuel cans");
        result = BDE_AppendVehicleMount(result, weaponMounts, "weapon mount", "weapon mounts");
        return result;
    }

    protected string BDE_AppendVehicleMount(string current, int count, string singular, string plural)
    {
        if (count <= 0)
            return current;

        string label = plural;
        if (count == 1)
            label = singular;

        string value = count.ToString() + " " + label;
        if (current == string.Empty)
            return value;

        return current + "  |  " + value;
    }

    protected string BDE_AppendVehicleInfoLine(string current, string value)
    {
        if (value == string.Empty)
            return current;

        if (current == string.Empty)
            return value;

        return current + "\n" + value;
    }

    protected string BDE_GetClientSettingsFolder()
    {
        return "$profile:BDE_ExpansionMarket_UI";
    }

    protected string BDE_GetClientSettingsPath()
    {
        return BDE_GetClientSettingsFolder() + "\\ClientSettings.json";
    }

    protected void BDE_LoadSortPreference()
    {
        if (m_BDESortPreferenceLoaded)
            return;

        BDE_EMU_ClientSortSettings settings = new BDE_EMU_ClientSortSettings;
        string settingsPath = BDE_GetClientSettingsPath();
        bool hadSettings = FileExist(settingsPath);
        if (hadSettings)
            JsonFileLoader<BDE_EMU_ClientSortSettings>.JsonLoadFile(settingsPath, settings);

        if (settings.SortMode < 0 || settings.SortMode > 3)
            settings.SortMode = 2;

        m_BDESortMode = settings.SortMode;
        m_BDESortPreferenceLoaded = true;
        BDE_ApplySortMode(m_BDESortMode, true);

        if (!hadSettings)
            BDE_SaveSortPreference();
    }

    protected void BDE_SaveSortPreference()
    {
        BDE_EMU_ClientSortSettings settings = new BDE_EMU_ClientSortSettings;
        settings.SortMode = m_BDESortMode;
        ExpansionStatic.MakeDirectoryRecursive(BDE_GetClientSettingsFolder());
        JsonFileLoader<BDE_EMU_ClientSortSettings>.JsonSaveFile(BDE_GetClientSettingsPath(), settings);
    }

    protected void BDE_ApplySortMode(int mode, bool sortNow)
    {
        m_BDESortMode = mode;

        if (mode == 0 || mode == 1)
        {
            m_SortPriority = ExpansionMarketMenuSortPriority.NAME;
            m_NameSortState = (mode == 1);
            if (sort_item_name_text)
            {
                if (m_NameSortState)
                    sort_item_name_text.SetText("Z-A");
                else
                    sort_item_name_text.SetText("A-Z");
                sort_item_name_text.SetAlpha(1.0);
            }
            if (sort_item_name_icon)
            {
                if (m_NameSortState)
                    sort_item_name_icon.ClearFlags(WidgetFlags.FLIPV);
                else
                    sort_item_name_icon.SetFlags(WidgetFlags.FLIPV);
                sort_item_name_icon.SetAlpha(1.0);
            }
            if (sort_item_price_symbol)
                sort_item_price_symbol.SetAlpha(0.4);
            if (sort_item_price_icon)
                sort_item_price_icon.SetAlpha(0.4);
        }
        else
        {
            m_SortPriority = ExpansionMarketMenuSortPriority.PRICE;
            m_PriceSortState = (mode == 3);
            if (sort_item_name_text)
                sort_item_name_text.SetAlpha(0.7);
            if (sort_item_name_icon)
                sort_item_name_icon.SetAlpha(0.4);
            if (sort_item_price_symbol)
                sort_item_price_symbol.SetAlpha(1.0);
            if (sort_item_price_icon)
            {
                if (m_PriceSortState)
                    sort_item_price_icon.ClearFlags(WidgetFlags.FLIPV);
                else
                    sort_item_price_icon.SetFlags(WidgetFlags.FLIPV);
                sort_item_price_icon.SetAlpha(1.0);
            }
        }

        if (sortNow)
        {
            if (m_BDEAttachmentMode)
            {
                BDE_RebuildAttachmentItems();
            }
            else if (m_BDEItemEditorMode)
            {
                BDE_RebuildItemEditorItems();
            }
            else
            {
                SortCategoryItems();
                m_BDEItemSignature = "#FORCE#";
                g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_SynchronizeItems, 0, false);
            }
        }
    }

    protected void BDE_RecordCurrentSortPreference()
    {
        if (m_SortPriority == ExpansionMarketMenuSortPriority.NAME)
        {
            if (m_NameSortState)
                m_BDESortMode = 1;
            else
                m_BDESortMode = 0;
        }
        else
        {
            if (m_PriceSortState)
                m_BDESortMode = 3;
            else
                m_BDESortMode = 2;
        }

        BDE_SaveSortPreference();
    }

    override void OnNameSortClick()
    {
        if (m_BDEAttachmentMode || m_BDEItemEditorMode)
        {
            int mode = 0;
            if (m_BDESortMode == 0)
                mode = 1;
            BDE_ApplySortMode(mode, false);
            BDE_SaveSortPreference();
            if (m_BDEAttachmentMode)
                BDE_RebuildAttachmentItems();
            else
                BDE_RebuildItemEditorItems();
            return;
        }

        super.OnNameSortClick();
        BDE_RecordCurrentSortPreference();
        m_BDEItemSignature = "#FORCE#";
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_SynchronizeItems, 0, false);
    }

    override void OnPriceSortClick()
    {
        if (m_BDEAttachmentMode || m_BDEItemEditorMode)
        {
            int mode = 2;
            if (m_BDESortMode == 2)
                mode = 3;
            BDE_ApplySortMode(mode, false);
            BDE_SaveSortPreference();
            if (m_BDEAttachmentMode)
                BDE_RebuildAttachmentItems();
            else
                BDE_RebuildItemEditorItems();
            return;
        }

        super.OnPriceSortClick();
        BDE_RecordCurrentSortPreference();
        m_BDEItemSignature = "#FORCE#";
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_SynchronizeItems, 0, false);
    }

    override float GetUpdateTickRate()
    {
        return 0.10;
    }

    override void Expansion_Update()
    {
        super.Expansion_Update();
        BDE_UpdateTransactionLabels();
        if (m_BDEAttachmentMode && m_BDEAttachmentFromItemEditor)
            BDE_UpdateInventoryAttachmentTransactionSummary();

        m_BDESyncAccumulator += GetUpdateTickRate();
        if (m_BDESyncAccumulator >= 0.20)
        {
            m_BDESyncAccumulator = 0;
            BDE_SynchronizeCategories();
            BDE_SynchronizeItems();
        }
    }

    override bool OnMouseWheel(Widget w, int x, int y, int wheel)
    {
        if (m_BDEVehiclePreview && m_BDEVehiclePreview.IsVehiclePreviewActive() && m_BDEVehiclePreview.OwnsWidget(w))
        {
            m_BDEVehiclePreview.Rotate(wheel * 5.0);
            return true;
        }
        return super.OnMouseWheel(w, x, y, wheel);
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (m_BDEVehiclePreview && m_BDEVehiclePreview.IsVehiclePreviewActive() && m_BDEVehiclePreview.OwnsWidget(w))
            return true;
        return super.OnMouseButtonDown(w, x, y, button);
    }

    protected void BDE_DestroyCategoryEntries()
    {
        if (!m_MarketMenuController)
            return;
        for (int i = m_MarketMenuController.BDECategories.Count() - 1; i >= 0; i--)
        {
            BDE_EMU_CategoryEntry entry = m_MarketMenuController.BDECategories[i];
            if (entry)
                entry.Destroy();
        }
        m_MarketMenuController.BDECategories.Clear();
    }

    protected void BDE_DestroyItemCards()
    {
        BDE_HideItemCardTooltip();

        if (!m_MarketMenuController)
            return;
        for (int i = m_MarketMenuController.BDEItems.Count() - 1; i >= 0; i--)
        {
            BDE_EMU_ItemCard card = m_MarketMenuController.BDEItems[i];
            if (card)
                card.Destroy();
        }
        m_MarketMenuController.BDEItems.Clear();
    }

    override void OnHide()
    {
        m_BDEClosing = true;
#ifdef BDE_HelicopterSling
        BDE_EMU_SlingInfoModule.Changed.Remove(BDE_OnSlingInfo);
#endif

        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(BDE_InitializeInterface);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(BDE_ForceSynchronizeInterface);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(BDE_ReassertMenuControlLock);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(BDE_SynchronizeCategories);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(BDE_SynchronizeItems);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(BDE_RebuildItemEditorCategories);

        ExpansionMarketModule.SI_BDEInventoryAttachmentEditResult.Remove(BDE_OnInventoryAttachmentEditResult);
        BDE_DestroyItemCards();
        BDE_DestroyCategoryEntries();
        BDE_DestroyAttachmentTemporaryItems();
        BDE_DestroyItemEditorTemporaryItems();
        BDE_ClearInventoryEditorSelection();
        if (m_MarketMenuItemManager)
            m_MarketMenuItemManager.BDE_ClearEmbeddedAttachmentData();
        m_BDEAttachmentMode = false;
        m_BDEAttachmentFromItemEditor = false;
        m_BDEItemEditorMode = false;
        m_BDEInventoryAttachmentRequestPending = false;
        BDE_HideItemCardTooltip();
        BDE_SetVehicleInfoTransparency(false);
        m_BDEInterfaceInitialized = false;

        if (m_BDEVehiclePreview)
        {
            m_BDEVehiclePreview.Clear();
            m_BDEVehiclePreview = null;
        }

        super.OnHide();
    }
};

modded class ExpansionMarketMenuItem
{
    bool BDE_CanFastBuy()
    {
        if (!m_MarketModule)
            return false;

        return m_CanBuy && HasRepBuy() && m_ItemStock > 0 && m_BuyPrice > -1 && m_MarketModule.GetPlayerWorth() >= m_BuyPrice;
    }

    int BDE_GetSellablePrimaryStock()
    {
        if (!m_MarketModule)
            return 0;

        ExpansionMarketMenu marketMenu = GetMarketMenu();
        ExpansionMarketItem marketItem = GetMarketItem();
        if (!marketMenu || !marketItem)
            return 0;

        ExpansionTraderObjectBase trader = marketMenu.GetTraderObject();
        if (!trader)
            return 0;

        ExpansionMarketTrader traderMarket = trader.GetTraderMarket();
        if (!traderMarket)
            return 0;

        return m_MarketModule.BDE_EMU_GetSellablePrimaryAmount(marketItem, traderMarket, m_MarketModule.LocalGetEntityInventory());
    }

    bool BDE_CanFastSell()
    {
        return m_CanSell && m_HasRepSell && BDE_GetSellablePrimaryStock() > 0 && m_SellPrice > -1;
    }

    override void SetExpansionSkin(int skinIndex)
    {
        super.SetExpansionSkin(skinIndex);
        if (m_MarketMenu)
            m_MarketMenu.BDE_RefreshVehiclePreview();
    }
};

