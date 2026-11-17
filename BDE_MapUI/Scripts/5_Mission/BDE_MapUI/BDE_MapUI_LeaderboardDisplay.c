class BDE_MapUI_LeaderboardDisplay
{
    private Widget m_RootWidget;
    private TextListboxWidget m_PlayerList;
    private ButtonWidget m_FilterAllButton;
    private ButtonWidget m_FilterKillsButton;
    private ButtonWidget m_FilterRichestButton;
    private TextWidget m_StatusText;
    private TextWidget m_SelectedNameText;
    private TextWidget m_SelectedStatusText;
    private TextWidget m_BankMoneyText;
    private TextWidget m_PlayerKillsText;
    private TextWidget m_AIKillsText;
    private TextWidget m_InfectedKilledText;
    private TextWidget m_TotalCombatKillsText;
    private TextWidget m_DistanceTravelledText;
    private TextWidget m_TimeSurvivedText;
    private TextWidget m_LongestSurvivorHitText;
    private TextWidget m_PreviewPlaceholderText;
    private PlayerPreviewWidget m_PlayerPreview;
    private PlayerBase m_OfflinePreviewPlayer;
    private string m_OfflinePreviewUID;

    private ref array<ref BDE_MapUI_LeaderboardListEntry> m_CurrentEntries;
    private ref BDE_MapUI_PlayerRecord m_SelectedRecord;
    private int m_CurrentFilter;
    private string m_SelectedUID;
    private bool m_AutoSelectFirstOnNextList;
    private vector m_PlayerPreviewOrientation;
    private int m_MouseX;
    private int m_MouseY;

    void BDE_MapUI_LeaderboardDisplay()
    {
        m_CurrentEntries = new array<ref BDE_MapUI_LeaderboardListEntry>();
        m_CurrentFilter = BDE_MapUI_LEADERBOARD_FILTER_ALL;
        m_SelectedUID = "";
        m_AutoSelectFirstOnNextList = false;
        m_OfflinePreviewUID = "";
        m_PlayerPreviewOrientation = vector.Zero;
    }

    void ~BDE_MapUI_LeaderboardDisplay()
    {
        OnMenuHide();
    }

    void Init(Widget rootWidget)
    {
        Widget parent = rootWidget.FindAnyWidget("Tab_2");
        if (!parent)
            return;

        Widget oldStaticScroll = rootWidget.FindAnyWidget("ScrollWidget2");
        if (oldStaticScroll)
            oldStaticScroll.Show(false);

        m_RootWidget = g_Game.GetWorkspace().CreateWidgets("BDE_MapUI/GUI/layouts/LeaderboardDisplay.layout", parent);
        if (!m_RootWidget)
            return;

        m_PlayerList = TextListboxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsPlayerList"));
        m_FilterAllButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsFilterAll"));
        m_FilterKillsButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsFilterKills"));
        m_FilterRichestButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsFilterRichest"));
        m_StatusText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsStatusText"));
        m_SelectedNameText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsSelectedName"));
        m_SelectedStatusText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsSelectedStatus"));
        m_BankMoneyText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsBankValue"));
        m_PlayerKillsText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsPlayerKillsValue"));
        m_AIKillsText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsAIKillsValue"));
        m_InfectedKilledText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsInfectedValue"));
        m_TotalCombatKillsText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsTotalKillsValue"));
        m_DistanceTravelledText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsDistanceValue"));
        m_TimeSurvivedText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsTimeValue"));
        m_LongestSurvivorHitText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsLongestHitValue"));
        m_PreviewPlaceholderText = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsPreviewPlaceholder"));
        m_PlayerPreview = PlayerPreviewWidget.Cast(m_RootWidget.FindAnyWidget("BDE_StatsPlayerPreview"));

        if (m_PlayerList)
            WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_PlayerList, this, "OnPlayerSelected");
        WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_RootWidget.FindAnyWidget("BDE_StatsPlayerListAll"), this, "OnPlayerSelected");
        if (m_PlayerPreview)
            WidgetEventHandler.GetInstance().RegisterOnMouseButtonDown(m_PlayerPreview, this, "OnPreviewMouseDown");

        SetStatus("Select a player to view stats.", BDE_MapUI_DisplayColors.WHITE);
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

    void OnTabShown()
    {
        m_AutoSelectFirstOnNextList = true;
        RequestList();
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
            m_CurrentFilter = BDE_MapUI_LEADERBOARD_FILTER_ALL;
            ApplyFilterButtonColors();
            RequestList();
            return true;
        }
        if (w == m_FilterKillsButton)
        {
            m_CurrentFilter = BDE_MapUI_LEADERBOARD_FILTER_KILLS;
            ApplyFilterButtonColors();
            RequestList();
            return true;
        }
        if (w == m_FilterRichestButton)
        {
            m_CurrentFilter = BDE_MapUI_LEADERBOARD_FILTER_RICHEST;
            ApplyFilterButtonColors();
            RequestList();
            return true;
        }

        return false;
    }

    private void RequestList()
    {
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RequestLeaderboardList", new Param1<int>(m_CurrentFilter), true, NULL);
    }

    private void RequestDetails(string uid)
    {
        if (uid == "")
            return;
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_RequestLeaderboardDetails", new Param1<string>(uid), true, NULL);
    }

    void ReceiveLeaderboardList(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param1<ref array<ref BDE_MapUI_LeaderboardListEntry>> data;
        if (!ctx.Read(data))
        {
            SetStatus("Leaderboard list read failed. Version mismatch?", BDE_MapUI_DisplayColors.RED);
            return;
        }

        m_CurrentEntries = data.param1;
        RenderList();
    }

    void ReceiveLeaderboardDetails(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param1<ref BDE_MapUI_PlayerRecord> data;
        if (!ctx.Read(data))
        {
            SetStatus("Leaderboard details read failed. Version mismatch?", BDE_MapUI_DisplayColors.RED);
            return;
        }

        m_SelectedRecord = data.param1;
        RenderDetails();
    }

    void OnPlayerSelected()
    {
        if (!m_PlayerList)
            return;

        int selectedIndex = m_PlayerList.GetSelectedRow();
        if (selectedIndex < 0)
            return;

        BDE_MapUI_LeaderboardListEntry entry;
        m_PlayerList.GetItemData(selectedIndex, 0, entry);
        if (!entry)
            return;

        m_SelectedUID = entry.UID;
        ShowListEntryAsLoading(entry);
        RequestDetails(m_SelectedUID);
    }

    private void RenderList()
    {
        if (!m_PlayerList)
            return;

        Widget standardList = m_RootWidget.FindAnyWidget("BDE_StatsPlayerList");
        Widget allList = m_RootWidget.FindAnyWidget("BDE_StatsPlayerListAll");
        bool allPlayers = m_CurrentFilter == BDE_MapUI_LEADERBOARD_FILTER_ALL;
        standardList.Show(!allPlayers);
        allList.Show(allPlayers);
        m_PlayerList = TextListboxWidget.Cast(standardList);
        if (allPlayers)
        {
            m_PlayerList = TextListboxWidget.Cast(allList);
        }
        m_PlayerList.ClearItems();

        if (!m_CurrentEntries)
            m_CurrentEntries = new array<ref BDE_MapUI_LeaderboardListEntry>();

        int selectedIndex = -1;
        for (int i = 0; i < m_CurrentEntries.Count(); i++)
        {
            BDE_MapUI_LeaderboardListEntry entry = m_CurrentEntries.Get(i);
            if (!entry)
                continue;

            string status = "OFF";
            if (entry.IsOnline)
                status = "ON";

            int row = m_PlayerList.AddItem(status, entry, 0);
            if (m_CurrentFilter == BDE_MapUI_LEADERBOARD_FILTER_KILLS)
            {
                m_PlayerList.SetItem(row, "| PK:" + entry.PlayerKills.ToString() + " AI:" + entry.AIKills.ToString(), entry, 1);
                m_PlayerList.SetItem(row, "| " + entry.Name, entry, 2);
            }
            else if (m_CurrentFilter == BDE_MapUI_LEADERBOARD_FILTER_RICHEST)
            {
                m_PlayerList.SetItem(row, "| " + FormatMoney(entry.BankMoney), entry, 1);
                m_PlayerList.SetItem(row, "| " + entry.Name, entry, 2);
            }
            else
            {
                m_PlayerList.SetItem(row, "| " + entry.Name, entry, 1);
            }

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

        BDE_MapUI_LeaderboardListEntry selectedEntry = m_CurrentEntries.Get(selectedIndex);
        if (selectedEntry)
        {
            m_SelectedUID = selectedEntry.UID;
            ShowListEntryAsLoading(selectedEntry);
            RequestDetails(m_SelectedUID);
        }
    }

    private void ShowListEntryAsLoading(BDE_MapUI_LeaderboardListEntry entry)
    {
        if (!entry)
            return;

        if (m_SelectedNameText)
            m_SelectedNameText.SetText(entry.Name);
        if (m_SelectedStatusText)
        {
            m_SelectedStatusText.SetText(BDE_MapUI_PresenceText.Status(entry.IsOnline, entry.IsRemoteServer, entry.CurrentMapName));
        }
        
        SetStatsLoading();
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
        RenderStats(m_SelectedRecord);

        ApplyPreviewForSelectedRecord();
    }

    private void RenderStats(BDE_MapUI_PlayerRecord record)
    {
        if (!record)
        {
            SetStatsEmpty();
            return;
        }

        int totalCombatKills = record.PlayerKills + record.AIKills;
        SetStatText(m_BankMoneyText, FormatMoney(record.BankMoney));
        SetStatText(m_PlayerKillsText, record.PlayerKills.ToString());
        SetStatText(m_AIKillsText, record.AIKills.ToString());
        SetStatText(m_InfectedKilledText, record.InfectedKilled.ToString());
        SetStatText(m_TotalCombatKillsText, totalCombatKills.ToString());
        SetStatText(m_DistanceTravelledText, FormatDistance(record.DistanceTravelled));
        SetStatText(m_TimeSurvivedText, FormatTime(record.TimeSurvived));
        SetStatText(m_LongestSurvivorHitText, FormatDistance(record.LongestSurvivorHit));
    }

    private void SetStatsLoading()
    {
        SetAllStatsText("...");
    }

    private void SetStatsEmpty()
    {
        SetAllStatsText("-");
    }

    private void SetAllStatsText(string value)
    {
        SetStatText(m_BankMoneyText, value);
        SetStatText(m_PlayerKillsText, value);
        SetStatText(m_AIKillsText, value);
        SetStatText(m_InfectedKilledText, value);
        SetStatText(m_TotalCombatKillsText, value);
        SetStatText(m_DistanceTravelledText, value);
        SetStatText(m_TimeSurvivedText, value);
        SetStatText(m_LongestSurvivorHitText, value);
    }

    private void SetStatText(TextWidget widget, string value)
    {
        if (!widget)
            return;
        
        widget.SetColor(BDE_MapUI_DisplayColors.WHITE);
        widget.SetText(value);
    }

    private string FormatMoney(int amount)
    {
        if (amount < 0)
            amount = 0;
        return "$" + amount.ToString();
    }

    private string FormatDistance(float distance)
    {
        if (distance < 0)
            distance = 0;

        int meters = Math.Floor(distance);
        if (meters >= 1000)
        {
            int kmWhole = Math.Floor(meters / 1000);
            int kmDecimal = Math.Floor((meters - (kmWhole * 1000)) / 100);
            return kmWhole.ToString() + "." + kmDecimal.ToString() + " km";
        }

        return meters.ToString() + " m";
    }

    private string FormatTime(float seconds)
    {
        if (seconds <= 0)
            return "0s";

        FullTimeData timeData = new FullTimeData();
        TimeConversions.ConvertSecondsToFullTime(seconds, timeData);
        return timeData.FormatedNonZero();
    }

    private void ClearDetails()
    {
        DestroyOfflinePreviewPlayer();

        if (m_SelectedNameText)
            m_SelectedNameText.SetText("No player selected");
        if (m_SelectedStatusText)
            m_SelectedStatusText.SetText("-");
        SetStatsEmpty();
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
        ApplyFilterButtonColor(m_FilterAllButton, m_CurrentFilter == BDE_MapUI_LEADERBOARD_FILTER_ALL);
        ApplyFilterButtonColor(m_FilterKillsButton, m_CurrentFilter == BDE_MapUI_LEADERBOARD_FILTER_KILLS);
        ApplyFilterButtonColor(m_FilterRichestButton, m_CurrentFilter == BDE_MapUI_LEADERBOARD_FILTER_RICHEST);
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
