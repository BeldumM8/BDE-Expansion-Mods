class BDE_EMU_LoadoutSlot
{
    EntityAI Parent;
    int SlotId;
    string Path;
    Widget Tile;
    ref array<int> Slots = new array<int>;
}

class BDE_EMU_LoadoutCard
{
    Widget Root;
    EntityAI Model;
    bool PreviewAttempted;
    bool Compatible = true;
    ref BDE_EMU_LoadoutOffer Offer;
    ref BDE_EMU_SavedLoadout Preset;
}

class BDE_EMU_LoadoutMenu: UIScriptedMenu
{
    protected ref array<ref BDE_EMU_LoadoutChange> m_Changes = new array<ref BDE_EMU_LoadoutChange>;
    protected static ref BDE_EMU_LoadoutMenu s_ActiveMenu;
    protected Object m_Station;
    protected PlayerBase m_Player;
    protected ExpansionMarketModule m_Module;
    protected Widget m_Slots;
    protected Widget m_ChildSlots;
    protected EntityAI m_BrowseParent;
    protected ref BDE_EMU_LoadoutSlot m_Selection;
    protected XComboBoxWidget m_GroupPicker;
    protected ref TStringArray m_GroupKeys = new TStringArray;
    protected bool m_UpdatingGroups;
    protected Widget m_SearchHint;
    protected MultilineTextWidget m_Description;
    protected bool m_InputsExcluded;
    protected int m_DragX;
    protected float m_DragYaw;
    protected Widget m_Content;
    protected TextWidget m_Status;
    protected TextWidget m_Path;
    protected TextWidget m_PreviewTitle;
    protected EditBoxWidget m_Search;
    protected XComboBoxWidget m_PreviewMode;
    protected XComboBoxWidget m_SavedLoadouts;
    protected XComboBoxWidget m_SavedItems;
    protected EditBoxWidget m_LoadoutName;
    protected EditBoxWidget m_ItemPresetName;
    protected ref BDE_EMU_LoadoutPresets m_Presets;
    protected bool m_PresetsWritable;
    protected int m_ConfirmToken;
    protected float m_ConfirmRemaining;
    protected int m_ConfirmTotal;
    protected bool m_HudHidden;
    protected float m_PreviewScale = 1;
    protected string m_RestoreGroup;
    protected ref map<string, string> m_SlotCategories = new map<string, string>;
    protected ref BDE_EMU_LoadoutSlot m_ContextSlot;
    protected Widget m_ContextMenu;
    protected ref BDE_EMU_SavedLoadout m_DeleteConfiguration;
    protected ref array<ref BDE_EMU_AmmoChange> m_AmmoChanges = new array<ref BDE_EMU_AmmoChange>;
    protected ref array<int> m_AmmoContextPath = new array<int>;
    protected int m_AmmoRequest;
    protected bool m_AmmoPending;
    protected float m_AmmoWait;
    protected ref array<int> m_ContextPath = new array<int>;
    protected ref array<ref BDE_EMU_SavedLoadout> m_ContextPresets = new array<ref BDE_EMU_SavedLoadout>;
    protected ref array<ref BDE_EMU_PresetJob> m_PresetJobs = new array<ref BDE_EMU_PresetJob>;
    protected ref BDE_EMU_PresetJob m_PresetJob;
    protected bool m_ApplyingPreset;
    protected float m_PresetWait;
    protected int m_PresetApplied;
    protected int m_PresetSkipped;
    protected string m_PresetSummary;
    protected PlayerPreviewWidget m_PlayerPreview;
    protected ItemPreviewWidget m_ItemPreview;
    protected ButtonWidget m_Buy;
    protected ref array<ref BDE_EMU_LoadoutSlot> m_Rows = new array<ref BDE_EMU_LoadoutSlot>;
    protected ref array<ref BDE_EMU_LoadoutCard> m_Cards = new array<ref BDE_EMU_LoadoutCard>;
    protected ref array<Widget> m_Headers = new array<Widget>;
    protected ref map<EntityAI, EntityAI> m_CloneMap = new map<EntityAI, EntityAI>;
    protected ref BDE_EMU_LoadoutOffer m_SelectedOffer;
    protected EntityAI m_PlayerClone;
    protected EntityAI m_CompatibilityClone;
    protected EntityAI m_CompatibilityParent;
    protected EntityAI m_ItemClone;
    protected float m_PreviewRefreshTime;
    protected int m_Request;
    protected bool m_Busy;
    protected bool m_Closed;
    protected bool m_Rebuilding;
    protected int m_Page;
    protected float m_CheckTime;
    protected string m_EquipmentSignature;
    protected float m_PreviewYaw;

    static void Open(Object station)
    {
        if (!station)
        {
            return;
        }
        if (GetGame().GetUIManager().GetMenu())
        {
            Print("[BDE Loadout] Cannot open while another menu is active.");
            return;
        }
        Print("[BDE Loadout] Client action executed; opening menu.");
        // Keep the menu alive after Open returns, as in BDE RecruitAI.
        s_ActiveMenu = new BDE_EMU_LoadoutMenu;
        s_ActiveMenu.SetStation(station);
        GetGame().GetUIManager().ShowScriptedMenu(s_ActiveMenu, null);
    }

    static void ReleaseMenu()
    {
        if (s_ActiveMenu)
        {
            s_ActiveMenu.Close();
            s_ActiveMenu = null;
        }
    }

    void SetStation(Object station)
    {
        m_Station = station;
    }

    override Widget Init()
    {
        layoutRoot = GetGame().GetWorkspace().CreateWidgets("BDE_ExpansionMarket_UI/gui/layouts/BDE_EMU_Loadout.layout");
        if (!layoutRoot)
        {
            Error("[BDE Loadout] Cannot create BDE_EMU_Loadout.layout; check the client addon contains gui/layouts.");
            return null;
        }
        m_Player = PlayerBase.Cast(GetGame().GetPlayer());
        m_Module = ExpansionMarketModule.GetInstance();
        m_Slots = layoutRoot.FindAnyWidget("SlotsContent");
        m_ChildSlots = layoutRoot.FindAnyWidget("ChildSlotsContent");
        m_GroupPicker = XComboBoxWidget.Cast(layoutRoot.FindAnyWidget("GroupPicker"));
        m_SearchHint = layoutRoot.FindAnyWidget("SearchHint");
        m_Description = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("Description"));
        m_Content = layoutRoot.FindAnyWidget("OffersContent");
        m_Status = TextWidget.Cast(layoutRoot.FindAnyWidget("Status"));
        m_Path = TextWidget.Cast(layoutRoot.FindAnyWidget("Path"));
        m_PreviewTitle = TextWidget.Cast(layoutRoot.FindAnyWidget("PreviewTitle"));
        m_Search = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("Search"));
        m_PreviewMode = XComboBoxWidget.Cast(layoutRoot.FindAnyWidget("PreviewMode"));
        m_PreviewMode.AddItem("Display Player");
        m_PreviewMode.AddItem("Display Parent Item");
        m_PreviewMode.AddItem("Display Selected");
        m_PreviewMode.SetCurrentItem(0);
        m_SavedLoadouts = XComboBoxWidget.Cast(layoutRoot.FindAnyWidget("SavedLoadouts"));
        m_SavedItems = XComboBoxWidget.Cast(layoutRoot.FindAnyWidget("SavedItems"));
        m_LoadoutName = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("LoadoutName"));
        m_ItemPresetName = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("ItemPresetName"));
        m_Presets = BDE_EMU_LoadoutPresets.Load(m_PresetsWritable);
        RefreshPresetPickers();
        m_PlayerPreview = PlayerPreviewWidget.Cast(layoutRoot.FindAnyWidget("PlayerPreview"));
        m_ItemPreview = ItemPreviewWidget.Cast(layoutRoot.FindAnyWidget("ItemPreview"));
        m_Buy = ButtonWidget.Cast(layoutRoot.FindAnyWidget("Buy"));
        m_Buy.Enable(false);
        ExpansionMarketModule.SI_BDELoadoutOffers.Insert(OnOffers);
        ExpansionMarketModule.SI_BDELoadoutResult.Insert(OnResult);
        ExpansionMarketModule.SI_BDELoadoutPrepared.Insert(OnPrepared);
        ExpansionMarketModule.SI_BDELoadoutAmmo.Insert(OnAmmoQuote);
        RebuildDraft();
        BuildSlots();
        BuildPreview();
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(SelectInitialShoulder, 1, false);
        return layoutRoot;
    }

    override void OnShow()
    {
        super.OnShow();
        if (!m_InputsExcluded && GetGame().GetMission())
        {
            GetGame().GetMission().AddActiveInputExcludes({"menu"});
            m_InputsExcluded = true;
        }
        if (GetGame().GetMission() && GetGame().GetMission().GetHud())
        {
            GetGame().GetMission().GetHud().ShowHudUI(false);
            GetGame().GetMission().GetHud().ShowQuickbarUI(false);
            m_HudHidden = true;
        }
        SetFocus(layoutRoot);
        PPEffects.SetBlurMenu(0.35);
    }

    override void OnHide()
    {
        CancelConfirmation();
        super.OnHide();
        Cleanup();
        PPEffects.SetBlurMenu(0);
    }

    void ~BDE_EMU_LoadoutMenu()
    {
        Cleanup();
    }

    override void Cleanup()
    {
        if (m_Closed)
        {
            return;
        }
        m_Closed = true;
        if (GetGame())
        {
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(RefreshCardPreviews);
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(SelectInitialShoulder);
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(DeferredDrawOffers);
        }
        if (m_InputsExcluded && GetGame() && GetGame().GetMission())
        {
            GetGame().GetMission().RemoveActiveInputExcludes({"menu"}, true);
            m_InputsExcluded = false;
        }
        if (m_HudHidden && GetGame() && GetGame().GetMission() && GetGame().GetMission().GetHud())
        {
            GetGame().GetMission().GetHud().ShowHudUI(true);
            GetGame().GetMission().GetHud().ShowQuickbarUI(true);
            m_HudHidden = false;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(NextPresetJob);
        ClearSlots();
        ExpansionMarketModule.SI_BDELoadoutOffers.Remove(OnOffers);
        ExpansionMarketModule.SI_BDELoadoutResult.Remove(OnResult);
        ExpansionMarketModule.SI_BDELoadoutPrepared.Remove(OnPrepared);
        CancelConfirmation();
        ExpansionMarketModule.SI_BDELoadoutAmmo.Remove(OnAmmoQuote);
        ClearCards();
        CloseContextMenu();
        ClearCompatibilityPreview();
        ClearPreview();
    }

    protected BDE_EMU_LoadoutSlot SelectedSlot()
    {
        return m_Selection;
    }

    protected void ClearSlots()
    {
        foreach (BDE_EMU_LoadoutSlot entry: m_Rows)
        {
            ItemPreviewWidget.Cast(entry.Tile.FindAnyWidget("SlotPreview")).SetItem(null);
            entry.Tile.Unlink();
        }
        m_Rows.Clear();
    }

    protected void BuildSlots()
    {
        m_Rebuilding = true;
        ClearSlots();
        TStringArray playerSlots =
        {
            "Shoulder", "Melee", "Headgear", "Mask",
            "Eyewear", "Gloves", "Armband", "Vest",
            "Body", "Back", "Hips", "Legs", "Feet"
        };
        int visibleIndex;
        foreach (int index, string slotName: playerSlots)
        {
            int playerSlot = InventorySlots.GetSlotIdFromString(slotName);
            if (playerSlot != InventorySlots.INVALID)
            {
                AddSlotTile(m_PlayerClone, playerSlot, m_Slots, visibleIndex, slotName);
                visibleIndex++;
            }
        }
        m_Slots.SetSize(visibleIndex * 76, 76);
        if (m_BrowseParent && !BDE_EMU_LoadoutUtil.IsEquippedParent(PlayerBase.Cast(m_PlayerClone), m_BrowseParent))
        {
            m_BrowseParent = null;
        }
        int childCount;
        int visibleChildren;
        if (m_BrowseParent)
        {
            childCount = m_BrowseParent.GetInventory().GetAttachmentSlotsCount();
            for (int childIndex = 0; childIndex < childCount; childIndex++)
            {
                int childSlot = m_BrowseParent.GetInventory().GetAttachmentSlotId(childIndex);
                if (InventorySlots.GetShowForSlotId(childSlot))
                {
                    AddSlotTile(m_BrowseParent, childSlot, m_ChildSlots, visibleChildren, InventorySlots.GetSlotName(childSlot));
                    visibleChildren++;
                }
            }
        }
        m_ChildSlots.SetSize(Math.Max(1, visibleChildren * 76), 76);
        layoutRoot.FindAnyWidget("BackLevel").Enable(m_BrowseParent != null);
        if (m_Selection && !BDE_EMU_LoadoutUtil.IsEquippedParent(PlayerBase.Cast(m_PlayerClone), m_Selection.Parent))
        {
            m_Selection = null;
        }
        m_Rebuilding = false;
        m_EquipmentSignature = EquipmentSignature(m_Player, 0);
    }

    protected void AddSlotTile(EntityAI parent, int slotId, Widget rowWidget, int index, string slotName)
    {
        BDE_EMU_LoadoutSlot entry = new BDE_EMU_LoadoutSlot;
        entry.Parent = parent;
        entry.SlotId = slotId;
        entry.Path = slotName;
        entry.Slots = PathTo(parent);
        entry.Slots.Insert(slotId);
        entry.Tile = GetGame().GetWorkspace().CreateWidgets("BDE_ExpansionMarket_UI/gui/layouts/BDE_EMU_LoadoutSlot.layout", rowWidget);
        entry.Tile.SetPos(index * 76, 0);
        bool supported = BDE_EMU_LoadoutUtil.HasSlot(parent, slotId);
        entry.Tile.Enable(supported);
        EntityAI item;
        if (supported)
        {
            item = parent.GetInventory().FindAttachment(slotId);
        }
        ItemPreviewWidget preview = ItemPreviewWidget.Cast(entry.Tile.FindAnyWidget("SlotPreview"));
        ImageWidget ghost = ImageWidget.Cast(entry.Tile.FindAnyWidget("SlotGhost"));
        preview.Show(item != null);
        ghost.Show(item == null);
        if (item)
        {
            preview.SetItem(item);
            preview.SetView(Math.Max(0, item.GetViewIndex()));
        }
        else
        {
            string icon;
            GetGame().ConfigGetText("CfgSlots Slot_" + slotName + " ghostIcon", icon);
            if (slotName == "Hands")
            {
                icon = "set:dayz_inventory image:hands";
            }
            if (icon == "")
            {
                icon = "set:dayz_inventory image:missing";
            }
            string imagePath = StaticGUIUtils.VerifyIconImageString(StaticGUIUtils.IMAGESETGROUP_INVENTORY, icon);
            if (!ghost.LoadImageFile(0, imagePath))
            {
                Print("[BDE Loadout] Slot icon failed: " + slotName + " / " + imagePath);
                ghost.LoadImageFile(0, "set:dayz_inventory image:missing");
            }
            ghost.SetImage(0);
            ghost.SetColor(ARGB(255, 210, 220, 224));
        }
        if (m_Selection && m_Selection.Parent == parent && m_Selection.SlotId == slotId)
        {
            entry.Tile.SetColor(ARGB(255, 25, 85, 115));
        }
        m_Rows.Insert(entry);
    }

    protected string EquipmentSignature(EntityAI parent, int depth)
    {
        if (!parent || depth > 8)
        {
            return "";
        }
        string result;
        for (int i = 0; i < parent.GetInventory().AttachmentCount(); i++)
        {
            EntityAI item = parent.GetInventory().GetAttachmentFromIndex(i);
            if (!item)
            {
                continue;
            }
            int low;
            int high;
            item.GetNetworkID(low, high);
            InventoryLocation location = new InventoryLocation;
            item.GetInventory().GetCurrentInventoryLocation(location);
            result += low.ToString() + ":" + high.ToString() + ":" + location.GetSlot().ToString() + ";" + EquipmentSignature(item, depth + 1);
        }
        return result;
    }

    void SelectInitialShoulder()
    {
        if (m_Closed || m_Selection)
        {
            return;
        }
        int shoulderId = InventorySlots.GetSlotIdFromString("Shoulder");
        foreach (int index, BDE_EMU_LoadoutSlot entry: m_Rows)
        {
            if (entry && entry.Parent == m_PlayerClone && entry.SlotId == shoulderId)
            {
                SelectSlot(index);
                return;
            }
        }
    }

    protected void QueueDrawOffers()
    {
        if (m_Closed || m_UpdatingGroups || m_ApplyingPreset)
        {
            return;
        }
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(DeferredDrawOffers);
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(DeferredDrawOffers, 1, false);
    }

    void DeferredDrawOffers()
    {
        if (!m_Closed && !m_ApplyingPreset)
        {
            DrawOffers();
        }
    }

    protected void SelectSlot(int row)
    {
        if (m_Rebuilding || m_Busy || m_ApplyingPreset || row < 0 || row >= m_Rows.Count())
        {
            return;
        }
        RememberCategory();
        ref BDE_EMU_LoadoutSlot selected = m_Rows[row];
        if (!selected || !selected.Slots || selected.Slots.Count() == 0)
        {
            return;
        }
        EntityAI selectedParent = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, selected.Slots, selected.Slots.Count() - 1);
        if (!BDE_EMU_LoadoutUtil.HasSlot(selectedParent, selected.SlotId))
        {
            return;
        }
        // Keep selection separate from the two visible rows while drilling down.
        m_Selection = new BDE_EMU_LoadoutSlot;
        m_Selection.Parent = selectedParent;
        m_Selection.SlotId = selected.SlotId;
        m_Selection.Path = selected.Path;
        m_Selection.Slots.Copy(selected.Slots);
        EntityAI child = selectedParent.GetInventory().FindAttachment(selected.SlotId);
        if (selectedParent == m_PlayerClone)
        {
            m_BrowseParent = child;
        }
        else if (child && child.GetInventory().GetAttachmentSlotsCount() > 0)
        {
            m_BrowseParent = child;
        }
        m_Page = 0;
        m_RestoreGroup = m_SlotCategories.Get(SlotCategoryKey());
        m_GroupKeys.Clear();
        BuildSlots();
        RefreshOffers();
        BuildPreview();
    }

    protected void RefreshOffers()
    {
        BDE_EMU_LoadoutSlot slot = SelectedSlot();
        if (!slot || m_Busy)
        {
            return;
        }
        RememberCategory();
        m_RestoreGroup = m_SlotCategories.Get(SlotCategoryKey());
        ClearCards();
        m_SelectedOffer = null;
        TextWidget.Cast(layoutRoot.FindAnyWidget("BuyLabel")).SetText("SELECT AN ITEM");
        m_Buy.Enable(false);
        m_Status.SetText("Loading current trader offers...");
        UpdateTotal();
        m_Request = RequestSlotOffers(slot);
    }

    protected void OnOffers(int request, string message)
    {
        if (m_Closed || request != m_Request)
        {
            return;
        }
        if (m_ApplyingPreset)
        {
            ReceivePresetOffers(message);
            return;
        }
        DrawOffers();
        if (m_PresetSummary != "")
        {
            m_Status.SetText(m_PresetSummary);
            m_PresetSummary = "";
            return;
        }
        BDE_EMU_LoadoutSlot slot = SelectedSlot();
        if (message != "")
        {
            m_Status.SetText(message);
        }
        else if (slot && slot.Parent.GetInventory().FindAttachment(slot.SlotId))
        {
            m_Status.SetText("Select a replacement to review the price after trade-in. Retained equipment goes to your temporary crate.");
        }
        else if (m_Module.BDE_LM_ClientOffers.Count() > 0)
        {
            m_Status.SetText("Select a card to preview. Buy & Equip purchases one item without default attachments.");
        }
    }

    protected void DeleteCardModel(EntityAI model)
    {
        if (!model)
        {
            return;
        }
        Weapon_Base weapon = Weapon_Base.Cast(model);
        if (weapon)
        {
            // Expansion queues assembly when it creates a weapon preview.
            // Cancel it before a category change deletes that preview.
            GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(weapon.AssembleGun);
        }
        GetGame().ObjectDelete(model);
    }

    protected void ClearCards()
    {
        foreach (BDE_EMU_LoadoutCard card: m_Cards)
        {
            ItemPreviewWidget preview = ItemPreviewWidget.Cast(card.Root.FindAnyWidget("CardPreview"));
            preview.SetItem(null);
            if (card.Model)
            {
                DeleteCardModel(card.Model);
            }
            card.Root.Unlink();
        }
        m_Cards.Clear();
        foreach (Widget header: m_Headers)
        {
            header.Unlink();
        }
        m_Headers.Clear();
    }

    protected void DrawOffers()
    {
        if (m_Closed || !m_Selection || !m_Module)
        {
            return;
        }
        CloseContextMenu();
        ClearCards();
        m_SelectedOffer = null;
        TextWidget.Cast(layoutRoot.FindAnyWidget("BuyLabel")).SetText("SELECT AN ITEM");
        m_Buy.Enable(false);
        string search = m_Search.GetText();
        search.ToLower();
        // Select one best listing per item before constructing category pages.
        map<string, ref array<ref BDE_EMU_LoadoutOffer>> groups = new map<string, ref array<ref BDE_EMU_LoadoutOffer>>;
        TStringArray keys = new TStringArray;
        int total;
        PrepareCompatibilityPreview();
        ref array<ref BDE_EMU_LoadoutOffer> uniqueOffers = UniqueOffers();
        foreach (BDE_EMU_LoadoutOffer offer: uniqueOffers)
        {
            if (!offer || offer.Kind == 1)
            {
                continue;
            }
            if (GetGame().IsKindOf(offer.ClassName, "Magazine_Base") && !CanFitOffer(offer.ClassName, m_Selection.SlotId))
            {
                continue;
            }
            string match = BDE_EMU_LoadoutUtil.DisplayName(offer.ClassName) + " " + offer.TraderName + " " + offer.CategoryName;
            match.ToLower();
            if (search != "" && !match.Contains(search))
            {
                continue;
            }
            string key = Widget.TranslateString(offer.TraderName) + " > " + Widget.TranslateString(offer.CategoryName);
            if (!groups.Contains(key))
            {
                groups.Insert(key, new array<ref BDE_EMU_LoadoutOffer>);
                keys.Insert(key);
            }
            groups[key].Insert(offer);
            total++;
        }
        if (keys.Count() == 0)
        {
            keys.Insert("Equipment");
            groups.Insert("Equipment", new array<ref BDE_EMU_LoadoutOffer>);
        }
        keys.Sort();
        TStringArray unlockedKeys = new TStringArray;
        TStringArray lockedKeys = new TStringArray;
        foreach (string orderedKey: keys)
        {
            bool locked = true;
            foreach (BDE_EMU_LoadoutOffer groupOffer: groups[orderedKey])
            {
                if (groupOffer.RequiredRP <= 0)
                {
                    locked = false;
                    break;
                }
            }
            if (locked && groups[orderedKey].Count() > 0)
            {
                lockedKeys.Insert(orderedKey);
            }
            else
            {
                unlockedKeys.Insert(orderedKey);
            }
        }
        keys.Clear();
        if (HasSavedItemCards())
        {
            keys.Insert("Saved Configurations");
            groups.Insert("Saved Configurations", new array<ref BDE_EMU_LoadoutOffer>);
        }
        keys.InsertAll(unlockedKeys);
        keys.InsertAll(lockedKeys);
        if (m_RestoreGroup != "")
        {
            int restoredPage = keys.Find(m_RestoreGroup);
            if (restoredPage >= 0)
            {
                m_Page = restoredPage;
            }
            m_RestoreGroup = "";
        }
        m_GroupKeys = keys;
        int pages = Math.Max(1, keys.Count());
        m_Page = Math.Clamp(m_Page, 0, pages - 1);
        m_UpdatingGroups = true;
        m_GroupPicker.ClearAll();
        foreach (string groupName: keys)
        {
            m_GroupPicker.AddItem(groupName);
        }
        if (keys.Count() > 0)
        {
            m_GroupPicker.SetCurrentItem(m_Page);
        }
        m_UpdatingGroups = false;
        float y;
        if (keys.Count() > 0)
        {
            foreach (BDE_EMU_LoadoutOffer groupedOffer: groups[keys[m_Page]])
            {
                if (IsStagedOffer(groupedOffer))
                {
                    AddCard(groupedOffer, y);
                    y += 120;
                }
            }
            if (keys[m_Page] == "Saved Configurations")
            {
                y = AddSavedItemCards(y);
            }
            else
            {
                y += AddSpecialCards(keys[m_Page], groups[keys[m_Page]], y);
            }
            foreach (BDE_EMU_LoadoutOffer remainingOffer: groups[keys[m_Page]])
            {
                if (!IsStagedOffer(remainingOffer))
                {
                    AddCard(remainingOffer, y);
                    y += 120;
                }
            }
        }
        ClearCompatibilityPreview();
        m_Content.SetSize(1, Math.Max(1, y));
        m_Content.Update();
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(RefreshCardPreviews);
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(RefreshCardPreviews, 1, false);
        ScrollWidget.Cast(layoutRoot.FindAnyWidget("OffersScroll")).VScrollToPos(0);
        if (total == 0)
        {
            m_Status.SetText("No purchasable market listings match this slot and search.");
        }
        BuildPreview();
    }

    protected void AddCard(BDE_EMU_LoadoutOffer offer, float y)
    {
        BDE_EMU_LoadoutCard card = new BDE_EMU_LoadoutCard;
        card.Offer = offer;
        card.Root = GetGame().GetWorkspace().CreateWidgets("BDE_ExpansionMarket_UI/gui/layouts/BDE_EMU_LoadoutCard.layout", m_Content);
        card.Root.SetPos(0, y);
        TextWidget.Cast(card.Root.FindAnyWidget("CardName")).SetText(CardName(offer));
        string stock = "Stock: " + offer.Stock.ToString();
        if (offer.Stock < 0)
        {
            stock = "Stock: unlimited";
        }
        string price = offer.PriceText;
        if (offer.Kind != 0)
        {
            stock = "";
            price = "0";
            if (offer.Kind == 1 && offer.SalePrice > 0)
            {
                price = "-" + offer.SalePrice.ToString();
            }
        }
        if (offer.RequiredRP > 0)
        {
            price = offer.RequiredRP.ToString() + " RP required";
        }
        TextWidget.Cast(card.Root.FindAnyWidget("CardStock")).SetText(stock);
        if ((IsStagedOffer(offer) || offer.Kind == 2) && m_Selection && offer.RequiredRP == 0)
        {
            int ammoPrice;
            if (offer.StartingAmmo)
            {
                ammoPrice = offer.StartingAmmo.Price;
            }
            foreach (BDE_EMU_AmmoChange ammoDisplay: m_AmmoChanges)
            {
                if (ammoDisplay.Slots.Count() == m_Selection.Slots.Count() && BDE_EMU_LoadoutPaths.Prefix(ammoDisplay.Slots, m_Selection.Slots))
                {
                    ammoPrice += ammoDisplay.Price;
                }
            }
            if (ammoPrice > 0)
            {
                int magazinePrice = offer.Price;
                if (offer.StartingAmmo)
                {
                    magazinePrice -= offer.StartingAmmo.Price;
                }
                price = magazinePrice.ToString() + " + " + ammoPrice.ToString() + " for Ammo";
            }
        }
        TextWidget.Cast(card.Root.FindAnyWidget("CardPrice")).SetText(price);
        TextWidget.Cast(card.Root.FindAnyWidget("CardState")).SetText(offer.Reason);
        if (!offer.CanBuy)
        {
            TextWidget.Cast(card.Root.FindAnyWidget("CardPrice")).SetColor(ARGB(255, 230, 165, 100));
        }
        m_Cards.Insert(card);
        EnsureCardPreview(card);
        UpdateCardCompatibility(card);
    }

    // Test against a disposable copy with the destination slot empty. The
    // currently equipped item must not make a valid replacement incompatible.
    protected void PrepareCompatibilityPreview()
    {
        ClearCompatibilityPreview();
        BDE_EMU_LoadoutSlot selected = SelectedSlot();
        if (!selected || !m_PlayerClone || selected.Slots.Count() == 0)
        {
            return;
        }
        m_CompatibilityClone = EntityAI.Cast(GetGame().CreateObjectEx(m_PlayerClone.GetType(), vector.Zero, ECE_LOCAL | ECE_NOLIFETIME));
        if (!m_CompatibilityClone)
        {
            return;
        }
        m_CompatibilityClone.DisableSimulation(true);
        EntityAI excludedItem = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, selected.Slots, selected.Slots.Count());
        CopyCompatibilityAttachments(m_PlayerClone, m_CompatibilityClone, 0, excludedItem);
        m_CompatibilityParent = BDE_EMU_LoadoutPaths.Resolve(m_CompatibilityClone, selected.Slots, selected.Slots.Count() - 1);
        if (m_CompatibilityParent)
        {
            EntityAI generated = m_CompatibilityParent.GetInventory().FindAttachment(selected.SlotId);
            if (generated)
            {
                GetGame().ObjectDelete(generated);
            }
        }
    }

    protected void CopyCompatibilityAttachments(EntityAI source, EntityAI destination, int depth, EntityAI excludedItem)
    {
        if (!source || !destination || depth > 8)
        {
            return;
        }
        ref array<EntityAI> completedItems = new array<EntityAI>;
        int attachmentCount = source.GetInventory().AttachmentCount();
        for (int pass = 0; pass <= attachmentCount; pass++)
        {
            int progress = 0;
            for (int i = 0; i < attachmentCount; i++)
            {
                EntityAI original = source.GetInventory().GetAttachmentFromIndex(i);
                InventoryLocation location = new InventoryLocation;
                if (!original || original == excludedItem || completedItems.Find(original) >= 0 || !original.GetInventory().GetCurrentInventoryLocation(location))
                {
                    continue;
                }
                EntityAI copy = destination.GetInventory().FindAttachment(location.GetSlot());
                if (copy && !CF_String.EqualsIgnoreCase(copy.GetType(), original.GetType()))
                {
                    GetGame().ObjectDelete(copy);
                    copy = null;
                }
                if (!copy)
                {
                    copy = destination.GetInventory().CreateAttachmentEx(original.GetType(), location.GetSlot());
                }
                if (copy)
                {
                    CopyCompatibilityAttachments(original, copy, depth + 1, excludedItem);
                    completedItems.Insert(original);
                    progress++;
                }
            }
            if (progress == 0)
            {
                break;
            }
        }
    }

    protected void ClearCompatibilityPreview()
    {
        m_CompatibilityParent = null;
        if (m_CompatibilityClone)
        {
            m_CompatibilityClone.DisableSimulation(true);
            GetGame().ObjectDelete(m_CompatibilityClone);
            m_CompatibilityClone = null;
        }
    }

    protected bool CanFitOffer(string className, int slotId)
    {
        if (!m_CompatibilityParent || className == "")
        {
            Print("[BDE Loadout fit] Missing compatibility parent for " + className);
            return false;
        }
        // Do not use an entity already assigned to an ItemPreviewWidget for
        // inventory checks. Keep the probe separate from rendered card models.
        EntityAI probe = EntityAI.Cast(GetGame().CreateObjectEx(className, vector.Zero, ECE_LOCAL | ECE_NOLIFETIME));
        if (!probe)
        {
            return false;
        }
        BDE_EMU_LoadoutInventoryAccess.Begin(m_CompatibilityParent);
        bool itemAllows = probe.CanPutAsAttachment(m_CompatibilityParent);
        bool parentAllows = m_CompatibilityParent.CanReceiveAttachment(probe, slotId);
        bool inventoryAllows = m_CompatibilityParent.GetInventory().CanAddAttachmentEx(probe, slotId);
        BDE_EMU_LoadoutInventoryAccess.End();
        bool fits = itemAllows && parentAllows && inventoryAllows;
        if (!fits)
        {
            Print("[BDE Loadout fit] item=" + className + " parent=" + m_CompatibilityParent.GetType() + " slot=" + InventorySlots.GetSlotName(slotId) + " itemAllows=" + itemAllows.ToString() + " parentAllows=" + parentAllows.ToString() + " inventoryAllows=" + inventoryAllows.ToString());
        }
        DeleteCardModel(probe);
        return fits;
    }

    protected void UpdateCardCompatibility(BDE_EMU_LoadoutCard card)
    {
        card.Compatible = true;
        if (card.Offer.Kind == 0)
        {
            BDE_EMU_LoadoutSlot selected = SelectedSlot();
            card.Compatible = false;
            if (selected && m_CompatibilityParent)
            {
                card.Compatible = CanFitOffer(card.Offer.ClassName, selected.SlotId);
            }
        }
        TextWidget state = TextWidget.Cast(card.Root.FindAnyWidget("CardState"));
        string reason = card.Offer.Reason;
        state.SetText(reason);
        if (!card.Compatible)
        {
            state.SetText("Incompatible");
        }
        // CardState retains the layout's existing amber RP warning colour.
        // Keep native widget receiver separate from nested script-field evaluation.
        Widget cardWidget = card.Root;
        bool canSelect = card.Compatible;
        if (!card.Offer.CanBuy)
        {
            canSelect = false;
        }
        cardWidget.Enable(canSelect);
    }

    #ifdef BDE_UnderSlungLaunchers
    protected void EnsureSix12PreviewCylinders(EntityAI previewEntity, int depth = 0)
    {
        if (!previewEntity || depth > 8)
        {
            return;
        }
        BDE_Six12 six12 = BDE_Six12.Cast(previewEntity);
        if (six12)
        {
            // islocal preview entities cannot use the server-only cylinder creation path.
            EntityAI cylinder = six12.FindAttachmentBySlotName("BDE_Six12_Magazine");
            BDE_Six12_Magazine cylinderMagazine = BDE_Six12_Magazine.Cast(cylinder);
            if (!cylinder)
            {
                string cylinderClass = "BDE_Six12_Magazine";
                if (six12.IsKindOf("BDE_Six12_FDE"))
                {
                    cylinderClass = "BDE_Six12_Magazine_FDE";
                }
                cylinderMagazine = BDE_Six12_Magazine.Cast(six12.GetInventory().CreateAttachment(cylinderClass));
                if (cylinderMagazine)
                {
                    // Cosmetic only: never invent purchased ammunition for the preview.
                    cylinderMagazine.LocalSetAmmoCount(0);
                }
            }
            if (cylinderMagazine)
            {
                cylinderMagazine.BDE_Six12_UpdateVisuals();
            }
        }
        for (int index = 0; index < previewEntity.GetInventory().AttachmentCount(); index++)
        {
            EnsureSix12PreviewCylinders(previewEntity.GetInventory().GetAttachmentFromIndex(index), depth + 1);
        }
    }
    #endif

    protected void EnsureCardPreview(BDE_EMU_LoadoutCard card)
    {
        if (card.Offer.ClassName == "")
        {
            card.Root.FindAnyWidget("CardPreview").Show(false);
            ImageWidget ghost = ImageWidget.Cast(card.Root.FindAnyWidget("CardGhost"));
            BDE_EMU_LoadoutSlot selectedEmpty = SelectedSlot();
            if (ghost && selectedEmpty)
            {
                string icon;
                GetGame().ConfigGetText("CfgSlots Slot_" + InventorySlots.GetSlotName(selectedEmpty.SlotId) + " ghostIcon", icon);
                if (icon == "")
                {
                    icon = "set:dayz_inventory image:missing";
                }
                ghost.LoadImageFile(0, StaticGUIUtils.VerifyIconImageString(StaticGUIUtils.IMAGESETGROUP_INVENTORY, icon));
                ghost.SetImage(0);
                ghost.SetColor(ARGB(255, 210, 220, 224));
                ghost.Show(true);
            }
            return;
        }
        if (!card.Model && !card.PreviewAttempted)
        {
            card.PreviewAttempted = true;
            ExpansionMarketMenu.CreatePreviewObject(card.Offer.ClassName, card.Model);
            if (!card.Model)
            {
                Print("[BDE Loadout] Preview creation failed: " + card.Offer.ClassName);
            }
            else if (card.Offer.Kind == 2 && SelectedSlot())
            {
                BDE_EMU_LoadoutSlot selectedSlot = SelectedSlot();
                EntityAI ownedItem = BDE_EMU_LoadoutPaths.Resolve(m_Player, selectedSlot.Slots, selectedSlot.Slots.Count());
                if (ownedItem)
                {
                    CopyAttachments(ownedItem, card.Model, 0);
                }
            }
            #ifdef BDE_UnderSlungLaunchers
            EnsureSix12PreviewCylinders(card.Model);
            #endif
        }
        if (card.Model)
        {
            ItemPreviewWidget preview = ItemPreviewWidget.Cast(card.Root.FindAnyWidget("CardPreview"));
            preview.SetItem(card.Model);
            preview.SetView(Math.Max(0, card.Model.GetViewIndex()));
            preview.Show(true, true);
        }
    }

    void RefreshCardPreviews()
    {
        if (m_Closed)
        {
            return;
        }
        foreach (BDE_EMU_LoadoutCard card: m_Cards)
        {
            EnsureCardPreview(card);
        }
    }

    protected void SelectOffer(BDE_EMU_LoadoutCard card)
    {
        BDE_EMU_LoadoutSlot slot = SelectedSlot();
        if (!slot || m_Busy || m_ApplyingPreset || !card.Offer.CanBuy || !card.Compatible)
        {
            return;
        }
        PrepareCompatibilityPreview();
        UpdateCardCompatibility(card);
        ClearCompatibilityPreview();
        if (!card.Compatible)
        {
            return;
        }
        if (card.Preset)
        {
            BeginItemPreset(card.Preset, slot.Slots);
            return;
        }
        StageOffer(card.Offer, slot.Slots);
        RefreshOffers();
    }

    protected void StageOffer(BDE_EMU_LoadoutOffer offer, array<int> path)
    {
        array<int> browsePath = PathTo(m_BrowseParent);
        CancelConfirmation();
        array<int> selectedPath = new array<int>;
        selectedPath.Copy(path);

        m_AmmoRequest++;
        m_AmmoPending = false;
        for (int ammoIndex = m_AmmoChanges.Count() - 1; ammoIndex >= 0; ammoIndex--)
        {
            if (BDE_EMU_LoadoutPaths.Prefix(selectedPath, m_AmmoChanges[ammoIndex].Slots))
            {
                m_AmmoChanges.Remove(ammoIndex);
            }
        }
        for (int i = m_Changes.Count() - 1; i >= 0; i--)
        {
            if (BDE_EMU_LoadoutPaths.Prefix(selectedPath, m_Changes[i].Slots))
            {
                m_Changes.Remove(i);
            }
        }
        if (offer.Kind != 2)
        {
            BDE_EMU_LoadoutChange change = new BDE_EMU_LoadoutChange;
            change.Slots.Copy(selectedPath);
            change.Offer = offer;
            if (offer.Kind == 0 || offer.ReplacedLow != 0 || offer.ReplacedHigh != 0)
            {
                m_Changes.Insert(change);
            }
        }
        SortChanges();
        m_SelectedOffer = null;
        RebuildDraft();
        m_Selection = new BDE_EMU_LoadoutSlot;
        m_Selection.Slots.Copy(selectedPath);
        m_Selection.SlotId = selectedPath[selectedPath.Count() - 1];
        m_Selection.Parent = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, selectedPath, selectedPath.Count() - 1);
        EntityAI selectedItem = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, selectedPath, selectedPath.Count());
        if (selectedItem && selectedItem.GetInventory().GetAttachmentSlotsCount() > 0)
        {
            m_BrowseParent = selectedItem;
        }
        else if (browsePath.Count() > 0)
        {
            m_BrowseParent = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, browsePath, browsePath.Count());
            if (!m_BrowseParent || m_BrowseParent.GetInventory().GetAttachmentSlotsCount() == 0)
            {
                m_BrowseParent = m_Selection.Parent;
            }
            if (m_BrowseParent == m_PlayerClone)
            {
                m_BrowseParent = null;
            }
        }
        BuildSlots();
        BuildPreview();
        UpdateTotal();
    }

    protected void CopyAttachments(EntityAI source, EntityAI destination, int depth)
    {
        if (!source || !destination || depth > 8)
        {
            return;
        }
        Magazine sourceMagazine = Magazine.Cast(source);
        Magazine destinationMagazine = Magazine.Cast(destination);
        if (sourceMagazine && destinationMagazine)
        {
            destinationMagazine.LocalSetAmmoCount(0);
            for (int cartridgeIndex = 0; cartridgeIndex < sourceMagazine.GetAmmoCount(); cartridgeIndex++)
            {
                float damage;
                string bullet;
                sourceMagazine.GetCartridgeAtIndex(cartridgeIndex, damage, bullet);
                destinationMagazine.LocalStoreCartridge(damage, bullet);
            }
        }
        m_CloneMap.Set(source, destination);
        ref array<EntityAI> completedItems = new array<EntityAI>;
        int attachmentCount = source.GetInventory().AttachmentCount();
        for (int pass = 0; pass <= attachmentCount; pass++)
        {
            int progress = 0;
            for (int i = 0; i < attachmentCount; i++)
            {
                EntityAI original = source.GetInventory().GetAttachmentFromIndex(i);
                InventoryLocation location = new InventoryLocation;
                if (!original || completedItems.Find(original) >= 0 || !original.GetInventory().GetCurrentInventoryLocation(location))
                {
                    continue;
                }
                EntityAI copy = destination.GetInventory().FindAttachment(location.GetSlot());
                if (copy && !CF_String.EqualsIgnoreCase(copy.GetType(), original.GetType()))
                {
                    GetGame().ObjectDelete(copy);
                    copy = null;
                }
                if (!copy)
                {
                    copy = destination.GetInventory().CreateAttachmentEx(original.GetType(), location.GetSlot());
                }
                if (copy)
                {
                    CopyAttachments(original, copy, depth + 1);
                    completedItems.Insert(original);
                    progress++;
                }
            }
            if (progress == 0)
            {
                break;
            }
        }
    }

    protected void ClearPreview()
    {
        if (m_PlayerPreview)
        {
            m_PlayerPreview.SetPlayer(null);
        }
        if (m_ItemPreview)
        {
            m_ItemPreview.SetItem(null);
        }
        if (m_PlayerClone)
        {
            m_PlayerClone.DisableSimulation(true);
            GetGame().ObjectDelete(m_PlayerClone);
            m_PlayerClone = null;
        }
        if (m_ItemClone)
        {
            GetGame().ObjectDelete(m_ItemClone);
            m_ItemClone = null;
        }
        m_CloneMap.Clear();
    }

    protected void BuildPreview()
    {
        // Combo-box selection events can fire while Init is still binding widgets.
        if (!m_PlayerPreview || !m_ItemPreview || !m_PreviewMode || !m_Buy)
        {
            return;
        }
        m_Description.SetText("");
        BDE_EMU_LoadoutSlot selected = SelectedSlot();
        EntityAI item;
        if (selected)
        {
            item = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, selected.Slots, selected.Slots.Count());
        }
        if (item)
        {
            string description;
            GetGame().ConfigGetText(BDE_EMU_LoadoutUtil.ConfigPath(item.GetType()) + " descriptionShort", description);
            m_Description.SetText(Widget.TranslateString(description));
        }
        EntityAI displayItem = item;
        int previewMode = m_PreviewMode.GetCurrentItem();
        if (previewMode == 1 && selected)
        {
            displayItem = selected.Parent;
        }
        bool itemMode = previewMode != 0 && displayItem && displayItem != m_PlayerClone;
        m_PlayerPreview.Show(!itemMode);
        m_ItemPreview.Show(itemMode);
        m_ItemPreview.SetItem(displayItem);
        if (displayItem)
        {
            m_ItemPreview.SetView(Math.Max(0, displayItem.GetViewIndex()));
        }
        m_ItemPreview.SetModelOrientation(Vector(0, m_PreviewYaw, 0));
        m_PlayerPreview.SetPlayer(DayZPlayer.Cast(m_PlayerClone));
        m_PlayerPreview.SetModelPosition(vector.Zero);
        m_PlayerPreview.SetModelOrientation(Vector(0, m_PreviewYaw, 0));
        if (layoutRoot.FindAnyWidget("ItemPresetPopup").IsVisible())
        {
            m_PlayerPreview.Show(false);
            m_ItemPreview.Show(false);
        }
        UpdateTotal();
    }

    protected void OnResult(bool success, string message)
    {
        m_ConfirmToken = 0;
        m_ConfirmRemaining = 0;
        if (m_Closed)
        {
            return;
        }
        m_Busy = false;
        m_SelectedOffer = null;
        if (success)
        {
            m_AmmoChanges.Clear();
            m_AmmoRequest++;
            m_AmmoPending = false;
            m_Changes.Clear();
            m_Selection = null;
            m_BrowseParent = null;
            RebuildDraft();
        }
        TextWidget.Cast(layoutRoot.FindAnyWidget("BuyLabel")).SetText("SELECT AN ITEM");
        m_Buy.Enable(false);
        BuildSlots();
        RefreshOffers();
        BuildPreview();
        m_Status.SetText(message);
    }

    override bool OnItemSelected(Widget w, int x, int y, int row, int column, int oldRow, int oldColumn)
    {
        if (w == m_PreviewMode)
        {
            BuildPreview();
            return true;
        }
        if (w == m_SavedLoadouts || w == m_SavedItems)
        {
            UpdatePresetName(w);
            return true;
        }
        if (m_ApplyingPreset)
        {
            return true;
        }
        if (w == m_GroupPicker && !m_UpdatingGroups)
        {
            int selectedPage = m_GroupPicker.GetCurrentItem();
            if (selectedPage != m_Page)
            {
                m_Page = selectedPage;
                RememberCategory();
                QueueDrawOffers();
            }
            return true;
        }
        return super.OnItemSelected(w, x, y, row, column, oldRow, oldColumn);
    }

    override bool OnChange(Widget w, int x, int y, bool finished)
    {
        if (w == m_PreviewMode)
        {
            BuildPreview();
            return true;
        }
        if (w == m_SavedLoadouts || w == m_SavedItems)
        {
            UpdatePresetName(w);
            return true;
        }
        if (m_ApplyingPreset)
        {
            return true;
        }
        if (w == m_GroupPicker && !m_UpdatingGroups)
        {
            m_Page = m_GroupPicker.GetCurrentItem();
            RememberCategory();
            QueueDrawOffers();
            return true;
        }
        if (w == m_Search)
        {
            m_SearchHint.Show(m_Search.GetText() == "" && GetFocus() != m_Search);
            m_Page = 0;
            QueueDrawOffers();
            return true;
        }
        return super.OnChange(w, x, y, finished);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        Widget clicked = w;
        while (clicked && !ButtonWidget.Cast(clicked))
        {
            clicked = clicked.GetParent();
        }
        if (clicked)
        {
            w = clicked;
        }
        string name = w.GetName();
        if (name == "ContextFill" || name == "ContextRemove")
        {
            CloseContextMenu();
            int mode = 1;
            if (name == "ContextRemove")
            {
                mode = 2;
            }
            RequestAmmoChange(mode);
            return true;
        }
        if (name == "ContextSave")
        {
            CloseContextMenu();
            if (m_DeleteConfiguration)
            {
                int deleteIndex = m_Presets.Items.Find(m_DeleteConfiguration);
                if (deleteIndex >= 0)
                {
                    m_Presets.Items.Remove(deleteIndex);
                    PersistPresets();
                    RefreshPresetPickers();
                }
                m_DeleteConfiguration = null;
                DrawOffers();
            }
            else if (m_ContextSlot)
            {
                OpenItemPreset(m_ContextSlot);
            }
            return true;
        }
        CloseContextMenu();
        foreach (int slotIndex, BDE_EMU_LoadoutSlot slotEntry: m_Rows)
        {
            if (w == slotEntry.Tile)
            {
                if (button == MouseState.RIGHT)
                {
                    OpenSlotContext(slotEntry);
                }
                else
                {
                    SelectSlot(slotIndex);
                }
                return true;
            }
        }
        if (name == "BackLevel")
        {
            if (m_BrowseParent)
            {
                EntityAI upper = m_BrowseParent.GetHierarchyParent();
                if (upper == m_PlayerClone)
                {
                    m_BrowseParent = null;
                }
                else
                {
                    m_BrowseParent = upper;
                }
                BuildSlots();
            }
            return true;
        }
        if (name == "Close")
        {
            Close();
            return true;
        }
        if (HandlePresetButton(name))
        {
            return true;
        }
        if (m_ApplyingPreset)
        {
            return true;
        }
        if (name == "Refresh")
        {
            if (m_Page >= 0 && m_Page < m_GroupKeys.Count())
            {
                m_RestoreGroup = m_GroupKeys[m_Page];
            }
            BuildSlots();
            RefreshOffers();
            BuildPreview();
            return true;
        }
        if (w == m_Buy && !m_Busy && !m_AmmoPending && m_Changes.Count() + m_AmmoChanges.Count() > 0)
        {
            m_Busy = true;
            m_Buy.Enable(false);
            m_Status.SetText("Validating the complete loadout...");
            if (m_ConfirmToken != 0)
            {
                m_Module.BDE_LM_Confirm(m_Station, m_ConfirmToken);
            }
            else
            {
                m_Module.BDE_LM_Checkout(m_Station, m_Changes, m_AmmoChanges);
            }
            return true;
        }
        foreach (BDE_EMU_LoadoutCard card: m_Cards)
        {
            if (w == card.Root)
            {
                if (button == MouseState.RIGHT)
                {
                    OpenCardContext(card);
                }
                else
                {
                    SelectOffer(card);
                }
                return true;
            }
        }
        return super.OnClick(w, x, y, button);
    }

    override bool OnFocus(Widget w, int x, int y)
    {
        if (w == m_Search)
        {
            m_SearchHint.Show(false);
        }
        return super.OnFocus(w, x, y);
    }

    override bool OnFocusLost(Widget w, int x, int y)
    {
        if (w == m_Search)
        {
            m_SearchHint.Show(m_Search.GetText() == "");
        }
        return super.OnFocusLost(w, x, y);
    }

    override bool OnMouseButtonDown(Widget w, int x, int y, int button)
    {
        if (button == MouseState.RIGHT)
        {
            foreach (BDE_EMU_LoadoutSlot entry: m_Rows)
            {
                Widget ancestor = w;
                while (ancestor)
                {
                    if (ancestor == entry.Tile)
                    {
                        OpenSlotContext(entry);
                        return true;
                    }
                    ancestor = ancestor.GetParent();
                }
            }
        }
        if (button == MouseState.RIGHT)
        {
            foreach (BDE_EMU_LoadoutCard card: m_Cards)
            {
                Widget cardAncestor = w;
                while (cardAncestor)
                {
                    if (cardAncestor == card.Root)
                    {
                        OpenCardContext(card);
                        return true;
                    }
                    cardAncestor = cardAncestor.GetParent();
                }
            }
        }
        if ((w == m_PlayerPreview || w == m_ItemPreview) && button == MouseState.LEFT)
        {
            int mouseY;
            GetMousePos(m_DragX, mouseY);
            m_DragYaw = m_PreviewYaw;
            GetGame().GetDragQueue().Call(this, "UpdatePreviewRotation");
            return true;
        }
        return super.OnMouseButtonDown(w, x, y, button);
    }

    void UpdatePreviewRotation(int mouse_x, int mouse_y, bool is_dragging)
    {
        if (m_Closed)
        {
            return;
        }
        m_PreviewYaw = m_DragYaw + mouse_x - m_DragX;
        m_PlayerPreview.SetModelOrientation(Vector(0, m_PreviewYaw, 0));
        m_ItemPreview.SetModelOrientation(Vector(0, m_PreviewYaw, 0));
    }

    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE && m_ContextMenu && m_ContextMenu.IsVisible())
        {
            CloseContextMenu();
            return true;
        }
        if (key == KeyCode.KC_ESCAPE)
        {
            if (layoutRoot.FindAnyWidget("ItemPresetPopup").IsVisible())
            {
                layoutRoot.FindAnyWidget("ItemPresetPopup").Show(false);
                BuildPreview();
                return true;
            }
            Close();
            return true;
        }
        return super.OnKeyDown(w, x, y, key);
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        if (m_ConfirmToken != 0 && !m_Busy)
        {
            m_ConfirmRemaining -= timeslice;
            if (m_ConfirmRemaining <= 0)
            {
                CancelConfirmation();
                m_Status.SetText("Confirmation expired. Press Buy & Equip to check again.");
            }
            UpdateTotal();
        }
        if (m_Closed)
        {
            return;
        }
        if (GetGame().GetInput().LocalPress("UAUIBack", false))
        {
            if (m_ContextMenu && m_ContextMenu.IsVisible())
            {
                CloseContextMenu();
                return;
            }
            if (layoutRoot.FindAnyWidget("ItemPresetPopup").IsVisible())
            {
                layoutRoot.FindAnyWidget("ItemPresetPopup").Show(false);
                BuildPreview();
                return;
            }
            Close();
            return;
        }
        if (m_AmmoPending)
        {
            m_AmmoWait += timeslice;
            if (m_AmmoWait > 15)
            {
                m_AmmoPending = false;
                m_AmmoRequest++;
                m_Status.SetText("Ammunition quote timed out. Right-click the magazine to retry.");
                UpdateTotal();
            }
        }
        if (m_ApplyingPreset)
        {
            m_PresetWait += timeslice;
            if (m_PresetWait > 15)
            {
                FinishPreset("Offer request timed out; remaining items were skipped.");
            }
        }
        m_PreviewRefreshTime += timeslice;
        if (m_PreviewRefreshTime >= 0.15)
        {
            m_PreviewRefreshTime = 0;
            RefreshCardPreviews();
        }
        m_CheckTime += timeslice;
        if (m_CheckTime < 1 || m_Closed)
        {
            return;
        }
        m_CheckTime = 0;
        if (!m_Player || !m_Player.IsAlive() || m_Player.IsUnconscious() || !m_Station || vector.DistanceSq(m_Player.GetPosition(), m_Station.GetPosition()) > 16)
        {
            Close();
            return;
        }
        string signature = EquipmentSignature(m_Player, 0);
        if (signature != m_EquipmentSignature && !m_Busy)
        {
            m_ApplyingPreset = false;
            m_PresetJobs.Clear();
            GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(NextPresetJob);
            m_AmmoChanges.Clear();
            m_AmmoRequest++;
            m_AmmoPending = false;
            m_Changes.Clear();
            m_Selection = null;
            m_BrowseParent = null;
            RebuildDraft();
            BuildSlots();
            RefreshOffers();
            BuildPreview();
        }
    }
    protected array<int> PathTo(EntityAI item)
    {
        array<int> result = new array<int>;
        EntityAI current = item;
        while (current && current != m_PlayerClone)
        {
            InventoryLocation location = new InventoryLocation;
            if (!current.GetInventory().GetCurrentInventoryLocation(location))
            {
                break;
            }
            result.InsertAt(location.GetSlot(), 0);
            current = location.GetParent();
        }
        return result;
    }

    protected bool IsProspective(array<int> path)
    {
        foreach (BDE_EMU_LoadoutChange change: m_Changes)
        {
            if (change.Slots.Count() < path.Count() && BDE_EMU_LoadoutPaths.Prefix(change.Slots, path))
            {
                return true;
            }
        }
        return false;
    }

    protected int RequestSlotOffers(BDE_EMU_LoadoutSlot selected)
    {
        EntityAI originalParent;
        if (!IsProspective(selected.Slots))
        {
            originalParent = BDE_EMU_LoadoutPaths.Resolve(m_Player, selected.Slots, selected.Slots.Count() - 1);
        }
        if (originalParent)
        {
            return m_Module.BDE_LM_RequestOffers(m_Station, originalParent, selected.SlotId);
        }
        return m_Module.BDE_LM_RequestOffers(m_Station, m_Player, selected.SlotId, selected.Parent.GetType());
    }

    protected void SortChanges()
    {
        for (int i = 1; i < m_Changes.Count(); i++)
        {
            int j = i;
            while (j > 0 && m_Changes[j - 1].Slots.Count() > m_Changes[j].Slots.Count())
            {
                BDE_EMU_LoadoutChange temporary = m_Changes[j];
                m_Changes[j] = m_Changes[j - 1];
                m_Changes[j - 1] = temporary;
                j--;
            }
        }
    }

    protected void RebuildDraft()
    {
        ClearSlots();
        ClearPreview();
        m_BrowseParent = null;
        m_PlayerClone = EntityAI.Cast(GetGame().CreateObjectEx(m_Player.GetType(), vector.Zero, ECE_LOCAL | ECE_NOLIFETIME));
        if (!m_PlayerClone)
        {
            m_Status.SetText("Cannot create your loadout preview.");
            return;
        }
        m_PlayerClone.DisableSimulation(true);
        CopyAttachments(m_Player, m_PlayerClone, 0);
        PlayerBase previewPlayer = PlayerBase.Cast(m_PlayerClone);
        PluginLifespan lifespan = PluginLifespan.Cast(GetPlugin(PluginLifespan));
        if (previewPlayer && lifespan)
        {
            lifespan.SynchLifespanVisual(previewPlayer, m_Player.GetLifeSpanState(), false, false, m_Player.GetBloodType());
        }
        for (int removeIndex = m_Changes.Count() - 1; removeIndex >= 0; removeIndex--)
        {
            BDE_EMU_LoadoutChange removal = m_Changes[removeIndex];
            EntityAI replaced = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, removal.Slots, removal.Slots.Count());
            if (replaced)
            {
                GetGame().ObjectDelete(replaced);
            }
        }
        ref array<ref BDE_EMU_LoadoutChange> pending = new array<ref BDE_EMU_LoadoutChange>;
        foreach (BDE_EMU_LoadoutChange pendingChange: m_Changes)
        {
            pending.Insert(pendingChange);
        }
        for (int pass = 0; pass <= m_Changes.Count() && pending.Count() > 0; pass++)
        {
            int completed = 0;
            for (int pendingIndex = 0; pendingIndex < pending.Count();)
            {
                BDE_EMU_LoadoutInventoryAccess.Begin(m_PlayerClone);
                bool applied = ApplyDraftChange(pending[pendingIndex]);
                BDE_EMU_LoadoutInventoryAccess.End();
                if (applied)
                {
                    pending.Remove(pendingIndex);
                    completed++;
                }
                else
                {
                    pendingIndex++;
                }
            }
            if (completed == 0)
            {
                break;
            }
        }
        foreach (BDE_EMU_LoadoutChange failed: pending)
        {
            Print("[BDE Loadout preview] Unresolved item=" + failed.Offer.ClassName + " slot=" + InventorySlots.GetSlotName(failed.Slots[failed.Slots.Count() - 1]));
        }
        ApplyAmmoPreview();
        #ifdef BDE_UnderSlungLaunchers
        EnsureSix12PreviewCylinders(m_PlayerClone);
        #endif
    }

    protected bool ApplyDraftChange(BDE_EMU_LoadoutChange change)
    {
        EntityAI destination = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, change.Slots, change.Slots.Count() - 1);
        if (!destination)
        {
            Print("[BDE Loadout preset v3] Waiting for parent path=" + PresetPathText(change.Slots) + " item=" + change.Offer.ClassName);
            return false;
        }
        int slotId = change.Slots[change.Slots.Count() - 1];
        EntityAI old = destination.GetInventory().FindAttachment(slotId);
        if (old)
        {
            GetGame().ObjectDelete(old);
        }
        if (change.Offer.Kind == 0)
        {
            EntityAI previewItem;
            if (GetGame().ConfigIsExisting(BDE_EMU_LoadoutUtil.ConfigPath(change.Offer.ClassName)))
            {
                previewItem = destination.GetInventory().CreateAttachmentEx(change.Offer.ClassName, slotId);
            }
            MagazineStorage newMagazine = MagazineStorage.Cast(previewItem);
            if (newMagazine)
            {
                newMagazine.LocalSetAmmoCount(0);
                if (change.Offer.StartingAmmo)
                {
                    for (int initialRound = 0; initialRound < change.Offer.StartingAmmo.Count; initialRound++)
                    {
                        newMagazine.LocalStoreCartridge(0, change.Offer.StartingAmmo.Bullet);
                    }
                }
            }
            if (!previewItem)
            {
                bool exists = GetGame().ConfigIsExisting(BDE_EMU_LoadoutUtil.ConfigPath(change.Offer.ClassName));
                Print("[BDE Loadout preset v3] Create failed path=" + PresetPathText(change.Slots) + " item=" + change.Offer.ClassName + " parent=" + destination.GetType() + " configExists=" + exists.ToString());
                return false;
            }
            Print("[BDE Loadout preset v3] Created path=" + PresetPathText(change.Slots) + " item=" + previewItem.GetType());
        }
        return true;
    }

    protected void ApplyAmmoPreview()
    {
        foreach (BDE_EMU_AmmoChange ammunition: m_AmmoChanges)
        {
            Magazine magazine = Magazine.Cast(BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, ammunition.Slots, ammunition.Slots.Count()));
            if (!magazine)
            {
                continue;
            }
            if (ammunition.Low == 0 && ammunition.High == 0)
            {
                magazine.LocalSetAmmoCount(0);
            }
            if (ammunition.Mode == 2)
            {
                magazine.LocalSetAmmoCount(0);
            }
            else
            {
                foreach (BDE_EMU_AmmoLine line: ammunition.Lines)
                {
                    for (int i = 0; i < line.Count; i++)
                    {
                        magazine.LocalStoreCartridge(0, line.Bullet);
                    }
                }
            }
        }
    }

    protected string CardName(BDE_EMU_LoadoutOffer offer)
    {
        if (offer.Kind == 1)
        {
            return "Empty";
        }
        if (offer.Kind == 2)
        {
            return "My " + BDE_EMU_LoadoutUtil.DisplayName(offer.ClassName);
        }
        string name = BDE_EMU_LoadoutUtil.DisplayName(offer.ClassName);
        if (IsStagedOffer(offer))
        {
            name += " | Selected";
        }
        return name;
    }

    protected float AddSpecialCards(string key, array<ref BDE_EMU_LoadoutOffer> offers, float startY = 0)
    {
        BDE_EMU_LoadoutOffer empty;
        foreach (BDE_EMU_LoadoutOffer candidate: m_Module.BDE_LM_ClientOffers)
        {
            if (candidate.Kind != 1)
            {
                continue;
            }
            if (!empty)
            {
                empty = candidate;
            }
            if (offers.Count() > 0 && candidate.TraderLow == offers[0].TraderLow && candidate.TraderHigh == offers[0].TraderHigh)
            {
                empty = candidate;
                break;
            }
        }
        if (!empty)
        {
            return 0;
        }
        AddCard(empty, startY);
        BDE_EMU_LoadoutSlot selected = SelectedSlot();
        if (selected && !IsProspective(selected.Slots))
        {
            EntityAI ownedItem = BDE_EMU_LoadoutPaths.Resolve(m_Player, selected.Slots, selected.Slots.Count());
            if (ownedItem)
            {
                BDE_EMU_LoadoutOffer mine = new BDE_EMU_LoadoutOffer;
                mine.Kind = 2;
                mine.ClassName = ownedItem.GetType();
                mine.CanBuy = true;
                AddCard(mine, startY + 120);
                return 240;
            }
        }
        return 120;
    }

    protected void UpdateTotal()
    {
        int total;
        foreach (BDE_EMU_LoadoutChange change: m_Changes)
        {
            total += change.Offer.Price - change.Offer.SalePrice;
        }
        foreach (BDE_EMU_AmmoChange ammunition: m_AmmoChanges)
        {
            total += ammunition.Price;
        }
        bool pendingQuote;
        foreach (BDE_EMU_LoadoutChange staged: m_Changes)
        {
            if (staged.Offer.ItemID == -1)
            {
                pendingQuote = true;
            }
        }
        string label = "BUY & EQUIP / $" + total.ToString();
        if (total < 0)
        {
            label = "BUY & EQUIP / RECEIVE $" + (-total).ToString();
        }
        if (pendingQuote)
        {
            label = "BUY & EQUIP / CHECK PRICE";
        }
        if (m_ConfirmToken != 0)
        {
            label = "Confirm Purchase - " + Math.Ceil(m_ConfirmRemaining).ToString();
        }
        TextWidget.Cast(layoutRoot.FindAnyWidget("BuyLabel")).SetText(label);
        m_Buy.Enable(!m_Busy && !m_ApplyingPreset && !m_AmmoPending && m_Changes.Count() + m_AmmoChanges.Count() > 0);
    }
    protected bool BetterOffer(BDE_EMU_LoadoutOffer candidate, BDE_EMU_LoadoutOffer current)
    {
        if (!current)
        {
            return true;
        }
        if (candidate.CanBuy != current.CanBuy)
        {
            return candidate.CanBuy;
        }
        if (candidate.RequiredRP != current.RequiredRP)
        {
            return candidate.RequiredRP < current.RequiredRP;
        }
        if (candidate.TraderRequiredRP != current.TraderRequiredRP)
        {
            return candidate.TraderRequiredRP < current.TraderRequiredRP;
        }
        if (candidate.Price != current.Price)
        {
            return candidate.Price < current.Price;
        }
        // Stable tie-break independent of the trader entity registration order.
        // Reverse name order puts Clothing before Armour when both offers match.
        string candidateKey = candidate.TraderName + " > " + candidate.CategoryName;
        string currentKey = current.TraderName + " > " + current.CategoryName;
        TStringArray ordered = {candidateKey, currentKey};
        ordered.Sort();
        return candidateKey != currentKey && ordered[1] == candidateKey;
    }

    protected array<ref BDE_EMU_LoadoutOffer> UniqueOffers()
    {
        ref array<ref BDE_EMU_LoadoutOffer> result = new array<ref BDE_EMU_LoadoutOffer>;
        map<string, ref BDE_EMU_LoadoutOffer> best = new map<string, ref BDE_EMU_LoadoutOffer>;
        foreach (BDE_EMU_LoadoutOffer candidate: m_Module.BDE_LM_ClientOffers)
        {
            if (!candidate || candidate.Kind != 0)
            {
                continue;
            }
            string key = candidate.ClassName;
            key.ToLower();
            if (BetterOffer(candidate, best.Get(key)))
            {
                best.Set(key, candidate);
            }
        }
        foreach (string classKey, BDE_EMU_LoadoutOffer offer: best)
        {
            result.Insert(offer);
        }
        return result;
    }

    protected BDE_EMU_LoadoutOffer FindPresetOffer(string type)
    {
        type.ToLower();
        BDE_EMU_LoadoutOffer best;
        foreach (BDE_EMU_LoadoutOffer candidate: m_Module.BDE_LM_ClientOffers)
        {
            string candidateType = candidate.ClassName;
            candidateType.ToLower();
            if (candidate.Kind == 0 && candidate.CanBuy && candidateType == type && PresetStockAvailable(candidate) && BetterOffer(candidate, best))
            {
                best = candidate;
            }
        }
        return best;
    }

    protected bool PresetStockAvailable(BDE_EMU_LoadoutOffer offer)
    {
        if (offer.Stock < 0)
        {
            return true;
        }
        int needed = 1;
        foreach (BDE_EMU_LoadoutChange change: m_Changes)
        {
            if (change.Offer.Kind != 0 || change.Offer.ItemID != offer.ItemID)
            {
                continue;
            }
            if (m_Selection && change.Slots.Count() == m_Selection.Slots.Count() && BDE_EMU_LoadoutPaths.Prefix(change.Slots, m_Selection.Slots))
            {
                continue;
            }
            needed++;
        }
        return needed <= offer.Stock;
    }

    protected bool FitsPresetOffer(BDE_EMU_LoadoutOffer offer)
    {
        if (!offer || !offer.CanBuy)
        {
            return false;
        }
        PrepareCompatibilityPreview();
        BDE_EMU_LoadoutSlot selected = SelectedSlot();
        bool fits;
        if (selected)
        {
            fits = CanFitOffer(offer.ClassName, selected.SlotId);
        }
        ClearCompatibilityPreview();
        return fits;
    }

    protected void PopulateSavedPreview(EntityAI parent, BDE_EMU_SavedNode node, int depth)
    {
        if (!parent || !node || !node.Children || depth > 8)
        {
            return;
        }
        foreach (BDE_EMU_SavedNode child: node.Children)
        {
            if (!child || child.ClassName == "" || !GetGame().ConfigIsExisting(BDE_EMU_LoadoutUtil.ConfigPath(child.ClassName)))
            {
                continue;
            }
            int childSlot = InventorySlots.GetSlotIdFromString(child.SlotName);
            EntityAI attached = parent.GetInventory().FindAttachment(childSlot);
            if (!attached)
            {
                attached = parent.GetInventory().CreateAttachmentEx(child.ClassName, childSlot);
            }
            PopulateSavedPreview(attached, child, depth + 1);
        }
    }

    protected bool HasSavedItemCards()
    {
        foreach (BDE_EMU_SavedLoadout preset: m_Presets.Items)
        {
            if (preset && preset.Nodes && preset.Nodes.Count() == 1 && preset.Nodes[0] && FindPresetOffer(preset.Nodes[0].ClassName))
            {
                return true;
            }
        }
        return false;
    }

    protected float AddSavedItemCards(float y)
    {
        foreach (BDE_EMU_SavedLoadout preset: m_Presets.Items)
        {
            if (!preset || !preset.Nodes || preset.Nodes.Count() != 1 || !preset.Nodes[0])
            {
                continue;
            }
            BDE_EMU_LoadoutOffer offer = FindPresetOffer(preset.Nodes[0].ClassName);
            if (!offer)
            {
                continue;
            }
            AddCard(offer, y);
            BDE_EMU_LoadoutCard card = m_Cards[m_Cards.Count() - 1];
            if (!card.Compatible)
            {
                ItemPreviewWidget.Cast(card.Root.FindAnyWidget("CardPreview")).SetItem(null);
                if (card.Model)
                {
                    DeleteCardModel(card.Model);
                }
                card.Root.Unlink();
                m_Cards.Remove(m_Cards.Count() - 1);
                continue;
            }
            card.Preset = preset;
            ItemPreviewWidget.Cast(card.Root.FindAnyWidget("CardPreview")).SetItem(null);
            PopulateSavedPreview(card.Model, preset.Nodes[0], 0);
            #ifdef BDE_UnderSlungLaunchers
            EnsureSix12PreviewCylinders(card.Model);
            #endif
            ItemPreviewWidget.Cast(card.Root.FindAnyWidget("CardPreview")).SetItem(card.Model);
            TextWidget.Cast(card.Root.FindAnyWidget("CardName")).SetText("Saved: " + preset.Name);
            TextWidget.Cast(card.Root.FindAnyWidget("CardPrice")).SetText("Apply configuration");
            TextWidget.Cast(card.Root.FindAnyWidget("CardStock")).SetText("");
            y += 120;
        }
        return y;
    }

    override bool OnMouseWheel(Widget w, int x, int y, int wheel)
    {
        if (w == m_PlayerPreview || w == m_ItemPreview)
        {
            m_PreviewScale = Math.Clamp(m_PreviewScale + wheel * 0.15, 0.5, 3.0);
            float offset = (1 - m_PreviewScale) * 0.5;
            m_PlayerPreview.SetSize(m_PreviewScale, m_PreviewScale);
            m_PlayerPreview.SetPos(offset, offset);
            m_ItemPreview.SetSize(m_PreviewScale, m_PreviewScale);
            m_ItemPreview.SetPos(offset, offset);
            return true;
        }
        return super.OnMouseWheel(w, x, y, wheel);
    }

    override bool OnMouseEnter(Widget w, int x, int y)
    {
        foreach (BDE_EMU_LoadoutSlot entry: m_Rows)
        {
            Widget ancestor = w;
            while (ancestor)
            {
                if (ancestor == entry.Tile)
                {
                    string label;
                    GetGame().ConfigGetText("CfgSlots Slot_" + InventorySlots.GetSlotName(entry.SlotId) + " displayName", label);
                    if (label == "")
                    {
                        label = InventorySlots.GetSlotName(entry.SlotId);
                    }
                    label = Widget.TranslateString(label);
                    EntityAI hoveredItem = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, entry.Slots, entry.Slots.Count());
                    if (CanConfigureItem(hoveredItem))
                    {
                        label += " | Right Click to Save Item Configuration";
                    }
                    TextWidget.Cast(layoutRoot.FindAnyWidget("SlotTooltip")).SetText(label);
                    return true;
                }
                ancestor = ancestor.GetParent();
            }
        }
        foreach (BDE_EMU_LoadoutCard card: m_Cards)
        {
            Widget cardAncestor = w;
            while (cardAncestor)
            {
                if (cardAncestor == card.Root)
                {
                    string hint;
                    if (card.Preset)
                    {
                        hint = "Right Click to Delete Configuration";
                    }
                    else if (card.Model && MagazineStorage.Cast(card.Model))
                    {
                        hint = "Right Click to Fill/Empty Magazine";
                    }
                    TextWidget.Cast(layoutRoot.FindAnyWidget("SlotTooltip")).SetText(hint);
                    return true;
                }
                cardAncestor = cardAncestor.GetParent();
            }
        }
        return super.OnMouseEnter(w, x, y);
    }

    override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
    {
        TextWidget.Cast(layoutRoot.FindAnyWidget("SlotTooltip")).SetText("");
        return super.OnMouseLeave(w, enterW, x, y);
    }

    protected void RefreshPresetPickers()
    {
        m_SavedLoadouts.ClearAll();
        foreach (BDE_EMU_SavedLoadout loadout: m_Presets.Loadouts)
        {
            if (loadout)
            {
                m_SavedLoadouts.AddItem(loadout.Name);
            }
        }
        if (m_Presets.Loadouts.Count() > 0)
        {
            int selectedIndex;
            for (int i = 0; i < m_Presets.Loadouts.Count(); i++)
            {
                if (m_Presets.Loadouts[i].Name == m_LoadoutName.GetText())
                {
                    selectedIndex = i;
                    break;
                }
            }
            m_SavedLoadouts.SetCurrentItem(selectedIndex);
        }
        else
        {
            m_SavedLoadouts.AddItem("Saved loadouts");
            m_SavedLoadouts.SetCurrentItem(0);
        }
    }

    protected void UpdatePresetName(Widget picker)
    {
        int index;
        if (picker == m_SavedLoadouts)
        {
            index = m_SavedLoadouts.GetCurrentItem();
            if (index >= 0 && index < m_Presets.Loadouts.Count() && m_Presets.Loadouts[index])
            {
                m_LoadoutName.SetText(m_Presets.Loadouts[index].Name);
            }
        }
        else
        {
            index = m_SavedItems.GetCurrentItem();
            if (index >= 0 && index < m_ContextPresets.Count())
            {
                m_ItemPresetName.SetText(m_ContextPresets[index].Name);
            }
        }
    }

    protected void PersistPresets()
    {
        string error;
        if (!m_PresetsWritable || !m_Presets.Save(error))
        {
            m_Status.SetText("Could not save client presets. Existing file was not reset. " + error);
            return;
        }
        RefreshPresetPickers();
        m_Status.SetText("Loadout Saved");
        EffectSound savedSound = SEffectManager.PlaySound("pickUpBackPack_Metal_SoundSet", m_Player.GetPosition());
        if (savedSound)
        {
            savedSound.SetAutodestroy(true);
        }
    }

    protected void StorePreset(array<ref BDE_EMU_SavedLoadout> collection, BDE_EMU_SavedLoadout preset)
    {
        for (int i = 0; i < collection.Count(); i++)
        {
            if (collection[i] && collection[i].Name == preset.Name)
            {
                collection[i] = preset;
                return;
            }
        }
        collection.Insert(preset);
    }

    protected string SlotCategoryKey()
    {
        if (!m_Selection)
        {
            return "";
        }
        string key;
        foreach (int slotId: m_Selection.Slots)
        {
            key += slotId.ToString() + "/";
        }
        return key;
    }

    protected void RememberCategory()
    {
        string key = SlotCategoryKey();
        if (key != "" && m_Page >= 0 && m_Page < m_GroupKeys.Count())
        {
            m_SlotCategories.Set(key, m_GroupKeys[m_Page]);
        }
    }

    protected bool IsStagedOffer(BDE_EMU_LoadoutOffer offer)
    {
        if (!m_Selection || !offer)
        {
            return false;
        }
        foreach (BDE_EMU_LoadoutChange change: m_Changes)
        {
            if (change.Slots.Count() == m_Selection.Slots.Count() && BDE_EMU_LoadoutPaths.Prefix(change.Slots, m_Selection.Slots))
            {
                return change.Offer.Kind == offer.Kind && change.Offer.ClassName == offer.ClassName;
            }
        }
        return false;
    }

    protected void CloseContextMenu()
    {
        if (m_ContextMenu)
        {
            m_ContextMenu.Show(false);
        }
    }

    protected bool CanConfigureItem(EntityAI item)
    {
        if (!item)
        {
            return false;
        }
        for (int index = 0; index < item.GetInventory().GetAttachmentSlotsCount(); index++)
        {
            if (item.GetInventory().GetAttachmentSlotId(index) != InventorySlots.INVALID)
            {
                return true;
            }
        }
        return false;
    }

    protected void OpenCardContext(BDE_EMU_LoadoutCard card)
    {
        if (m_Busy || m_ApplyingPreset)
        {
            return;
        }
        m_DeleteConfiguration = card.Preset;
        if (m_DeleteConfiguration)
        {
            ShowContextMenu(false, false, true);
            TextWidget.Cast(m_ContextMenu.FindAnyWidget("SaveConfigurationLabel")).SetText("Delete Configuration");
            return;
        }
        OpenMagazineContext(card);
    }

    protected void OpenSlotContext(BDE_EMU_LoadoutSlot entry)
    {
        if (m_Busy || m_ApplyingPreset)
        {
            return;
        }
        CloseContextMenu();
        m_ContextSlot = new BDE_EMU_LoadoutSlot;
        m_ContextSlot.Slots.Copy(entry.Slots);
        m_ContextSlot.SlotId = entry.SlotId;
        m_ContextSlot.Parent = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, entry.Slots, entry.Slots.Count() - 1);
        if (!m_ContextSlot.Parent || !CanConfigureItem(m_ContextSlot.Parent.GetInventory().FindAttachment(entry.SlotId)))
        {
            return;
        }
        m_DeleteConfiguration = null;
        ShowContextMenu(false, false, true);
        TextWidget.Cast(m_ContextMenu.FindAnyWidget("SaveConfigurationLabel")).SetText("Save Configuration");
    }

    protected void ShowContextMenu(bool fill, bool remove, bool save)
    {
        if (!m_ContextMenu)
        {
            m_ContextMenu = GetGame().GetWorkspace().CreateWidgets("BDE_ExpansionMarket_UI/gui/layouts/BDE_EMU_LoadoutContext.layout", layoutRoot);
        }
        m_ContextMenu.Show(true);
        int rowCount;
        if (save)
        {
            m_ContextMenu.FindAnyWidget("ContextSave").SetPos(0, rowCount * 38);
            rowCount++;
        }
        if (fill)
        {
            m_ContextMenu.FindAnyWidget("ContextFill").SetPos(0, rowCount * 38);
            rowCount++;
        }
        if (remove)
        {
            m_ContextMenu.FindAnyWidget("ContextRemove").SetPos(0, rowCount * 38);
            rowCount++;
        }
        m_ContextMenu.SetSize(230, Math.Max(38, rowCount * 38));
        m_ContextMenu.FindAnyWidget("ContextSave").Show(save);
        m_ContextMenu.FindAnyWidget("ContextFill").Show(fill);
        m_ContextMenu.FindAnyWidget("ContextRemove").Show(remove);
        int mouseX;
        int mouseY;
        GetMousePos(mouseX, mouseY);
        float rootX;
        float rootY;
        float rootW;
        float rootH;
        layoutRoot.GetScreenPos(rootX, rootY);
        layoutRoot.GetScreenSize(rootW, rootH);
        m_ContextMenu.SetPos(Math.Clamp(mouseX - rootX, 0, rootW - 230), Math.Clamp(mouseY - rootY, 0, rootH - 120));
    }

    protected void OpenMagazineContext(BDE_EMU_LoadoutCard card)
    {
        if (!card || !card.Compatible || !card.Offer.CanBuy || !m_Selection || m_Busy || m_ApplyingPreset || m_AmmoPending)
        {
            return;
        }
        MagazineStorage magazine = MagazineStorage.Cast(card.Model);
        if (!magazine)
        {
            return;
        }
        ref BDE_EMU_LoadoutOffer offer = card.Offer;
        array<int> path = new array<int>;
        path.Copy(m_Selection.Slots);
        bool needsSelection = offer.Kind == 0 && !IsStagedOffer(offer);
        if (offer.Kind == 2)
        {
            foreach (BDE_EMU_LoadoutChange existingChange: m_Changes)
            {
                if (BDE_EMU_LoadoutPaths.Prefix(existingChange.Slots, path))
                {
                    needsSelection = true;
                    break;
                }
            }
        }
        if (needsSelection)
        {
            StageOffer(offer, path);
        }
        m_AmmoContextPath.Copy(path);
        magazine = MagazineStorage.Cast(BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, path, path.Count()));
        if (!magazine)
        {
            return;
        }
        CloseContextMenu();
        ShowContextMenu(magazine.GetAmmoCount() < magazine.GetAmmoMax(), magazine.GetAmmoCount() > 0, false);
    }

    protected void RequestAmmoChange(int mode)
    {
        CancelConfirmation();
        Magazine magazine = Magazine.Cast(BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, m_AmmoContextPath, m_AmmoContextPath.Count()));
        if (!magazine || m_Busy || m_AmmoPending)
        {
            return;
        }
        ref BDE_EMU_AmmoChange request = new BDE_EMU_AmmoChange;
        request.Slots.Copy(m_AmmoContextPath);
        request.MagazineClassName = magazine.GetType();
        request.Mode = mode;
        bool purchased;
        foreach (BDE_EMU_LoadoutChange change: m_Changes)
        {
            if (BDE_EMU_LoadoutPaths.Prefix(change.Slots, request.Slots))
            {
                purchased = true;
                if (change.Slots.Count() == request.Slots.Count() && change.Offer.Kind == 0)
                {
                    request.PurchaseItemID = change.Offer.ItemID;
                    request.PurchaseTraderLow = change.Offer.TraderLow;
                    request.PurchaseTraderHigh = change.Offer.TraderHigh;
                    break;
                }
            }
        }
        if (!purchased)
        {
            EntityAI original = BDE_EMU_LoadoutPaths.Resolve(m_Player, request.Slots, request.Slots.Count());
            if (original)
            {
                original.GetNetworkID(request.Low, request.High);
            }
        }
        for (int i = m_AmmoChanges.Count() - 1; i >= 0; i--)
        {
            if (m_AmmoChanges[i].Slots.Count() == request.Slots.Count() && BDE_EMU_LoadoutPaths.Prefix(m_AmmoChanges[i].Slots, request.Slots))
            {
                m_AmmoChanges.Remove(i);
            }
        }
        Magazine originalMagazine = Magazine.Cast(BDE_EMU_LoadoutPaths.Resolve(m_Player, request.Slots, request.Slots.Count()));
        if (mode == 2 && !purchased && originalMagazine && originalMagazine.GetAmmoCount() == 0)
        {
            RefreshAmmoDraft();
            m_Status.SetText("Unpurchased ammunition removed from the total.");
            return;
        }
        m_AmmoPending = true;
        m_AmmoWait = 0;
        m_AmmoRequest++;
        m_Module.BDE_LM_RequestAmmo(m_Station, m_AmmoRequest, request);
        m_Status.SetText("Checking current ammunition prices and stock...");
        UpdateTotal();
    }

    protected void OnAmmoQuote(int request, BDE_EMU_AmmoChange quote, string error)
    {
        if (m_Closed || request != m_AmmoRequest)
        {
            return;
        }
        m_AmmoPending = false;
        if (quote)
        {
            m_AmmoChanges.Insert(quote);
        }
        RefreshAmmoDraft();
        if (error != "")
        {
            m_Status.SetText(error);
        }
        else
        {
            m_Status.SetText("Ammunition staged. Buy & Equip applies the complete loadout.");
        }
    }

    protected void RefreshAmmoDraft()
    {
        array<int> path = new array<int>;
        if (m_Selection)
        {
            path.Copy(m_Selection.Slots);
        }
        array<int> browse = PathTo(m_BrowseParent);
        RebuildDraft();
        if (path.Count() > 0)
        {
            m_Selection = new BDE_EMU_LoadoutSlot;
            m_Selection.Slots.Copy(path);
            m_Selection.SlotId = path[path.Count() - 1];
            m_Selection.Parent = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, path, path.Count() - 1);
        }
        if (browse.Count() > 0)
        {
            m_BrowseParent = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, browse, browse.Count());
        }
        BuildSlots();
        BuildPreview();
        RefreshOffers();
    }

    protected void OpenItemPreset(BDE_EMU_LoadoutSlot slotEntry)
    {
        if (m_Busy || m_ApplyingPreset)
        {
            return;
        }
        EntityAI item = slotEntry.Parent.GetInventory().FindAttachment(slotEntry.SlotId);
        if (!CanConfigureItem(item))
        {
            m_Status.SetText("This item has no configurable slots.");
            return;
        }
        m_ContextPath.Copy(slotEntry.Slots);
        m_ContextPresets.Clear();
        m_SavedItems.ClearAll();
        foreach (BDE_EMU_SavedLoadout preset: m_Presets.Items)
        {
            if (preset && preset.Nodes && preset.Nodes.Count() == 1 && preset.Nodes[0] && preset.Nodes[0].ClassName == item.GetType())
            {
                m_ContextPresets.Insert(preset);
                m_SavedItems.AddItem(preset.Name);
            }
        }
        m_ItemPresetName.SetText(BDE_EMU_LoadoutUtil.DisplayName(item.GetType()));
        if (m_ContextPresets.Count() > 0)
        {
            m_SavedItems.SetCurrentItem(0);
        }
        layoutRoot.FindAnyWidget("ItemPresetPopup").Show(true);
        m_PlayerPreview.Show(false);
        m_ItemPreview.Show(false);
    }

    protected bool HandlePresetButton(string name)
    {
        if (name == "CloseItemPreset")
        {
            layoutRoot.FindAnyWidget("ItemPresetPopup").Show(false);
            BuildPreview();
            return true;
        }
        if (m_Busy || m_ApplyingPreset)
        {
            return false;
        }
        int selectedIndex;
        BDE_EMU_SavedLoadout preset;
        if (name == "ApplyLoadout")
        {
            selectedIndex = m_SavedLoadouts.GetCurrentItem();
            if (selectedIndex >= 0 && selectedIndex < m_Presets.Loadouts.Count())
            {
                BeginPreset(m_Presets.Loadouts[selectedIndex]);
            }
            return true;
        }
        if (name != "SaveLoadout" && name != "DeleteLoadout" && name != "SaveItemPreset" && name != "DeleteItemPreset")
        {
            return false;
        }
        if (!m_PresetsWritable)
        {
            m_Status.SetText("Preset file could not be read. Repair it before saving; it will not be overwritten.");
            return true;
        }
        if (name == "SaveLoadout")
        {
            string loadoutName = m_LoadoutName.GetText();
            loadoutName.TrimInPlace();
            if (loadoutName == "")
            {
                m_Status.SetText("Enter a name for this loadout.");
                return true;
            }
            preset = new BDE_EMU_SavedLoadout;
            preset.Name = loadoutName;
            foreach (BDE_EMU_LoadoutSlot entry: m_Rows)
            {
                if (entry.Parent == m_PlayerClone && BDE_EMU_LoadoutUtil.HasSlot(entry.Parent, entry.SlotId))
                {
                    preset.Nodes.Insert(BDE_EMU_LoadoutPresets.Capture(entry.Parent, entry.SlotId));
                }
            }
            StorePreset(m_Presets.Loadouts, preset);
        }
        else if (name == "DeleteLoadout")
        {
            selectedIndex = m_SavedLoadouts.GetCurrentItem();
            if (selectedIndex >= 0 && selectedIndex < m_Presets.Loadouts.Count())
            {
                m_Presets.Loadouts.Remove(selectedIndex);
            }
        }
        else if (name == "SaveItemPreset")
        {
            string itemName = m_ItemPresetName.GetText();
            itemName.TrimInPlace();
            if (itemName == "" || m_ContextPath.Count() == 0)
            {
                m_Status.SetText("Enter a name for this item configuration.");
                return true;
            }
            EntityAI parent = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, m_ContextPath, m_ContextPath.Count() - 1);
            if (!parent || !CanConfigureItem(parent.GetInventory().FindAttachment(m_ContextPath[m_ContextPath.Count() - 1])))
            {
                return true;
            }
            preset = new BDE_EMU_SavedLoadout;
            preset.Name = itemName;
            preset.Nodes.Insert(BDE_EMU_LoadoutPresets.Capture(parent, m_ContextPath[m_ContextPath.Count() - 1]));
            StorePreset(m_Presets.Items, preset);
            layoutRoot.FindAnyWidget("ItemPresetPopup").Show(false);
            BuildPreview();
        }
        else if (name == "DeleteItemPreset")
        {
            selectedIndex = m_SavedItems.GetCurrentItem();
            if (selectedIndex >= 0 && selectedIndex < m_ContextPresets.Count())
            {
                int storedIndex = m_Presets.Items.Find(m_ContextPresets[selectedIndex]);
                if (storedIndex >= 0)
                {
                    m_Presets.Items.Remove(storedIndex);
                }
            }
            layoutRoot.FindAnyWidget("ItemPresetPopup").Show(false);
            BuildPreview();
        }
        PersistPresets();
        DrawOffers();
        return true;
    }

    protected void BeginPreset(BDE_EMU_SavedLoadout preset)
    {
        if (!preset || !preset.Nodes || m_Busy || m_ApplyingPreset)
        {
            return;
        }
        m_PresetJobs.Clear();
        foreach (BDE_EMU_SavedNode node: preset.Nodes)
        {
            if (node)
            {
                BDE_EMU_PresetJob job = new BDE_EMU_PresetJob;
                job.Node = node;
                m_PresetJobs.Insert(job);
            }
        }
        StartPresetQueue();
    }

    protected void BeginItemPreset(BDE_EMU_SavedLoadout preset, array<int> selectedPath)
    {
        if (!preset || !preset.Nodes || preset.Nodes.Count() != 1 || selectedPath.Count() == 0)
        {
            return;
        }
        m_PresetJobs.Clear();
        BDE_EMU_PresetJob job = new BDE_EMU_PresetJob;
        job.Node = new BDE_EMU_SavedNode;
        job.Node.ClassName = preset.Nodes[0].ClassName;
        job.Node.SlotName = InventorySlots.GetSlotName(selectedPath[selectedPath.Count() - 1]);
        job.Node.Children = preset.Nodes[0].Children;
        job.ParentPath.Copy(selectedPath);
        job.ParentPath.Remove(job.ParentPath.Count() - 1);
        m_PresetJobs.Insert(job);
        StartPresetQueue();
    }

    protected void StartPresetQueue()
    {
        CancelConfirmation();
        m_AmmoRequest++;
        m_AmmoPending = false;
        m_Request = -1;
        m_ApplyingPreset = true;
        m_SelectedOffer = null;
        Print("[BDE Loadout preset v3] Begin jobs=" + m_PresetJobs.Count().ToString());
        foreach (BDE_EMU_PresetJob job: m_PresetJobs)
        {
            if (!job || !job.Node)
            {
                continue;
            }
            array<int> path = new array<int>;
            path.Copy(job.ParentPath);
            path.Insert(InventorySlots.GetSlotIdFromString(job.Node.SlotName));
            for (int i = m_Changes.Count() - 1; i >= 0; i--)
            {
                if (BDE_EMU_LoadoutPaths.Prefix(path, m_Changes[i].Slots))
                {
                    m_Changes.Remove(i);
                }
            }
            for (int ammoIndex = m_AmmoChanges.Count() - 1; ammoIndex >= 0; ammoIndex--)
            {
                if (BDE_EMU_LoadoutPaths.Prefix(path, m_AmmoChanges[ammoIndex].Slots))
                {
                    m_AmmoChanges.Remove(ammoIndex);
                }
            }
            StageSavedNode(job.Node, job.ParentPath, false, 0);
        }
        m_ApplyingPreset = false;
        Print("[BDE Loadout preset v3] Planned changes=" + m_Changes.Count().ToString());
        SortChanges();
        m_Selection = null;
        m_BrowseParent = null;
        ClearCards();
        RebuildDraft();
        BuildSlots();
        BuildPreview();
        UpdateTotal();
        layoutRoot.FindAnyWidget("ItemPresetPopup").Show(false);
        int mismatches = 0;
        foreach (BDE_EMU_PresetJob auditJob: m_PresetJobs)
        {
            mismatches += AuditPresetNode(auditJob.Node, auditJob.ParentPath, 0);
        }
        m_PresetJobs.Clear();
        Print("[BDE Loadout preset v3] Finished mismatches=" + mismatches.ToString());
        if (mismatches > 0)
        {
            m_Status.SetText(mismatches.ToString() + " saved slots could not be previewed. Details recorded in the client script log.");
        }
        else
        {
            m_Status.SetText("Loadout applied. Prices and availability will be checked by Buy & Equip.");
        }
    }

    protected string PresetPathText(array<int> path)
    {
        string result;
        foreach (int slotId: path)
        {
            if (result != "")
            {
                result += "/";
            }
            result += InventorySlots.GetSlotName(slotId);
        }
        return result;
    }

    protected int AuditPresetNode(BDE_EMU_SavedNode node, array<int> parentPath, int depth)
    {
        if (!node || depth > 8)
        {
            return 0;
        }
        if (node.SlotName == "Hands" || node.SlotName == "Head" || node.SlotName == "LeftHand")
        {
            return 0;
        }
        ref array<int> path = new array<int>;
        path.Copy(parentPath);
        path.Insert(InventorySlots.GetSlotIdFromString(node.SlotName));
        EntityAI actual = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, path, path.Count());
        string actualType;
        if (actual)
        {
            actualType = actual.GetType();
        }
        int mismatches = 0;
        if (!CF_String.EqualsIgnoreCase(actualType, node.ClassName))
        {
            mismatches++;
            Print("[BDE Loadout preset v3] MISMATCH path=" + PresetPathText(path) + " saved=" + node.ClassName + " preview=" + actualType);
        }
        if (node.Children)
        {
            foreach (BDE_EMU_SavedNode child: node.Children)
            {
                mismatches += AuditPresetNode(child, path, depth + 1);
            }
        }
        return mismatches;
    }

    protected void StageSavedNode(BDE_EMU_SavedNode node, array<int> parentPath, bool newParent, int depth)
    {
        if (!node || depth > 8)
        {
            Print("[BDE Loadout preset v3] Stage skipped: null node or depth limit " + depth.ToString());
            return;
        }
        int slotId = InventorySlots.GetSlotIdFromString(node.SlotName);
        if (slotId == InventorySlots.INVALID)
        {
            Print("[BDE Loadout preset v3] Invalid saved slot name=" + node.SlotName + " id=" + slotId.ToString());
            return;
        }
        if (node.SlotName == "Hands" || node.SlotName == "Head" || node.SlotName == "LeftHand")
        {
            return;
        }
        array<int> path = new array<int>;
        path.Copy(parentPath);
        path.Insert(slotId);
        EntityAI currentItem;
        if (!newParent)
        {
            currentItem = BDE_EMU_LoadoutPaths.Resolve(m_Player, path, path.Count());
        }
        bool retained = currentItem && CF_String.EqualsIgnoreCase(currentItem.GetType(), node.ClassName);
        Print("[BDE Loadout preset v3] Stage path=" + PresetPathText(path) + " wanted=" + node.ClassName + " retained=" + retained.ToString() + " newParent=" + newParent.ToString());
        if (!retained && (node.ClassName != "" || currentItem || newParent))
        {
            ref BDE_EMU_LoadoutChange change = new BDE_EMU_LoadoutChange;
            change.Slots.Copy(path);
            change.Offer = new BDE_EMU_LoadoutOffer;
            change.Offer.ClassName = node.ClassName;
            change.Offer.ItemID = -1;
            change.Offer.CanBuy = true;
            change.Offer.PriceText = "Price at checkout";
            if (node.ClassName == "")
            {
                change.Offer.Kind = 1;
            }
            m_Changes.Insert(change);
        }
        if (node.ClassName != "" && node.Children)
        {
            foreach (BDE_EMU_SavedNode child: node.Children)
            {
                StageSavedNode(child, path, !retained, depth + 1);
            }
        }
    }

    protected void CancelConfirmation()
    {
        if (m_ConfirmToken != 0 && m_Module)
        {
            m_Module.BDE_LM_Cancel(m_Station, m_ConfirmToken);
        }
        m_ConfirmToken = 0;
        m_ConfirmRemaining = 0;
    }

    protected void OnPrepared(int token, int missing, int total, array<ref BDE_EMU_LoadoutChange> changes, array<ref BDE_EMU_AmmoChange> ammunition)
    {
        if (m_Closed)
        {
            m_Module.BDE_LM_Cancel(m_Station, token);
            return;
        }
        m_Busy = false;
        m_Changes = changes;
        m_AmmoChanges = ammunition;
        m_ConfirmToken = token;
        m_ConfirmRemaining = 10;
        m_ConfirmTotal = total;
        m_Selection = null;
        m_BrowseParent = null;
        ClearCards();
        RebuildDraft();
        BuildSlots();
        BuildPreview();
        UpdateTotal();
        m_Status.SetText(missing.ToString() + " slot items are not available. Press Confirm Purchase to confirm purchase. Total: " + total.ToString());
    }

    void NextPresetJob()
    {
        if (m_Closed || !m_ApplyingPreset)
        {
            return;
        }
        m_PresetWait = 0;
        m_PresetJob = null;
        while (m_PresetJobs.Count() > 0)
        {
            ref BDE_EMU_PresetJob job = m_PresetJobs[0];
            m_PresetJobs.Remove(0);
            if (!job || !job.Node || !job.ParentPath || job.Node.SlotName == "Head" || job.Node.SlotName == "Hands" || job.Node.SlotName == "LeftHand" || job.Node.SlotName == "Splint_Right")
            {
                m_PresetSkipped++;
                continue;
            }
            EntityAI parent = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, job.ParentPath, job.ParentPath.Count());
            int slotId = InventorySlots.GetSlotIdFromString(job.Node.SlotName);
            if (!parent || job.ParentPath.Count() > 8 || !BDE_EMU_LoadoutUtil.HasSlot(parent, slotId))
            {
                m_PresetSkipped++;
                continue;
            }
            m_Selection = new BDE_EMU_LoadoutSlot;
            m_Selection.Parent = parent;
            m_Selection.SlotId = slotId;
            m_Selection.Slots.Copy(job.ParentPath);
            m_Selection.Slots.Insert(slotId);
            m_PresetJob = job;
            m_Status.SetText("Checking saved loadout against current traders...");
            m_Request = RequestSlotOffers(m_Selection);
            return;
        }
        FinishPreset("");
    }

    protected void ReceivePresetOffers(string message)
    {
        if (!m_PresetJob || !m_Selection)
        {
            FinishPreset("Saved loadout request lost its slot.");
            return;
        }
        BDE_EMU_LoadoutOffer offer;
        bool applied;
        array<int> path = new array<int>;
        path.Copy(m_Selection.Slots);
        EntityAI currentItem = BDE_EMU_LoadoutPaths.Resolve(m_PlayerClone, path, path.Count());
        string desired = m_PresetJob.Node.ClassName;
        if (message == "" && desired == "")
        {
            foreach (BDE_EMU_LoadoutOffer empty: m_Module.BDE_LM_ClientOffers)
            {
                if (empty.Kind == 1 && empty.CanBuy && (!offer || empty.SalePrice > offer.SalePrice))
                {
                    offer = empty;
                }
            }
            if (offer)
            {
                StageOffer(offer, path);
                applied = true;
            }
        }
        else if (message == "")
        {
            offer = FindPresetOffer(desired);
            if (offer && FitsPresetOffer(offer))
            {
                string currentType;
                if (currentItem)
                {
                    currentType = currentItem.GetType();
                }
                currentType.ToLower();
                desired.ToLower();
                if (currentType != desired)
                {
                    StageOffer(offer, path);
                }
                applied = true;
            }
        }
        if (applied)
        {
            m_PresetApplied++;
            if (desired != "" && m_PresetJob.Node.Children)
            {
                // Parents are staged before their child slots are requested.
                foreach (BDE_EMU_SavedNode child: m_PresetJob.Node.Children)
                {
                    if (child)
                    {
                        BDE_EMU_PresetJob next = new BDE_EMU_PresetJob;
                        next.Node = child;
                        next.ParentPath.Copy(path);
                        m_PresetJobs.Insert(next);
                    }
                }
            }
        }
        else
        {
            m_PresetSkipped++;
        }
        m_PresetWait = 0;
        m_Request = -1;
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(NextPresetJob, 200, false);
    }

    protected void FinishPreset(string message)
    {
        m_ApplyingPreset = false;
        m_PresetJobs.Clear();
        m_PresetJob = null;
        GetGame().GetCallQueue(CALL_CATEGORY_GUI).Remove(NextPresetJob);
        BuildSlots();
        BuildPreview();
        RefreshOffers();
        m_PresetSummary = "Loadout staged: " + m_PresetApplied.ToString() + " slots checked, " + m_PresetSkipped.ToString() + " unavailable slots skipped. Review before Buy & Equip. " + message;
        m_Status.SetText(m_PresetSummary);
    }
}
