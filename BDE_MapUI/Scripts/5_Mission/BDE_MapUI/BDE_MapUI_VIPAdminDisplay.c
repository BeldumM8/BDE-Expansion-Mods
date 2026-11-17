class BDE_MapUI_VIPAdminDisplay
{
    private Widget m_RootWidget;
    private TextListboxWidget m_PlayerList;
    private TextListboxWidget m_AssignTierList;
    private TextListboxWidget m_TierList;
    private EditBoxWidget m_Search;
    private ButtonWidget m_FilterAll;
    private ButtonWidget m_FilterVIP;
    private ButtonWidget m_FilterNonVIP;
    private ButtonWidget m_FilterPending;
    private ButtonWidget m_Refresh;
    private ButtonWidget m_AssignButton;
    private ButtonWidget m_DisableButton;
    private EditBoxWidget m_PendingSteam64;
    private ButtonWidget m_PendingAdd;
    private TextWidget m_ListStatus;
    private TextWidget m_SelectedName;
    private MultilineTextWidget m_SelectedIDs;
    private TextWidget m_SelectedState;
    private TextWidget m_SelectedTier;
    private TextWidget m_ActionStatus;

    private EditBoxWidget m_TierName;
    private EditBoxWidget m_IncomeMultiplier;
    private EditBoxWidget m_RepMultiplier;
    private EditBoxWidget m_PointsMultiplier;
    private CheckBoxWidget m_PremiumBattlePass;
    private MultilineTextWidget m_TierPreview;
    private ButtonWidget m_SaveTier;
    private ButtonWidget m_NewTier;
    private TextWidget m_TierStatus;

    private TextWidget m_DonationSummary;
    private EditBoxWidget m_DonationCurrent;
    private EditBoxWidget m_DonationGoal;
    private ButtonWidget m_DonationSave;
    private ButtonWidget m_DonationReset;
    private TextWidget m_DonationStatus;

    private ref BDE_MapUI_VIPAdminSnapshot m_Data;
    private ref array<ref BDE_MapUI_VIPPlayerAdminEntry> m_VisiblePlayers;
    private BDE_MapUI_VIPPlayerAdminEntry m_SelectedPlayer;
    private BDE_MapUI_VIPTierData m_SelectedTierData;
    private int m_Filter;
    private bool m_CanView;
    private bool m_CanManage;
    private string m_LastSearch;
    private float m_SearchPoll;

    void BDE_MapUI_VIPAdminDisplay()
    {
        m_Data = new BDE_MapUI_VIPAdminSnapshot();
        m_VisiblePlayers = new array<ref BDE_MapUI_VIPPlayerAdminEntry>();
        m_Filter = BDE_MAPUI_VIP_FILTER_ALL;
        m_LastSearch = "";
    }

    void Init(Widget rootWidget)
    {
        if (!rootWidget)
            return;

        Widget parent = rootWidget.FindAnyWidget("Tab_VIP");
        if (!parent)
            return;

        m_RootWidget = g_Game.GetWorkspace().CreateWidgets("BDE_MapUI/GUI/layouts/VIPAdminDisplay.layout", parent);
        if (!m_RootWidget)
            return;

        m_PlayerList = TextListboxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPPlayerList"));
        m_AssignTierList = TextListboxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPAssignTierList"));
        m_TierList = TextListboxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPTierList"));
        m_Search = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPSearch"));
        m_FilterAll = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPFilterAll"));
        m_FilterVIP = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPFilterVIP"));
        m_FilterNonVIP = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPFilterNonVIP"));
        m_FilterPending = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPFilterPending"));
        m_Refresh = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPRefresh"));
        m_AssignButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPAssignButton"));
        m_DisableButton = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPDisableButton"));
        m_PendingSteam64 = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPPendingSteam64"));
        m_PendingAdd = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPPendingAdd"));
        m_ListStatus = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPListStatus"));
        m_SelectedName = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPSelectedName"));
        m_SelectedIDs = MultilineTextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPSelectedIDs"));
        m_SelectedState = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPSelectedState"));
        m_SelectedTier = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPSelectedTier"));
        m_ActionStatus = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPActionStatus"));

        m_TierName = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPTierName"));
        m_IncomeMultiplier = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPIncomeMultiplier"));
        m_RepMultiplier = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPRepMultiplier"));
        m_PointsMultiplier = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPPointsMultiplier"));
        m_PremiumBattlePass = CheckBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPPremiumBattlePass"));
        m_TierPreview = MultilineTextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPTierPreview"));
        m_SaveTier = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPSaveTier"));
        m_NewTier = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPNewTier"));
        m_TierStatus = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPTierStatus"));

        m_DonationSummary = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPDonationSummary"));
        m_DonationCurrent = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPDonationCurrent"));
        m_DonationGoal = EditBoxWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPDonationGoal"));
        m_DonationSave = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPDonationSave"));
        m_DonationReset = ButtonWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPDonationReset"));
        m_DonationStatus = TextWidget.Cast(m_RootWidget.FindAnyWidget("BDE_VIPDonationStatus"));

        if (m_PlayerList)
            WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_PlayerList, this, "OnPlayerSelected");
        if (m_TierList)
            WidgetEventHandler.GetInstance().RegisterOnMouseButtonUp(m_TierList, this, "OnTierSelected");

        ClearSelectedPlayer();
        ClearTierEditor();
        ApplyManageState();
    }

    void RequestAccess()
    {
        if (!g_Game || !g_Game.IsClient())
            return;
        GetRPCManager().SendRPC(BDE_MAPUI_VIP_RPC_SERVER, "RequestMapUIVIPAccess", NULL, true, NULL);
    }

    void OnMenuShow()
    {
        RequestData();
    }

    void OnMenuHide()
    {
    }

    void Update(float timeslice)
    {
        m_SearchPoll += timeslice;
        if (m_SearchPoll < 0.25 || !m_Search)
            return;
        m_SearchPoll = 0.0;
        string value = m_Search.GetText();
        if (value != m_LastSearch)
        {
            m_LastSearch = value;
            RenderPlayers();
        }
    }

    void ReceiveAccess(bool canView, bool canManage)
    {
        m_CanView = canView;
        m_CanManage = canManage;
        ApplyManageState();
        if (canView)
            RequestData();
    }

    void ReceiveData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param1<ref BDE_MapUI_VIPAdminSnapshot> payload;
        if (!ctx.Read(payload) || !payload.param1)
        {
            SetActionStatus("VIP data read failed. Version mismatch?");
            return;
        }

        m_Data = payload.param1;
        m_CanView = m_Data.CanView;
        m_CanManage = m_Data.CanManage;
        ApplyManageState();
        RenderTierLists();
        RenderPlayers();
        RenderDonation();
    }

    void ReceiveMessage(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client || !g_Game.IsClient())
            return;

        Param2<string, int> payload;
        if (!ctx.Read(payload))
            return;

        SetActionStatus(payload.param1);
        SetTierStatus(payload.param1);
        SetDonationStatus(payload.param1);
    }

    bool OnClick(Widget w, int x, int y, int button)
    {
        if (button != MouseState.LEFT)
            return false;

        if (w == m_FilterAll) { m_Filter = BDE_MAPUI_VIP_FILTER_ALL; ApplyFilterColors(); RenderPlayers(); return true; }
        if (w == m_FilterVIP) { m_Filter = BDE_MAPUI_VIP_FILTER_VIP; ApplyFilterColors(); RenderPlayers(); return true; }
        if (w == m_FilterNonVIP) { m_Filter = BDE_MAPUI_VIP_FILTER_NONVIP; ApplyFilterColors(); RenderPlayers(); return true; }
        if (w == m_FilterPending) { m_Filter = BDE_MAPUI_VIP_FILTER_PENDING; ApplyFilterColors(); RenderPlayers(); return true; }
        if (w == m_Refresh) { RequestData(); return true; }
        if (w == m_AssignButton) { AssignSelectedPlayer(); return true; }
        if (w == m_DisableButton) { DisableSelectedPlayer(); return true; }
        if (w == m_PendingAdd) { AddPendingPlayer(); return true; }
        if (w == m_SaveTier) { SaveTier(); return true; }
        if (w == m_NewTier) { ClearTierEditor(); return true; }
        if (w == m_DonationSave) { SaveDonation(); return true; }
        if (w == m_DonationReset) { ResetDonation(); return true; }
        return false;
    }

    void OnPlayerSelected()
    {
        if (!m_PlayerList)
            return;
        int row = m_PlayerList.GetSelectedRow();
        if (row < 0)
            return;
        BDE_MapUI_VIPPlayerAdminEntry entry;
        m_PlayerList.GetItemData(row, 0, entry);
        if (!entry)
            return;
        m_SelectedPlayer = entry;
        RenderSelectedPlayer();
    }

    void OnTierSelected()
    {
        if (!m_TierList)
            return;
        int row = m_TierList.GetSelectedRow();
        if (row < 0)
            return;
        BDE_MapUI_VIPTierData tier;
        m_TierList.GetItemData(row, 0, tier);
        if (!tier)
            return;
        m_SelectedTierData = tier;
        LoadTierEditor(tier);
    }

    private void RequestData()
    {
        if (!m_CanView)
            return;
        GetRPCManager().SendRPC(BDE_MAPUI_VIP_RPC_SERVER, "RequestMapUIVIPData", NULL, true, NULL);
    }

    private void RenderPlayers()
    {
        if (!m_PlayerList)
            return;

        m_PlayerList.ClearItems();
        m_VisiblePlayers.Clear();

        if (!m_Data || !m_Data.Players)
        {
            SetListStatus("No player data.");
            return;
        }

        string needle = "";
        if (m_Search)
            needle = m_Search.GetText();
        needle.ToLower();

        for (int i = 0; i < m_Data.Players.Count(); i++)
        {
            BDE_MapUI_VIPPlayerAdminEntry entry = m_Data.Players.Get(i);
            if (!entry || !PassesFilter(entry, needle))
                continue;

            m_VisiblePlayers.Insert(entry);
            string tierName = entry.TierName;
            if (tierName == "") tierName = "None";
            string state = "OFFLINE";
            if (entry.Pending) state = "PENDING";
            else if (entry.IsOnline) state = "ONLINE";
            if (entry.HasVIP && !entry.Enabled) state = state + " / DISABLED";

            int row = m_PlayerList.AddItem(entry.Name, entry, 0);
            m_PlayerList.SetItem(row, tierName, entry, 1);
            m_PlayerList.SetItem(row, state, entry, 2);
        }

        SetListStatus(m_VisiblePlayers.Count().ToString() + " / " + m_Data.Players.Count().ToString() + " players");
        RestorePlayerSelection();
    }

    private bool PassesFilter(BDE_MapUI_VIPPlayerAdminEntry entry, string needle)
    {
        if (m_Filter == BDE_MAPUI_VIP_FILTER_VIP && !entry.HasVIP)
            return false;
        if (m_Filter == BDE_MAPUI_VIP_FILTER_NONVIP && entry.HasVIP)
            return false;
        if (m_Filter == BDE_MAPUI_VIP_FILTER_PENDING && !entry.Pending)
            return false;

        if (needle == "")
            return true;

        string hay = entry.Name + " " + entry.UID + " " + entry.PlainID + " " + entry.TierName;
        hay.ToLower();
        return hay.Contains(needle);
    }

    private void RestorePlayerSelection()
    {
        if (!m_PlayerList)
            return;

        if (m_SelectedPlayer)
        {
            for (int i = 0; i < m_VisiblePlayers.Count(); i++)
            {
                BDE_MapUI_VIPPlayerAdminEntry entry = m_VisiblePlayers.Get(i);
                if (entry && entry.UID == m_SelectedPlayer.UID && entry.PlainID == m_SelectedPlayer.PlainID)
                {
                    m_SelectedPlayer = entry;
                    m_PlayerList.SelectRow(i);
                    RenderSelectedPlayer();
                    return;
                }
            }
        }

        if (m_VisiblePlayers.Count() > 0)
        {
            m_SelectedPlayer = m_VisiblePlayers.Get(0);
            m_PlayerList.SelectRow(0);
            RenderSelectedPlayer();
            return;
        }

        ClearSelectedPlayer();
    }

    private void RenderSelectedPlayer()
    {
        if (!m_SelectedPlayer)
        {
            ClearSelectedPlayer();
            return;
        }

        if (m_SelectedName) m_SelectedName.SetText(m_SelectedPlayer.Name);
        if (m_SelectedIDs) m_SelectedIDs.SetText("DayZ ID: " + SafeText(m_SelectedPlayer.UID) + "\nSteam64: " + SafeText(m_SelectedPlayer.PlainID));
        string state = "Offline";
        if (m_SelectedPlayer.Pending) state = "Pending Steam64 entry";
        else if (m_SelectedPlayer.IsOnline) state = "Online";
        if (m_SelectedState) m_SelectedState.SetText("Status: " + state);
        string tier = m_SelectedPlayer.TierName;
        if (tier == "") tier = "None";
        if (m_SelectedTier) m_SelectedTier.SetText("VIP Tier: " + tier);

        if (m_AssignTierList && m_Data && m_Data.Tiers)
        {
            for (int i = 0; i < m_Data.Tiers.Count(); i++)
            {
                if (m_Data.Tiers[i] && m_Data.Tiers[i].TierName == m_SelectedPlayer.TierName)
                {
                    m_AssignTierList.SelectRow(i);
                    break;
                }
            }
        }
    }

    private void ClearSelectedPlayer()
    {
        m_SelectedPlayer = null;
        if (m_SelectedName) m_SelectedName.SetText("No player selected");
        if (m_SelectedIDs) m_SelectedIDs.SetText("DayZ ID: -\nSteam64: -");
        if (m_SelectedState) m_SelectedState.SetText("Status: -");
        if (m_SelectedTier) m_SelectedTier.SetText("VIP Tier: None");
    }

    private void RenderTierLists()
    {
        if (m_AssignTierList) m_AssignTierList.ClearItems();
        if (m_TierList) m_TierList.ClearItems();
        if (!m_Data || !m_Data.Tiers)
            return;

        for (int i = 0; i < m_Data.Tiers.Count(); i++)
        {
            BDE_MapUI_VIPTierData tier = m_Data.Tiers.Get(i);
            if (!tier) continue;
            if (m_AssignTierList) m_AssignTierList.AddItem(tier.TierName, tier, 0);
            if (m_TierList) m_TierList.AddItem(tier.TierName, tier, 0);
        }

        if (m_Data.Tiers.Count() > 0 && !m_SelectedTierData)
        {
            m_SelectedTierData = m_Data.Tiers.Get(0);
            if (m_TierList) m_TierList.SelectRow(0);
            LoadTierEditor(m_SelectedTierData);
        }
    }

    private void LoadTierEditor(BDE_MapUI_VIPTierData tier)
    {
        if (!tier) return;
        if (m_TierName) m_TierName.SetText(tier.TierName);
        if (m_IncomeMultiplier) m_IncomeMultiplier.SetText(tier.PassiveIncomeMultiplier.ToString());
        if (m_RepMultiplier) m_RepMultiplier.SetText(tier.PassiveReputationMultiplier.ToString());
        if (m_PointsMultiplier) m_PointsMultiplier.SetText(tier.PointStoreMultiplier.ToString());
        if (m_PremiumBattlePass) m_PremiumBattlePass.SetChecked(tier.PremiumBattlePass == 1);
        RenderTierPreview(tier.PassiveIncomeMultiplier, tier.PassiveReputationMultiplier, tier.PointStoreMultiplier);
    }

    private void ClearTierEditor()
    {
        m_SelectedTierData = null;
        if (m_TierName) m_TierName.SetText("");
        if (m_IncomeMultiplier) m_IncomeMultiplier.SetText("1.0");
        if (m_RepMultiplier) m_RepMultiplier.SetText("1.0");
        if (m_PointsMultiplier) m_PointsMultiplier.SetText("1.0");
        if (m_PremiumBattlePass) m_PremiumBattlePass.SetChecked(false);
        RenderTierPreview(1.0, 1.0, 1.0);
        SetTierStatus("Enter a new tier name and save.");
    }

    private void RenderTierPreview(float incomeMult, float repMult, float pointsMult)
    {
        if (!m_TierPreview || !m_Data)
            return;
        int money = Math.Round(m_Data.PassiveIncomeBaseAmount * incomeMult);
        int rep = Math.Round(m_Data.PassiveReputationBaseAmount * repMult);
        m_TierPreview.SetText("Passive $$: $" + money.ToString() + " (x" + incomeMult.ToString() + ") every " + m_Data.PassiveIncomeTimerMinutes.ToString() + " min\nRP: " + rep.ToString() + " (x" + repMult.ToString() + ") every " + m_Data.PassiveReputationTimerMinutes.ToString() + " min | Points: x" + pointsMult.ToString());
    }

    private BDE_MapUI_VIPTierData GetSelectedAssignTier()
    {
        if (!m_AssignTierList)
            return null;
        int row = m_AssignTierList.GetSelectedRow();
        if (row < 0)
            return null;
        BDE_MapUI_VIPTierData tier;
        m_AssignTierList.GetItemData(row, 0, tier);
        return tier;
    }

    private void AssignSelectedPlayer()
    {
        if (!m_CanManage) { SetActionStatus("You have view-only VIP access."); return; }
        if (!m_SelectedPlayer) { SetActionStatus("Select a player first."); return; }
        BDE_MapUI_VIPTierData tier = GetSelectedAssignTier();
        if (!tier) { SetActionStatus("Select a VIP tier first."); return; }
        Param4<string, string, string, string> payload = new Param4<string, string, string, string>(m_SelectedPlayer.UID, m_SelectedPlayer.PlainID, m_SelectedPlayer.Name, tier.TierName);
        GetRPCManager().SendRPC(BDE_MAPUI_VIP_RPC_SERVER, "MapUISetPlayerVIPTier", payload, true, NULL);
    }

    private void DisableSelectedPlayer()
    {
        if (!m_CanManage) { SetActionStatus("You have view-only VIP access."); return; }
        if (!m_SelectedPlayer) { SetActionStatus("Select a player first."); return; }
        GetRPCManager().SendRPC(BDE_MAPUI_VIP_RPC_SERVER, "MapUIDisablePlayerVIP", new Param2<string, string>(m_SelectedPlayer.UID, m_SelectedPlayer.PlainID), true, NULL);
    }

    private void AddPendingPlayer()
    {
        if (!m_CanManage) { SetActionStatus("You have view-only VIP access."); return; }
        BDE_MapUI_VIPTierData tier = GetSelectedAssignTier();
        if (!tier) { SetActionStatus("Select a VIP tier first."); return; }
        string steam64 = "";
        if (m_PendingSteam64) steam64 = m_PendingSteam64.GetText();
        if (steam64 == "") { SetActionStatus("Enter a Steam64 ID."); return; }
        GetRPCManager().SendRPC(BDE_MAPUI_VIP_RPC_SERVER, "MapUIAddPendingVIP", new Param2<string, string>(steam64, tier.TierName), true, NULL);
    }

    private void SaveTier()
    {
        if (!m_CanManage) { SetTierStatus("You have view-only VIP access."); return; }
        string name = m_TierName.GetText();
        float incomeMult = m_IncomeMultiplier.GetText().ToFloat();
        float repMult = m_RepMultiplier.GetText().ToFloat();
        float pointsMult = m_PointsMultiplier.GetText().ToFloat();
        if (name == "") { SetTierStatus("Tier name cannot be empty."); return; }
        if (incomeMult < 1.0 || repMult < 1.0 || pointsMult < 1.0) { SetTierStatus("All multipliers must be 1.0 or greater."); return; }

        BDE_MapUI_VIPTierData tier = new BDE_MapUI_VIPTierData();
        tier.TierName = name;
        tier.PassiveIncomeMultiplier = incomeMult;
        tier.PassiveReputationMultiplier = repMult;
        tier.PointStoreMultiplier = pointsMult;
        if (m_PremiumBattlePass && m_PremiumBattlePass.IsChecked()) tier.PremiumBattlePass = 1;
        RenderTierPreview(incomeMult, repMult, pointsMult);
        GetRPCManager().SendRPC(BDE_MAPUI_VIP_RPC_SERVER, "MapUISaveVIPTier", new Param1<ref BDE_MapUI_VIPTierData>(tier), true, NULL);
    }

    private void RenderDonation()
    {
        if (!m_Data) return;
        string symbol = m_Data.DonationCurrencySymbol;
        if (symbol == "") symbol = "$";
        if (m_DonationSummary) m_DonationSummary.SetText(symbol + m_Data.DonationCurrentAmount.ToString() + " / " + symbol + m_Data.DonationMonthlyGoal.ToString());
        if (m_DonationCurrent) m_DonationCurrent.SetText(m_Data.DonationCurrentAmount.ToString());
        if (m_DonationGoal) m_DonationGoal.SetText(m_Data.DonationMonthlyGoal.ToString());
    }

    private void SaveDonation()
    {
        if (!m_CanManage) { SetDonationStatus("You have view-only VIP access."); return; }
        int current = m_DonationCurrent.GetText().ToInt();
        int goal = m_DonationGoal.GetText().ToInt();
        if (current < 0 || goal < 0) { SetDonationStatus("Donation values cannot be negative."); return; }
        GetRPCManager().SendRPC(BDE_MAPUI_VIP_RPC_SERVER, "MapUIUpdateDonation", new Param2<int, int>(current, goal), true, NULL);
    }

    private void ResetDonation()
    {
        if (!m_CanManage) { SetDonationStatus("You have view-only VIP access."); return; }
        GetRPCManager().SendRPC(BDE_MAPUI_VIP_RPC_SERVER, "MapUIResetDonation", NULL, true, NULL);
    }

    private void ApplyManageState()
    {
        if (m_AssignButton) m_AssignButton.Enable(m_CanManage);
        if (m_DisableButton) m_DisableButton.Enable(m_CanManage);
        if (m_PendingSteam64) m_PendingSteam64.Enable(m_CanManage);
        if (m_PendingAdd) m_PendingAdd.Enable(m_CanManage);
        if (m_TierName) m_TierName.Enable(m_CanManage);
        if (m_IncomeMultiplier) m_IncomeMultiplier.Enable(m_CanManage);
        if (m_RepMultiplier) m_RepMultiplier.Enable(m_CanManage);
        if (m_PointsMultiplier) m_PointsMultiplier.Enable(m_CanManage);
        if (m_PremiumBattlePass) m_PremiumBattlePass.Enable(m_CanManage);
        if (m_SaveTier) m_SaveTier.Enable(m_CanManage);
        if (m_NewTier) m_NewTier.Enable(m_CanManage);
        if (m_DonationCurrent) m_DonationCurrent.Enable(m_CanManage);
        if (m_DonationGoal) m_DonationGoal.Enable(m_CanManage);
        if (m_DonationSave) m_DonationSave.Enable(m_CanManage);
        if (m_DonationReset) m_DonationReset.Enable(m_CanManage);
    }

    private void ApplyFilterColors()
    {
        int active = ARGB(255, 41, 89, 122);
        int idle = ARGB(255, 31, 31, 31);

        if (m_FilterAll)
        {
            if (m_Filter == BDE_MAPUI_VIP_FILTER_ALL)
                m_FilterAll.SetColor(active);
            else
                m_FilterAll.SetColor(idle);
        }

        if (m_FilterVIP)
        {
            if (m_Filter == BDE_MAPUI_VIP_FILTER_VIP)
                m_FilterVIP.SetColor(active);
            else
                m_FilterVIP.SetColor(idle);
        }

        if (m_FilterNonVIP)
        {
            if (m_Filter == BDE_MAPUI_VIP_FILTER_NONVIP)
                m_FilterNonVIP.SetColor(active);
            else
                m_FilterNonVIP.SetColor(idle);
        }

        if (m_FilterPending)
        {
            if (m_Filter == BDE_MAPUI_VIP_FILTER_PENDING)
                m_FilterPending.SetColor(active);
            else
                m_FilterPending.SetColor(idle);
        }
    }

    private string SafeText(string value)
    {
        if (value == "") return "-";
        return value;
    }

    private void SetListStatus(string value) { if (m_ListStatus) m_ListStatus.SetText(value); }
    private void SetActionStatus(string value) { if (m_ActionStatus) m_ActionStatus.SetText(value); }
    private void SetTierStatus(string value) { if (m_TierStatus) m_TierStatus.SetText(value); }
    private void SetDonationStatus(string value) { if (m_DonationStatus) m_DonationStatus.SetText(value); }
}
