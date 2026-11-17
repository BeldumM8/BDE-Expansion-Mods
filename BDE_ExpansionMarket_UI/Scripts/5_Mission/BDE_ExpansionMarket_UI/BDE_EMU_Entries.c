class BDE_EMU_CategoryEntryWidgetEventHandler: ScriptedWidgetEventHandler
{
    protected BDE_EMU_CategoryEntry m_Entry;

    void BDE_EMU_CategoryEntryWidgetEventHandler(BDE_EMU_CategoryEntry entry)
    {
        m_Entry = entry;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (m_Entry)
            return m_Entry.OnClick(w, x, y, button);
        return false;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (m_Entry)
            return m_Entry.OnMouseEnter(w, x, y);
        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (m_Entry)
            return m_Entry.OnMouseLeave(w, enterW, x, y);
        return false;
    }
};

class BDE_EMU_ItemCardWidgetEventHandler: ScriptedWidgetEventHandler
{
    protected BDE_EMU_ItemCard m_Card;

    void BDE_EMU_ItemCardWidgetEventHandler(BDE_EMU_ItemCard card)
    {
        m_Card = card;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (m_Card)
            return m_Card.OnClick(w, x, y, button);
        return false;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (m_Card)
            return m_Card.OnMouseEnter(w, x, y);
        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (m_Card)
            return m_Card.OnMouseLeave(w, enterW, x, y);
        return false;
    }
};

/**
 * One visual category entry for both normal market mode and attachment mode.
 *
 * Attachment mode deliberately uses the exact same layout/controller and the
 * exact same BDECategories collection as the normal market list.  Only the
 * source object and click target change.
 */
class BDE_EMU_CategoryEntry: ExpansionScriptView
{
    protected ExpansionMarketMenu m_Menu;
    protected ExpansionMarketMenuCategory m_SourceCategory;
    protected ExpansionMarketMenuItemManagerCategory m_AttachmentCategory;
    protected int m_BDEEditorCategory = -1;
    protected string m_BDEEditorCategoryName;
    protected string m_BDEEditorCategoryIcon;
    protected int m_BDEEditorCategoryCount;
    protected ref BDE_EMU_CategoryEntryController m_BDEController;
    protected Widget bde_category_border;
    protected Widget bde_category_background;
    protected ButtonWidget bde_category_button;
    protected bool m_Selected;
    protected ref BDE_EMU_CategoryEntryWidgetEventHandler m_BDEWidgetEventHandler;

    void BDE_EMU_CategoryEntry(ExpansionMarketMenu menu, ExpansionMarketMenuCategory sourceCategory)
    {
        m_Menu = menu;
        m_SourceCategory = sourceCategory;
        m_BDEController = BDE_EMU_CategoryEntryController.Cast(GetController());

        m_BDEWidgetEventHandler = new BDE_EMU_CategoryEntryWidgetEventHandler(this);
        if (bde_category_button)
            bde_category_button.SetHandler(m_BDEWidgetEventHandler);

        Refresh();
        SetSelected(false);
    }

    override string GetLayoutFile()
    {
        return "BDE_ExpansionMarket_UI/gui/layouts/BDE_EMU_CategoryEntry.layout";
    }

    override typename GetControllerType()
    {
        return BDE_EMU_CategoryEntryController;
    }

    void BDE_SetAttachmentCategorySource(ExpansionMarketMenuItemManagerCategory sourceCategory)
    {
        m_SourceCategory = null;
        m_AttachmentCategory = sourceCategory;
        m_BDEEditorCategory = -1;
        Refresh();
        SetSelected(false);
    }

    void BDE_SetEditorCategorySource(int categoryID, string categoryName, string categoryIcon, int itemCount)
    {
        m_SourceCategory = null;
        m_AttachmentCategory = null;
        m_BDEEditorCategory = categoryID;
        m_BDEEditorCategoryName = categoryName;
        m_BDEEditorCategoryIcon = categoryIcon;
        m_BDEEditorCategoryCount = itemCount;
        Refresh();
        SetSelected(false);
    }

    bool BDE_IsEditorCategory()
    {
        return m_BDEEditorCategory > -1;
    }

    int BDE_GetEditorCategoryID()
    {
        return m_BDEEditorCategory;
    }

    bool BDE_IsAttachmentCategory()
    {
        return m_AttachmentCategory != null;
    }

    void Refresh()
    {
        if (!m_BDEController)
            return;

        if (m_BDEEditorCategory > -1)
        {
            m_BDEController.CategoryName = m_BDEEditorCategoryName;
            m_BDEController.CategoryIcon = m_BDEEditorCategoryIcon;
            m_BDEController.CategoryInfo = m_BDEEditorCategoryCount.ToString() + " ITEMS";
            m_BDEController.NotifyPropertiesChanged({"CategoryName", "CategoryIcon", "CategoryInfo"});
            return;
        }

        if (m_AttachmentCategory)
        {
            m_BDEController.CategoryName = m_AttachmentCategory.BDE_GetCategoryName();
            m_BDEController.CategoryIcon = m_AttachmentCategory.BDE_GetCategoryIcon();
            m_BDEController.CategoryInfo = m_AttachmentCategory.BDE_GetAttachmentCount().ToString() + " ITEMS";
            m_BDEController.NotifyPropertiesChanged({"CategoryName", "CategoryIcon", "CategoryInfo"});
            return;
        }

        if (!m_SourceCategory)
            return;

        ExpansionMarketMenuCategoryController sourceController = m_SourceCategory.GetCategoryController();
        if (!sourceController)
            return;

        m_BDEController.CategoryName = sourceController.CategoryName;
        m_BDEController.CategoryIcon = sourceController.CategoryIcon;
        m_BDEController.CategoryInfo = sourceController.CategoryInfo;
        m_BDEController.NotifyPropertiesChanged({"CategoryName", "CategoryIcon", "CategoryInfo"});
    }

    void SelectCategory()
    {
        if (!m_Menu)
            return;

        if (m_BDEEditorCategory > -1)
            m_Menu.BDE_SelectEditorCategorySource(m_BDEEditorCategory);
        else if (m_AttachmentCategory)
            m_Menu.BDE_SelectAttachmentCategorySource(m_AttachmentCategory);
        else if (m_SourceCategory)
            m_Menu.BDE_SelectCategorySource(m_SourceCategory);
    }

    void SetSelected(bool selected)
    {
        m_Selected = selected;

        if (selected)
        {
            if (bde_category_border)
                bde_category_border.SetColor(ARGB(255, 35, 126, 191));
            if (bde_category_background)
                bde_category_background.SetColor(ARGB(255, 8, 28, 44));
        }
        else
        {
            if (bde_category_border)
                bde_category_border.SetColor(ARGB(255, 10, 35, 54));
            if (bde_category_background)
                bde_category_background.SetColor(ARGB(255, 3, 8, 14));
        }
    }

    bool IsSelected()
    {
        return m_Selected;
    }

    ExpansionMarketMenuCategory GetSourceCategory()
    {
        return m_SourceCategory;
    }

    ExpansionMarketMenuItemManagerCategory BDE_GetAttachmentCategorySource()
    {
        return m_AttachmentCategory;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (w == bde_category_button && button == MouseState.LEFT)
        {
            SelectCategory();
            return true;
        }

        return false;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (w == bde_category_button && !m_Selected && bde_category_background)
        {
            bde_category_background.SetColor(ARGB(255, 6, 19, 31));
            return true;
        }

        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (w == bde_category_button && !m_Selected && bde_category_background)
        {
            bde_category_background.SetColor(ARGB(255, 3, 8, 14));
            return true;
        }

        return false;
    }
};

class BDE_EMU_CategoryEntryController: ExpansionViewController
{
    string CategoryName;
    string CategoryIcon;
    string CategoryInfo;
};

/**
 * The single product card used everywhere.
 *
 * Normal products and compatible attachments both use this exact ScriptView,
 * layout, controller, ItemPreviewWidget binding and source preview object.
 * Attachment mode only changes the displayed status/action text and what a
 * left click does.
 */
class BDE_EMU_ItemCard: ExpansionScriptView
{
    protected ExpansionMarketMenu m_Menu;
    protected ExpansionMarketMenuItem m_SourceItem;
    protected ExpansionMarketMenuItemManagerCategoryItem m_AttachmentSource;
    protected EntityAI m_BDEInventoryItem;
    protected ref BDE_EMU_ItemCardController m_BDEController;
    protected Widget bde_item_border;
    protected Widget bde_item_background;
    protected ItemPreviewWidget bde_item_preview;
    protected ButtonWidget bde_item_button;
    protected Widget bde_item_sell_action;
    protected Widget bde_item_buy_action;
    protected ButtonWidget bde_item_sell_button;
    protected ButtonWidget bde_item_buy_button;
    protected TextWidget bde_item_name;
    protected TextWidget bde_item_sell_price;
    protected TextWidget bde_item_buy_price;
    protected TextWidget bde_item_market_stock;
    protected TextWidget bde_item_player_stock;
    protected ImageWidget bde_item_sell_currency;
    protected ImageWidget bde_item_buy_currency;
    protected TextWidget bde_attachment_price;
    protected TextWidget bde_attachment_action;
    protected bool m_Selected;
    protected ref BDE_EMU_ItemCardWidgetEventHandler m_BDEWidgetEventHandler;

    void BDE_EMU_ItemCard(ExpansionMarketMenu menu, ExpansionMarketMenuItem sourceItem)
    {
        m_Menu = menu;
        m_SourceItem = sourceItem;
        m_BDEController = BDE_EMU_ItemCardController.Cast(GetController());

        m_BDEWidgetEventHandler = new BDE_EMU_ItemCardWidgetEventHandler(this);
        if (bde_item_button)
            bde_item_button.SetHandler(m_BDEWidgetEventHandler);
        if (bde_item_sell_button)
            bde_item_sell_button.SetHandler(m_BDEWidgetEventHandler);
        if (bde_item_buy_button)
            bde_item_buy_button.SetHandler(m_BDEWidgetEventHandler);

        Refresh();
        SetSelected(false);
    }

    void ~BDE_EMU_ItemCard()
    {
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(BDE_ApplyAttachmentPreview);
    }

    override string GetLayoutFile()
    {
        return "BDE_ExpansionMarket_UI/gui/layouts/BDE_EMU_ItemCard.layout";
    }

    override typename GetControllerType()
    {
        return BDE_EMU_ItemCardController;
    }

    void BDE_SetAttachmentSource(ExpansionMarketMenuItemManagerCategoryItem source)
    {
        m_BDEInventoryItem = null;
        m_AttachmentSource = source;
        Refresh();
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_RefreshAttachmentCompatibility, 0, false);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_RefreshAttachmentCompatibility, 50, false);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_ApplyAttachmentPreview, 0, false);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(BDE_ApplyAttachmentPreview, 50, false);
    }

    protected void BDE_RefreshAttachmentCompatibility()
    {
        if (!m_AttachmentSource)
            return;

        m_AttachmentSource.BDE_RefreshAttachmentCompatibility();
        Refresh();
    }

    void BDE_SetInventoryItemSource(EntityAI item)
    {
        m_AttachmentSource = null;
        m_BDEInventoryItem = item;
        Refresh();
    }

    EntityAI BDE_GetInventoryItem()
    {
        return m_BDEInventoryItem;
    }

    bool BDE_IsInventoryItemCard()
    {
        return m_BDEInventoryItem != null;
    }

    protected void BDE_ApplyAttachmentPreview()
    {
        if (!m_AttachmentSource || !m_SourceItem || !bde_item_preview)
            return;

        EntityAI preview = EntityAI.Cast(m_SourceItem.GetPreviewObject());
        if (!preview)
            return;

        int viewIndex = preview.GetViewIndex();
        if (viewIndex < 0)
            viewIndex = 0;

        bde_item_preview.SetItem(preview);
        bde_item_preview.SetView(viewIndex);
        bde_item_preview.Show(true, true);
    }

    bool BDE_IsAttachmentCard()
    {
        return m_AttachmentSource != null;
    }

    ExpansionMarketMenuItemManagerCategoryItem BDE_GetAttachmentSource()
    {
        return m_AttachmentSource;
    }

    void Refresh()
    {
        if (!m_SourceItem || !m_BDEController)
            return;

        if (!m_SourceItem.GetPreviewObject())
            m_SourceItem.UpdatePreviewObject();

        if (m_AttachmentSource && !m_SourceItem.GetPreviewObject())
            Print("[BDE Market UI] Normal market preview source is NULL for attachment " + m_AttachmentSource.GetItemClassName());

        if (m_BDEInventoryItem)
        {
            int attachmentCount = m_BDEInventoryItem.GetInventory().AttachmentCount();
            m_BDEController.ItemName = ExpansionStatic.GetItemDisplayNameWithType(m_BDEInventoryItem.GetType());
            if (bde_item_name)
                bde_item_name.SetText(m_BDEController.ItemName);
            m_BDEController.ItemMarketStock = attachmentCount.ToString() + " ATTACHMENTS";
            m_BDEController.ItemPlayerStock = "IN INVENTORY";
            m_BDEController.ItemSellPrice = "";
            m_BDEController.ItemBuyPrice = "";
            m_BDEController.CurrencyBuyIcon = "";
            m_BDEController.CurrencySellIcon = "";
            m_BDEController.Preview = m_SourceItem.GetPreviewObject();
            m_BDEController.NotifyPropertiesChanged({"ItemName", "ItemBuyPrice", "ItemSellPrice", "ItemMarketStock", "ItemPlayerStock", "CurrencyBuyIcon", "CurrencySellIcon", "Preview"});
            BDE_UpdateTransactionButtons();
            return;
        }

        if (m_AttachmentSource)
        {
            int count = m_AttachmentSource.GetCount();

            if (m_Menu && m_Menu.BDE_IsInventoryAttachmentEditorActive() && !m_Menu.BDE_UseMarketStyleInventoryWeaponEditor())
            {
                bool showRemove = m_Menu.BDE_ShouldShowInventoryAttachmentRemove(m_AttachmentSource);
                bool showAdd = m_Menu.BDE_ShouldShowInventoryAttachmentAdd(m_AttachmentSource);
                bool canRemove = m_Menu.BDE_CanRemoveInventoryAttachment(m_AttachmentSource);
                bool canAdd = m_Menu.BDE_CanAddInventoryAttachment(m_AttachmentSource);
                bool cancelAddition = m_Menu.BDE_InventoryAttachmentRemovalCancelsAddition(m_AttachmentSource);

                // Keep every Expansion candidate card visible. Transaction
                // availability controls the action state/color, not whether
                // the attachment disappears from the category.
                GetLayoutRoot().Show(true);
                m_BDEController.ItemName = ExpansionStatic.GetItemDisplayNameWithType(m_AttachmentSource.GetItemClassName());
                if (bde_item_name)
                    bde_item_name.SetText(m_BDEController.ItemName);
                if (showRemove)
                    m_BDEController.ItemMarketStock = "REMOVE x" + count.ToString();
                else if (count > 0)
                    m_BDEController.ItemMarketStock = "ATTACHED x" + count.ToString();
                else
                    m_BDEController.ItemMarketStock = "";

                if (showAdd)
                    m_BDEController.ItemPlayerStock = "ADD";
                else
                    m_BDEController.ItemPlayerStock = "";

                m_BDEController.ItemSellPrice = m_Menu.BDE_GetInventoryAttachmentRemoveDisplayPrice(m_AttachmentSource);
                m_BDEController.ItemBuyPrice = m_Menu.BDE_GetInventoryAttachmentBuyDisplayPrice(m_AttachmentSource);
                if (showRemove && !cancelAddition && m_BDEController.ItemSellPrice != "N/A")
                    m_BDEController.CurrencySellIcon = m_Menu.BDE_GetMarketCurrencyIcon();
                else
                    m_BDEController.CurrencySellIcon = "";
                if (showAdd && m_BDEController.ItemBuyPrice != "N/A")
                    m_BDEController.CurrencyBuyIcon = m_Menu.BDE_GetMarketCurrencyIcon();
                else
                    m_BDEController.CurrencyBuyIcon = "";
                m_BDEController.Preview = m_SourceItem.GetPreviewObject();
                m_BDEController.NotifyPropertiesChanged({"ItemName", "ItemBuyPrice", "ItemSellPrice", "ItemMarketStock", "ItemPlayerStock", "CurrencyBuyIcon", "CurrencySellIcon", "Preview"});
                BDE_ApplyAttachmentPreview();

                SetSelected(count > 0);
                BDE_UpdateTransactionButtons();
                return;
            }

            string status;
            string action;
            if (count > 0)
            {
                status = "ATTACHED x" + count.ToString();
                action = "REMOVE";
            }
            else if (m_AttachmentSource.BDE_CanAddAttachment())
            {
                status = "AVAILABLE";
                action = "ADD";
            }
            else
            {
                status = "INCOMPATIBLE";
                action = "";
            }

            m_BDEController.ItemName = ExpansionStatic.GetItemDisplayNameWithType(m_AttachmentSource.GetItemClassName());
            if (bde_item_name)
                bde_item_name.SetText(m_BDEController.ItemName);
            m_BDEController.ItemMarketStock = status;
            m_BDEController.ItemPlayerStock = "";
            m_BDEController.ItemSellPrice = m_Menu.BDE_GetAttachmentPriceDelta(m_AttachmentSource, count > 0);
            m_BDEController.ItemBuyPrice = action;
            m_BDEController.CurrencyBuyIcon = "";
            m_BDEController.CurrencySellIcon = "";
            m_BDEController.Preview = m_SourceItem.GetPreviewObject();
            m_BDEController.NotifyPropertiesChanged({"ItemName", "ItemBuyPrice", "ItemSellPrice", "ItemMarketStock", "ItemPlayerStock", "CurrencyBuyIcon", "CurrencySellIcon", "Preview"});
            BDE_ApplyAttachmentPreview();

            SetSelected(count > 0);
            BDE_UpdateTransactionButtons();
            return;
        }

        ExpansionMarketMenuItemController sourceController = ExpansionMarketMenuItemController.Cast(m_SourceItem.GetController());
        if (!sourceController)
            return;

        m_BDEController.ItemName = m_Menu.GetDisplayName(m_Menu.GetPreviewClassName(m_SourceItem.GetMarketItem().ClassName, true));
        if (bde_item_name)
            bde_item_name.SetText(m_BDEController.ItemName);
        m_BDEController.ItemBuyPrice = sourceController.ItemBuyPrice;
        m_BDEController.ItemSellPrice = sourceController.ItemSellPrice;

        int requiredRep = BDE_EMU_ReputationAccess.GetDisplayRequirement(m_SourceItem.GetMarketItem(), m_Menu.GetMarketTrader());
        if (requiredRep > 0 && !m_SourceItem.HasRepBuy())
        {
            m_BDEController.ItemMarketStock = string.Format("%1RP", ExpansionStatic.FormatInt(requiredRep));
        }
        else
        {
            m_BDEController.ItemMarketStock = sourceController.ItemMarketStock;
        }

        if (m_SourceItem.CanSell())
            m_BDEController.ItemPlayerStock = m_SourceItem.BDE_GetSellablePrimaryStock().ToString() + " #STR_EXPANSION_MARKET_ITEM_ONPLAYER";
        else
            m_BDEController.ItemPlayerStock = sourceController.ItemPlayerStock;
        m_BDEController.CurrencyBuyIcon = sourceController.CurrencyBuyIcon;
        m_BDEController.CurrencySellIcon = sourceController.CurrencySellIcon;
        m_BDEController.Preview = m_SourceItem.GetPreviewObject();
        m_BDEController.NotifyPropertiesChanged({"ItemName", "ItemBuyPrice", "ItemSellPrice", "ItemMarketStock", "ItemPlayerStock", "CurrencyBuyIcon", "CurrencySellIcon", "Preview"});
        BDE_UpdateTransactionButtons();
    }

    protected void BDE_UpdateTransactionButtons()
    {
        if (!m_SourceItem)
            return;

        bool inventoryCard = m_BDEInventoryItem != null;
        if (inventoryCard)
        {
            if (bde_item_sell_action)
                bde_item_sell_action.Show(false);
            if (bde_item_buy_action)
                bde_item_buy_action.Show(false);
            if (bde_item_sell_button)
                bde_item_sell_button.Enable(false);
            if (bde_item_buy_button)
                bde_item_buy_button.Enable(false);
            if (bde_attachment_price)
                bde_attachment_price.Show(false);
            if (bde_attachment_action)
                bde_attachment_action.Show(false);
            return;
        }

        bool attachmentCard = m_AttachmentSource != null;
        if (attachmentCard)
        {
            if (m_Menu && m_Menu.BDE_IsInventoryAttachmentEditorActive() && !m_Menu.BDE_UseMarketStyleInventoryWeaponEditor())
            {
                bool showRemove = m_Menu.BDE_ShouldShowInventoryAttachmentRemove(m_AttachmentSource);
                bool showAdd = m_Menu.BDE_ShouldShowInventoryAttachmentAdd(m_AttachmentSource);
                bool canRemove = m_Menu.BDE_CanRemoveInventoryAttachment(m_AttachmentSource);
                bool canAdd = m_Menu.BDE_CanAddInventoryAttachment(m_AttachmentSource);

                // Do not hide unavailable attachment candidates. Leave the
                // card visible and communicate availability through the
                // enabled state and red/normal action styling instead.
                GetLayoutRoot().Show(true);
                if (bde_attachment_price)
                    bde_attachment_price.Show(false);
                if (bde_attachment_action)
                    bde_attachment_action.Show(false);
                if (bde_item_sell_action)
                    bde_item_sell_action.Show(showRemove);
                if (bde_item_buy_action)
                    bde_item_buy_action.Show(showAdd);
                if (bde_item_sell_button)
                    bde_item_sell_button.Enable(canRemove);
                if (bde_item_buy_button)
                    bde_item_buy_button.Enable(canAdd);

                int attachmentUnavailableColor = ARGB(255, 235, 75, 75);
                int attachmentSellAvailableColor = ARGB(255, 209, 232, 255);
                int attachmentBuyAvailableColor = ARGB(255, 199, 255, 219);
                if (bde_item_market_stock)
                {
                    if (canRemove)
                        bde_item_market_stock.SetColor(attachmentSellAvailableColor);
                    else
                        bde_item_market_stock.SetColor(attachmentUnavailableColor);
                }
                if (bde_item_player_stock)
                {
                    if (canAdd)
                        bde_item_player_stock.SetColor(attachmentBuyAvailableColor);
                    else
                        bde_item_player_stock.SetColor(attachmentUnavailableColor);
                }
                if (bde_item_sell_price)
                {
                    if (canRemove)
                        bde_item_sell_price.SetColor(attachmentSellAvailableColor);
                    else
                        bde_item_sell_price.SetColor(attachmentUnavailableColor);
                }
                if (bde_item_sell_currency)
                {
                    if (canRemove)
                        bde_item_sell_currency.SetColor(attachmentSellAvailableColor);
                    else
                        bde_item_sell_currency.SetColor(attachmentUnavailableColor);
                }
                if (bde_item_buy_price)
                {
                    if (canAdd)
                        bde_item_buy_price.SetColor(attachmentBuyAvailableColor);
                    else
                        bde_item_buy_price.SetColor(attachmentUnavailableColor);
                }
                if (bde_item_buy_currency)
                {
                    if (canAdd)
                        bde_item_buy_currency.SetColor(attachmentBuyAvailableColor);
                    else
                        bde_item_buy_currency.SetColor(attachmentUnavailableColor);
                }
                if (bde_item_sell_action)
                {
                    if (canRemove)
                        bde_item_sell_action.SetAlpha(1.0);
                    else
                        bde_item_sell_action.SetAlpha(0.45);
                }
                if (bde_item_buy_action)
                {
                    if (canAdd)
                        bde_item_buy_action.SetAlpha(1.0);
                    else
                        bde_item_buy_action.SetAlpha(0.45);
                }
                return;
            }

            if (bde_item_sell_action)
                bde_item_sell_action.Show(false);
            if (bde_item_buy_action)
                bde_item_buy_action.Show(false);
            if (bde_item_sell_button)
                bde_item_sell_button.Enable(false);
            if (bde_item_buy_button)
                bde_item_buy_button.Enable(false);
            if (bde_attachment_price)
                bde_attachment_price.Show(true);
            if (bde_attachment_action)
                bde_attachment_action.Show(true);
            return;
        }

        if (bde_attachment_price)
            bde_attachment_price.Show(false);
        if (bde_attachment_action)
            bde_attachment_action.Show(false);

        ExpansionMarketMenuItemController sourceController = ExpansionMarketMenuItemController.Cast(m_SourceItem.GetController());
        bool showSell = false;
        bool showBuy = false;
        if (sourceController)
        {
            showSell = m_SourceItem.CanSell() && sourceController.ItemSellPrice != string.Empty;
            showBuy = m_SourceItem.CanBuy() && sourceController.ItemBuyPrice != string.Empty;
        }

        if (bde_item_sell_action)
            bde_item_sell_action.Show(showSell);
        if (bde_item_buy_action)
            bde_item_buy_action.Show(showBuy);

        bool canSell = showSell && m_SourceItem.BDE_CanFastSell();
        bool canBuy = showBuy && m_SourceItem.BDE_CanFastBuy();

        int unavailableColor = ARGB(255, 235, 75, 75);
        int sellAvailableColor = ARGB(255, 209, 232, 255);
        int buyAvailableColor = ARGB(255, 199, 255, 219);

        if (bde_item_sell_price)
        {
            if (canSell)
                bde_item_sell_price.SetColor(sellAvailableColor);
            else
                bde_item_sell_price.SetColor(unavailableColor);
        }
        if (bde_item_sell_currency)
        {
            if (canSell)
                bde_item_sell_currency.SetColor(sellAvailableColor);
            else
                bde_item_sell_currency.SetColor(unavailableColor);
        }
        if (bde_item_buy_price)
        {
            if (canBuy)
                bde_item_buy_price.SetColor(buyAvailableColor);
            else
                bde_item_buy_price.SetColor(unavailableColor);
        }
        if (bde_item_buy_currency)
        {
            if (canBuy)
                bde_item_buy_currency.SetColor(buyAvailableColor);
            else
                bde_item_buy_currency.SetColor(unavailableColor);
        }

        if (bde_item_sell_button)
            bde_item_sell_button.Enable(canSell);
        if (bde_item_buy_button)
            bde_item_buy_button.Enable(canBuy);
        if (bde_item_sell_action)
        {
            if (canSell)
                bde_item_sell_action.SetAlpha(1.0);
            else
                bde_item_sell_action.SetAlpha(0.45);
        }

        if (bde_item_buy_action)
        {
            if (canBuy)
                bde_item_buy_action.SetAlpha(1.0);
            else
                bde_item_buy_action.SetAlpha(0.45);
        }
    }

    void SelectItem()
    {
        if (!m_Menu)
            return;

        if (m_BDEInventoryItem)
            m_Menu.BDE_SelectInventoryEditorItem(m_BDEInventoryItem);
        else if (m_AttachmentSource)
            m_Menu.BDE_ToggleAttachmentSource(m_AttachmentSource);
        else if (m_SourceItem)
            m_Menu.BDE_SelectMarketItem(m_SourceItem);
    }

    void SetSelected(bool selected)
    {
        m_Selected = selected;

        if (selected)
        {
            if (bde_item_border)
                bde_item_border.SetColor(ARGB(255, 35, 126, 191));
            if (bde_item_background)
                bde_item_background.SetColor(ARGB(255, 7, 24, 38));
        }
        else
        {
            if (bde_item_border)
                bde_item_border.SetColor(ARGB(255, 9, 33, 51));
            if (bde_item_background)
                bde_item_background.SetColor(ARGB(255, 2, 7, 12));
        }
    }

    ExpansionMarketMenuItem GetSourceItem()
    {
        return m_SourceItem;
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (button == MouseState.LEFT && w == bde_item_sell_button)
        {
            if (m_AttachmentSource && m_Menu && m_Menu.BDE_IsInventoryAttachmentEditorActive() && !m_Menu.BDE_UseMarketStyleInventoryWeaponEditor())
            {
                m_Menu.BDE_HideItemCardTooltip();
                m_Menu.BDE_RemoveInventoryAttachment(m_AttachmentSource);
            }
            else if (!m_AttachmentSource && !m_BDEInventoryItem && m_SourceItem && m_SourceItem.BDE_CanFastSell())
            {
                if (m_Menu)
                    m_Menu.BDE_HideItemCardTooltip();
                m_Menu.BDE_SellMarketItemFromCard(m_SourceItem);
            }
            return true;
        }

        if (button == MouseState.LEFT && w == bde_item_buy_button)
        {
            if (m_AttachmentSource && m_Menu && m_Menu.BDE_IsInventoryAttachmentEditorActive() && !m_Menu.BDE_UseMarketStyleInventoryWeaponEditor())
            {
                m_Menu.BDE_HideItemCardTooltip();
                m_Menu.BDE_AddInventoryAttachment(m_AttachmentSource);
            }
            else if (!m_AttachmentSource && !m_BDEInventoryItem && m_SourceItem && m_SourceItem.BDE_CanFastBuy())
            {
                if (m_Menu)
                    m_Menu.BDE_HideItemCardTooltip();
                m_Menu.BDE_BuyMarketItemFromCard(m_SourceItem);
            }
            return true;
        }

        if (w == bde_item_button && button == MouseState.LEFT)
        {
            if (m_AttachmentSource && m_Menu && m_Menu.BDE_IsInventoryAttachmentEditorActive() && !m_Menu.BDE_UseMarketStyleInventoryWeaponEditor())
                return true;

            SelectItem();
            return true;
        }

        if (w == bde_item_button && button == MouseState.MIDDLE)
        {
            Object preview;
            if (m_BDEInventoryItem)
                preview = m_BDEInventoryItem;
            else if (m_SourceItem)
                preview = m_SourceItem.GetPreviewObject();
            if (preview && m_Menu)
                MissionGameplay.InspectItem(m_Menu, preview);
            return true;
        }

        return false;
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        if (w == bde_item_button || w == bde_item_sell_button || w == bde_item_buy_button)
        {
            if (w == bde_item_button && !m_Selected && bde_item_background)
                bde_item_background.SetColor(ARGB(255, 5, 16, 26));

            if (m_Menu)
            {
                if (m_BDEInventoryItem)
                    m_Menu.BDE_ShowInventoryItemCardTooltip(m_BDEInventoryItem, w);
                else
                    m_Menu.BDE_ShowItemCardTooltip(m_SourceItem, w);
            }
            return true;
        }

        return false;
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        if (w == bde_item_button || w == bde_item_sell_button || w == bde_item_buy_button)
        {
            if (w == bde_item_button && !m_Selected && bde_item_background)
                bde_item_background.SetColor(ARGB(255, 2, 7, 12));

            if (m_Menu)
                m_Menu.BDE_HideItemCardTooltip();
            return true;
        }

        return false;
    }
};

class BDE_EMU_ItemCardController: ExpansionViewController
{
    string ItemName;
    string ItemBuyPrice;
    string ItemSellPrice;
    string ItemMarketStock;
    string ItemPlayerStock;
    string CurrencyBuyIcon;
    string CurrencySellIcon;
    Object Preview;
};
