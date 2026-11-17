class BDE_MapUI_BountyDisplay
{
    private Widget m_RootWidget;
    private TextListboxWidget m_PlayerList;
    private ButtonWidget m_FilterAllButton;
    private ButtonWidget m_FilterOnlineButton;
    private ButtonWidget m_FilterBountiesButton;
    private ButtonWidget m_AddBountyButton;
    private ButtonWidget m_RemoveBountyButton;
    private EditBoxWidget m_AmountEditBox;
    private TextWidget m_StatusText;
    private TextWidget m_SelectedNameText;
    private TextWidget m_SelectedStatusText;
    private TextWidget m_SelectedBountyText;
    private TextWidget m_SelectedLocationText;
    private MultilineTextWidget m_ContributorsText;
    private MultilineTextWidget m_GearText;
    private TextWidget m_PreviewPlaceholderText;
    private PlayerPreviewWidget m_PlayerPreview;
    private PlayerBase m_OfflinePreviewPlayer;
    private string m_OfflinePreviewUID;

    private ref array<ref BDE_MapUI_BountyListEntry> m_CurrentEntries;
    private ref BDE_MapUI_PlayerRecord m_SelectedRecord;
    private int m_CurrentFilter;
    private string m_SelectedUID;
    private bool m_AutoSelectFirstOnNextList;
    private vector m_PlayerPreviewOrientation;
    private int m_MouseX;
    private int m_MouseY;

    void BDE_MapUI_BountyDisplay()
    {
        m_CurrentEntries = new array<ref BDE_MapUI_BountyListEntry>();
        m_CurrentFilter = BDE_MapUI_BOUNTY_FILTER_ALL;
        m_SelectedUID = "";
        m_AutoSelectFirstOnNextList = false;
        m_OfflinePreviewUID = "";
        m_PlayerPreviewOrientation = vector.Zero;
    }

    void ~BDE_MapUI_BountyDisplay()
    {
        OnMenuHide();
    }

    void Init(Widget rootWidget)
    {
        Widget parent = rootWidget.FindAnyWidget("Tab_0");
        if (!parent)
            return;

        Widget oldStaticScroll = rootWidget.FindAnyWidget("ScrollWidget0");
        if (oldStaticScroll)
            oldStaticScroll.Show(false);

        m_RootWidget = g_Game.GetWorkspace().CreateWidgets("BDE_MapUI/GUI/layouts/BountyDisplay.layout", parent);

        m_PlayerList = TextListboxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyPlayerList"));
        m_FilterAllButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyFilterAll"));
        m_FilterOnlineButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyFilterOnline"));
        m_FilterBountiesButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyFilterActive"));
        m_AddBountyButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyAddButton"));
        m_RemoveBountyButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyRemoveButton"));
        m_AmountEditBox = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyAmountEdit"));
        m_StatusText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyStatusText"));
        m_SelectedNameText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountySelectedName"));
        m_SelectedStatusText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountySelectedStatus"));
        m_SelectedBountyText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountySelectedAmount"));
        m_SelectedLocationText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountySelectedLocation"));
        m_ContributorsText = MultilineTextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyContributorsText"));
        m_GearText = MultilineTextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyGearText"));
        m_PreviewPlaceholderText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyPreviewPlaceholder"));
        m_PlayerPreview = PlayerPreviewWidget.Cast(m_RootWidget.FindAnyWidget("BDE_BountyPlayerPreview"));

        Widget gearPanel = m_RootWidget.FindAnyWidget("BDE_BountyGearPanel");
        if (gearPanel)
            gearPanel.Show(false);

        WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_PlayerList, this, "OnPlayerSelected");
        WidgetEventHandler.GetInstance().RegisterOnMouseButtonDown(m_PlayerPreview, this, "OnPreviewMouseDown");

        if (m_AmountEditBox)
            m_AmountEditBox.SetText(BDE_MapUI_ConfigManager.Get().MinBountyAmount.ToString());

        SetStatus("Select a player to view bounty details.", BDE_MapUI_DisplayColors.WHITE);
        ApplyFilterButtonColors();
        ClearDetails();
    }

    void OnMenuShow()
    {
        m_AutoSelectFirstOnNextList = true;
        RequestList();
    }

    void OnMenuHide()
    {
        DestroyOfflinePreviewPlayer();
    }

    void Update(float timeslice)
    {
    }

    bool OnClick(Widget w, int x, int y, int button)
    {
        if (button != MouseState.LEFT)
            return false;

        if (w == m_FilterAllButton)
        {
            m_CurrentFilter = BDE_MapUI_BOUNTY_FILTER_ALL;
            ApplyFilterButtonColors();
            RequestList();
            return true;
        }
        if (w == m_FilterOnlineButton)
        {
            m_CurrentFilter = BDE_MapUI_BOUNTY_FILTER_ONLINE;
            ApplyFilterButtonColors();
            RequestList();
            return true;
        }
        if (w == m_FilterBountiesButton)
        {
            m_CurrentFilter = BDE_MapUI_BOUNTY_FILTER_ACTIVE;
            ApplyFilterButtonColors();
            RequestList();
            return true;
        }
        if (w == m_AddBountyButton)
        {
            SendAddBounty();
            return true;
        }
        if (w == m_RemoveBountyButton)
        {
            SendRemoveOwnBounty();
            return true;
        }

        return false;
    }

    private void RequestList()
    {
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RequestBountyList", new Param1<int>(m_CurrentFilter), true, NULL);
    }

    private void RequestDetails(string uid)
    {
        if (uid == "")
            return;
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RequestBountyDetails", new Param1<string>(uid), true, NULL);
    }

    private void SendAddBounty()
    {
        if (m_SelectedUID == "")
        {
            SetStatus("Select a player first.", BDE_MapUI_DisplayColors.YELLOW);
            return;
        }

        int amount = GetEnteredAmount();
        if (amount < BDE_MapUI_ConfigManager.Get().MinBountyAmount)
        {
            SetStatus("Minimum bounty is $" + BDE_MapUI_ConfigManager.Get().MinBountyAmount.ToString() + ".", BDE_MapUI_DisplayColors.YELLOW);
            return;
        }

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_AddBounty", new Param2<string, int>(m_SelectedUID, amount), true, NULL);
    }

    private void SendRemoveOwnBounty()
    {
        if (m_SelectedUID == "")
        {
            SetStatus("Select a player first.", BDE_MapUI_DisplayColors.YELLOW);
            return;
        }

        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RemoveOwnBounty", new Param1<string>(m_SelectedUID), true, NULL);
    }

    private int GetEnteredAmount()
    {
        if (!m_AmountEditBox)
            return 0;

        string amountText = m_AmountEditBox.GetText();
        amountText.Replace("$", "");
        amountText.Replace(",", "");
        amountText.Replace(" ", "");
        return amountText.ToInt();
    }

    void OnPlayerSelected()
    {
        if (!m_PlayerList)
            return;

        int selectedIndex = m_PlayerList.GetSelectedRow();
        if (selectedIndex < 0)
            return;

        BDE_MapUI_BountyListEntry entry;
        m_PlayerList.GetItemData(selectedIndex, 0, entry);
        if (!entry)
            return;

        m_SelectedUID = entry.UID;
        ShowListEntryAsLoading(entry);
        RequestDetails(m_SelectedUID);
    }

    void ReceiveBountyList(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param1<ref array<ref BDE_MapUI_BountyListEntry>> data;
        if (!ctx.Read(data))
        {
            SetStatus("Bounty list read failed. Version mismatch?", BDE_MapUI_DisplayColors.RED);
            return;
        }

        m_CurrentEntries = data.param1;
        RenderList();
    }

    void ReceiveBountyDetails(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param1<ref BDE_MapUI_PlayerRecord> data;
        if (!ctx.Read(data))
        {
            SetStatus("Bounty details read failed. Version mismatch?", BDE_MapUI_DisplayColors.RED);
            return;
        }

        m_SelectedRecord = data.param1;
        RenderDetails();
    }

    void ReceiveBountyActionResult(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param3<bool, string, string> data;
        if (!ctx.Read(data))
            return;

        if (data.param1)
            SetStatus(data.param2, BDE_MapUI_DisplayColors.GREEN);
        else
            SetStatus(data.param2, BDE_MapUI_DisplayColors.RED);

        if (data.param3 != "")
        {
            m_SelectedUID = data.param3;
            RequestDetails(m_SelectedUID);
        }

        RequestList();
    }

    private void RenderList()
    {
        if (!m_PlayerList)
            return;

        m_PlayerList.ClearItems();

        if (!m_CurrentEntries)
            m_CurrentEntries = new array<ref BDE_MapUI_BountyListEntry>();

        int selectedIndex = -1;
        for (int i = 0; i < m_CurrentEntries.Count(); i++)
        {
            BDE_MapUI_BountyListEntry entry = m_CurrentEntries.Get(i);
            if (!entry)
                continue;

            string status = "OFF";
            if (entry.IsOnline)
                status = "ON";

            string bountyText = "No Bounty";
            if (entry.HasActiveBounty)
                bountyText = "$" + entry.BountyAmount.ToString();

            int row = m_PlayerList.AddItem(status, entry, 0);
            m_PlayerList.SetItem(row, "| " + bountyText, entry, 1);
            m_PlayerList.SetItem(row, "| " + entry.Name, entry, 2);

            if (m_SelectedUID != "" && entry.UID == m_SelectedUID)
                selectedIndex = i;
        }

        SetStatus("Players loaded: " + m_CurrentEntries.Count().ToString(), BDE_MapUI_DisplayColors.WHITE);

        if (m_CurrentEntries.Count() == 0)
        {
            m_SelectedUID = "";
            m_SelectedRecord = null;
            ClearDetails();
            m_AutoSelectFirstOnNextList = false;
            return;
        }

        if (m_AutoSelectFirstOnNextList || selectedIndex < 0)
            selectedIndex = 0;

        m_AutoSelectFirstOnNextList = false;
        m_PlayerList.SelectRow(selectedIndex);

        BDE_MapUI_BountyListEntry selectedEntry = m_CurrentEntries.Get(selectedIndex);
        if (selectedEntry)
        {
            m_SelectedUID = selectedEntry.UID;
            ShowListEntryAsLoading(selectedEntry);
            RequestDetails(m_SelectedUID);
        }
    }

    private void ShowListEntryAsLoading(BDE_MapUI_BountyListEntry entry)
    {
        if (!entry)
            return;

        if (m_SelectedNameText)
            m_SelectedNameText.SetText(entry.Name);
        if (m_SelectedStatusText)
        {
            m_SelectedStatusText.SetText(BDE_MapUI_PresenceText.Status(entry.IsOnline, entry.IsRemoteServer, entry.CurrentMapName));
        }
        if (m_SelectedBountyText)
            m_SelectedBountyText.SetText("$" + entry.BountyAmount.ToString());
        if (m_SelectedLocationText)
            m_SelectedLocationText.SetText(GetDisplayLocation(entry.HasActiveBounty || entry.IsRemoteServer, entry.LastKnownLocation));
        if (m_ContributorsText)
            m_ContributorsText.SetText("Loading contributors...");
        if (m_GearText)
            m_GearText.SetText("");
    }

    private void RenderDetails()
    {
        if (!m_SelectedRecord)
        {
            ClearDetails();
            return;
        }

        m_SelectedRecord.EnsureArrays();
        m_SelectedRecord.RecalculateBounty();

        if (m_SelectedNameText)
            m_SelectedNameText.SetText(m_SelectedRecord.Name);
        if (m_SelectedStatusText)
        {
            m_SelectedStatusText.SetText(BDE_MapUI_PresenceText.Status(m_SelectedRecord.IsOnline, m_SelectedRecord.IsRemoteServer, m_SelectedRecord.CurrentMapName));
        }
        if (m_SelectedBountyText)
            m_SelectedBountyText.SetText("$" + m_SelectedRecord.BountyAmount.ToString());
        if (m_SelectedLocationText)
            m_SelectedLocationText.SetText(GetDisplayLocation(m_SelectedRecord.HasActiveBounty || m_SelectedRecord.IsRemoteServer, m_SelectedRecord.LastKnownLocation));
        if (m_ContributorsText)
            m_ContributorsText.SetText(BuildContributorText(m_SelectedRecord));
        if (m_GearText)
            m_GearText.SetText("");

        ApplyPreviewForSelectedRecord();
    }

    private string GetDisplayLocation(bool hasActiveBounty, string locationName)
    {
        if (!hasActiveBounty)
            return "Unknown";

        if (locationName == "")
            return "Unknown";

        return locationName;
    }

    private string BuildContributorText(BDE_MapUI_PlayerRecord record)
    {
        string text = "Placed by:\n";
        if (!record || !record.BountyContributors || record.BountyContributors.Count() == 0)
            return text + "None";

        for (int i = 0; i < record.BountyContributors.Count(); i++)
        {
            BDE_MapUI_BountyContributor contributor = record.BountyContributors.Get(i);
            if (!contributor || contributor.Amount <= 0)
                continue;
            text += contributor.Name + " - $" + contributor.Amount.ToString() + "\n";
        }
        return text;
    }

    private string BuildGearText(BDE_MapUI_PlayerRecord record)
    {
        string text = "Last gear snapshot:\n";
        if (!record || !record.Gear || record.Gear.Count() == 0)
            return text + "No gear snapshot yet.";

        int shown = 0;
        for (int i = 0; i < record.Gear.Count(); i++)
        {
            BDE_MapUI_GearItem gearItem = record.Gear.Get(i);
            if (!gearItem)
                continue;

            string line = gearItem.DisplayName;
            if (line == "")
                line = gearItem.ClassName;
            if (gearItem.SlotName != "")
                line += " [" + gearItem.SlotName + "]";
            if (gearItem.InHands)
                line += " (Hands)";

            text += line + "\n";
            shown++;
            if (shown >= 24)
            {
                text += "...";
                break;
            }
        }

        return text;
    }

    private void ClearDetails()
    {
        DestroyOfflinePreviewPlayer();

        if (m_SelectedNameText)
            m_SelectedNameText.SetText("No player selected");
        if (m_SelectedStatusText)
            m_SelectedStatusText.SetText("-");
        if (m_SelectedBountyText)
            m_SelectedBountyText.SetText("$0");
        if (m_SelectedLocationText)
            m_SelectedLocationText.SetText("Unknown");
        if (m_ContributorsText)
            m_ContributorsText.SetText("Placed by:\nNone");
        if (m_GearText)
            m_GearText.SetText("");
        if (m_PlayerPreview)
            m_PlayerPreview.Show(false);
        if (m_PreviewPlaceholderText)
            m_PreviewPlaceholderText.Show(true);
    }

    private void ApplyPreviewForSelectedRecord()
    {
        if (!m_PlayerPreview || !m_SelectedRecord)
            return;

        PlayerBase localPlayer = PlayerBase.Cast(g_Game.GetPlayer());
        string localUID = "";
        if (localPlayer && localPlayer.GetIdentity())
            localUID = localPlayer.GetIdentity().GetId();

        if (localUID != "" && localUID == m_SelectedRecord.UID && localPlayer)
        {
            DestroyOfflinePreviewPlayer();
            m_PlayerPreview.Show(true);
            if (m_PreviewPlaceholderText)
                m_PreviewPlaceholderText.Show(false);
            m_PlayerPreview.SetPlayer(localPlayer);
            m_PlayerPreview.SetModelPosition("0 0 0.605");
            m_PlayerPreview.SetModelOrientation(m_PlayerPreviewOrientation);
            return;
        }

        if (BuildOfflinePreviewPlayer(m_SelectedRecord))
        {
            m_PlayerPreview.Show(true);
            if (m_PreviewPlaceholderText)
                m_PreviewPlaceholderText.Show(false);
            m_PlayerPreview.SetPlayer(m_OfflinePreviewPlayer);
            m_PlayerPreview.SetModelPosition("0 0 0.605");
            m_PlayerPreview.SetModelOrientation(m_PlayerPreviewOrientation);
            return;
        }

        m_PlayerPreview.Show(false);
        if (m_PreviewPlaceholderText)
            m_PreviewPlaceholderText.Show(true);
    }

    private bool BuildOfflinePreviewPlayer(BDE_MapUI_PlayerRecord record)
    {
        if (!record)
            return false;

        DestroyOfflinePreviewPlayer();

        string characterType = record.CharacterType;
        if (characterType == "")
        {
            PlayerBase localPlayer = PlayerBase.Cast(g_Game.GetPlayer());
            if (localPlayer)
                characterType = localPlayer.GetType();
        }
        if (characterType == "")
            characterType = "SurvivorM_Mirek";

        if (!g_Game.ConfigIsExisting("CfgVehicles " + characterType))
            characterType = "SurvivorM_Mirek";

        vector spawnPos = GetOfflinePreviewSpawnPosition();
        m_OfflinePreviewPlayer = PlayerBase.Cast(g_Game.CreateObjectEx(characterType, spawnPos, ECE_LOCAL|ECE_NOLIFETIME));
        if (!m_OfflinePreviewPlayer)
            return false;

        m_OfflinePreviewUID = record.UID;
        m_OfflinePreviewPlayer.SetOrientation("0 0 0");
        ApplyGearSnapshotToPreview(record);
        return true;
    }

    private vector GetOfflinePreviewSpawnPosition()
    {
        PlayerBase localPlayer = PlayerBase.Cast(g_Game.GetPlayer());
        if (!localPlayer)
            return "0 0 0";

        vector dir = g_Game.GetCurrentCameraDirection();
        dir.Normalize();
        vector pos = g_Game.GetCurrentCameraPosition() - dir * 0.5;
        pos[1] = localPlayer.GetPosition()[1];
        return pos;
    }

    private void DestroyOfflinePreviewPlayer()
    {
        if (m_OfflinePreviewPlayer)
        {
            // Release the widget's source before destroying the local mannequin.
            if (m_PlayerPreview)
            {
                m_PlayerPreview.SetPlayer(null);
            }
            g_Game.ObjectDelete(m_OfflinePreviewPlayer);
            m_OfflinePreviewPlayer = null;
        }
        m_OfflinePreviewUID = "";
    }

    private void ApplyGearSnapshotToPreview(BDE_MapUI_PlayerRecord record)
    {
        if (!record || !m_OfflinePreviewPlayer || !record.Gear)
            return;

        map<int, EntityAI> spawned = new map<int, EntityAI>();
        array<int> completed = new array<int>();

        int pass = 0;
        bool progress = true;
        while (progress && pass < 10)
        {
            progress = false;
            pass++;

            for (int i = 0; i < record.Gear.Count(); i++)
            {
                if (completed.Find(i) >= 0)
                    continue;

                BDE_MapUI_GearItem gearItem = record.Gear.Get(i);
                if (!gearItem || gearItem.ClassName == "")
                {
                    completed.Insert(i);
                    progress = true;
                    continue;
                }

                int gearID = gearItem.GearID;
                if (gearID < 0)
                    gearID = i;

                EntityAI parentEntity = null;
                if (gearItem.ParentGearID >= 0)
                {
                    if (!spawned.Find(gearItem.ParentGearID, parentEntity) || !parentEntity)
                        continue;
                }

                EntityAI spawnedEntity = SpawnPreviewGearItem(gearItem, parentEntity);
                if (spawnedEntity)
                {
                    spawned.Set(gearID, spawnedEntity);
                    ApplyPreviewItemState(spawnedEntity, gearItem);
                }

                completed.Insert(i);
                progress = true;
            }
        }
    }

    private EntityAI SpawnPreviewGearItem(BDE_MapUI_GearItem gearItem, EntityAI parentEntity)
    {
        if (!gearItem || gearItem.ClassName == "")
            return null;

        if (!g_Game.ConfigIsExisting("CfgVehicles " + gearItem.ClassName))
            return null;

        EntityAI spawnedEntity = null;

        if (parentEntity)
        {
            if (parentEntity.GetInventory())
                spawnedEntity = parentEntity.GetInventory().CreateAttachment(gearItem.ClassName);
            return spawnedEntity;
        }

        if (gearItem.InHands && m_OfflinePreviewPlayer.GetHumanInventory())
            spawnedEntity = m_OfflinePreviewPlayer.GetHumanInventory().CreateInHands(gearItem.ClassName);

        if (!spawnedEntity && gearItem.SlotName != "" && m_OfflinePreviewPlayer.GetInventory())
            spawnedEntity = m_OfflinePreviewPlayer.GetInventory().CreateAttachment(gearItem.ClassName);

        return spawnedEntity;
    }

    private void ApplyPreviewItemState(EntityAI entity, BDE_MapUI_GearItem gearItem)
    {
        if (!entity || !gearItem)
            return;

        if (gearItem.Health > 0)
            entity.SetHealth("", "", gearItem.Health);

        ItemBase itemBase = ItemBase.Cast(entity);
        if (itemBase && itemBase.HasQuantity() && gearItem.Quantity > 0)
            // These are disposable, client-local preview entities.
            itemBase.SetQuantity(gearItem.Quantity, false, false, true);
    }

    private void SetStatus(string message, int color)
    {
        if (!m_StatusText)
            return;
        m_StatusText.SetText(message);
        m_StatusText.SetColor(color);
    }

    private void ApplyFilterButtonColors()
    {
        ApplyFilterButtonColor(m_FilterAllButton, m_CurrentFilter == BDE_MapUI_BOUNTY_FILTER_ALL);
        ApplyFilterButtonColor(m_FilterOnlineButton, m_CurrentFilter == BDE_MapUI_BOUNTY_FILTER_ONLINE);
        ApplyFilterButtonColor(m_FilterBountiesButton, m_CurrentFilter == BDE_MapUI_BOUNTY_FILTER_ACTIVE);
    }

    private void ApplyFilterButtonColor(ButtonWidget button, bool active)
    {
        if (!button)
            return;
        if (active)
            button.SetColor(BDE_MapUI_DisplayColors.ACTIVE_BUTTON_COLOR);
        else
            button.SetColor(BDE_MapUI_DisplayColors.NORMAL_BUTTON_COLOR);
    }

    private bool OnPreviewMouseDown(Widget w, int x, int y, int button)
    {
        if (w != m_PlayerPreview)
            return false;

        GetMousePos(m_MouseX, m_MouseY);
        g_Game.GetDragQueue().Call(this, "UpdatePreviewRotation");
        return true;
    }

    private void UpdatePreviewRotation(int mouse_x, int mouse_y, bool is_dragging)
    {
        if (!m_PlayerPreview)
            return;

        vector orientation = m_PlayerPreviewOrientation;
        orientation[1] = orientation[1] - (m_MouseX - mouse_x);
        m_PlayerPreview.SetModelOrientation(orientation);

        if (!is_dragging)
            m_PlayerPreviewOrientation = orientation;
    }
};
