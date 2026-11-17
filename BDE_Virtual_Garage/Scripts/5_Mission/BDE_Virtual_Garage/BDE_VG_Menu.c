class BDE_VG_Menu : UIScriptedMenu
{
    protected ref BDE_VG_Module m_Module;
    protected ref array<ref BDE_VG_VehicleData> m_WorldVehicles;
    protected ref array<ref BDE_VG_VehicleData> m_StoredVehicles;
    protected ref array<ref BDE_VG_VehicleData> m_VisibleVehicles;
    protected bool m_ShowStoredVehicles;
    protected bool m_RequestPending;
    protected bool m_Subscribed;
    protected bool m_MenuControlsLocked;
    protected int m_AccessNetworkIDLow;
    protected int m_AccessNetworkIDHigh;
    protected string m_AccessName;
    protected float m_StoreRangeMeters;
    protected int m_ATMBalance;
    protected bool m_IsTraderAccess;
    protected string m_PendingAlternativeStorageKey;
    protected ref BDE_VG_ExclusivePromptData m_ExclusivePrompt;
    protected ref BDE_VG_PreviewRenderer m_PreviewRenderer;
    protected Widget m_PreviewViewport;
    protected Widget m_HeaderPanel;
    protected Widget m_LeftPanel;
    protected Widget m_CargoPanel;
    protected Widget m_CargoSeparator;
    protected Widget m_SelectedSeparator;
    protected Widget m_ConfirmBorder;
    protected TextWidget m_ListHeaderText;
    protected bool m_HudSuppressed;
    protected bool m_CargoVisible;
    protected float m_LastLayoutWidth = -1.0;
    protected float m_LastLayoutHeight = -1.0;
    protected ItemPreviewWidget m_VehicleFallbackPreview;
    protected Widget m_DifferentMapPanel;
    protected CheckBoxWidget m_DifferentMapCheckBox;
    protected TextWidget m_DifferentMapText;
    protected Widget m_RepairPanel;
    protected CheckBoxWidget m_RepairCheckBox;
    protected TextWidget m_RepairText;

    protected TextWidget m_TitleText;
    protected TextWidget m_VersionText;
    protected TextWidget m_AccessText;
    protected TextWidget m_SummaryText;
    protected TextWidget m_CountText;
    protected TextWidget m_NameText;
    protected TextWidget m_ClassText;
    protected TextWidget m_OwnerText;
    protected TextWidget m_StatusText;
    protected TextWidget m_HealthText;
    protected TextWidget m_RetrievalText;
    protected TextWidget m_CargoHeaderText;
    protected TextWidget m_FeedbackText;
    protected TextWidget m_ConfirmText;
    protected TextListboxWidget m_VehicleList;
    protected TextListboxWidget m_CargoList;
    protected ButtonWidget m_NearbyTabButton;
    protected ButtonWidget m_StoredTabButton;
    protected ButtonWidget m_RefreshButton;
    protected ButtonWidget m_ActionButton;
    protected ButtonWidget m_CloseButton;
    protected ButtonWidget m_ConfirmYesButton;
    protected ButtonWidget m_ConfirmNoButton;
    protected Widget m_ConfirmationPanel;

    void BDE_VG_Menu()
    {
        Class.CastTo(m_Module,CF_ModuleCoreManager.Get(BDE_VG_Module));
        m_WorldVehicles = new array<ref BDE_VG_VehicleData>;
        m_StoredVehicles = new array<ref BDE_VG_VehicleData>;
        m_VisibleVehicles = new array<ref BDE_VG_VehicleData>;
    }

    void ~BDE_VG_Menu()
    {
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(RequestTimedOut);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(ReassertMenuControlLock);
        Unsubscribe();
        DestroyPreview();
        SuppressPlayerHud(false);
    }

    override Widget Init()
    {
        layoutRoot = g_Game.GetWorkspace().CreateWidgets("BDE_Virtual_Garage/gui/layouts/BDE_VG2_Menu.layout");
        m_HeaderPanel = layoutRoot.FindAnyWidget("HeaderPanel");
        m_LeftPanel = layoutRoot.FindAnyWidget("LeftPanel");
        m_CargoPanel = layoutRoot.FindAnyWidget("CargoPanel");
        m_CargoSeparator = layoutRoot.FindAnyWidget("CargoSeparator");
        m_SelectedSeparator = layoutRoot.FindAnyWidget("SelectedSeparator");
        m_ConfirmBorder = layoutRoot.FindAnyWidget("ConfirmBorder");
        m_ListHeaderText = TextWidget.Cast(layoutRoot.FindAnyWidget("ListHeaderText"));
        m_TitleText = TextWidget.Cast(layoutRoot.FindAnyWidget("TitleText"));
        m_VersionText = TextWidget.Cast(layoutRoot.FindAnyWidget("VersionText"));
        m_AccessText = TextWidget.Cast(layoutRoot.FindAnyWidget("AccessText"));
        m_SummaryText = TextWidget.Cast(layoutRoot.FindAnyWidget("SummaryText"));
        m_CountText = TextWidget.Cast(layoutRoot.FindAnyWidget("CountText"));
        m_NameText = TextWidget.Cast(layoutRoot.FindAnyWidget("NameText"));
        m_ClassText = TextWidget.Cast(layoutRoot.FindAnyWidget("ClassText"));
        m_OwnerText = TextWidget.Cast(layoutRoot.FindAnyWidget("OwnerText"));
        m_StatusText = TextWidget.Cast(layoutRoot.FindAnyWidget("StatusText"));
        m_HealthText = TextWidget.Cast(layoutRoot.FindAnyWidget("HealthText"));
        m_RetrievalText = TextWidget.Cast(layoutRoot.FindAnyWidget("RetrievalText"));
        m_CargoHeaderText = TextWidget.Cast(layoutRoot.FindAnyWidget("CargoHeaderText"));
        m_FeedbackText = TextWidget.Cast(layoutRoot.FindAnyWidget("FeedbackText"));
        m_ConfirmText = TextWidget.Cast(layoutRoot.FindAnyWidget("ConfirmText"));
        m_VehicleList = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("VehicleList"));
        m_CargoList = TextListboxWidget.Cast(layoutRoot.FindAnyWidget("CargoList"));
        m_PreviewViewport = layoutRoot.FindAnyWidget("PreviewViewport");
        m_VehicleFallbackPreview = ItemPreviewWidget.Cast(layoutRoot.FindAnyWidget("VehicleFallbackPreview"));
        m_DifferentMapPanel = layoutRoot.FindAnyWidget("DifferentMapPanel");
        m_DifferentMapCheckBox = CheckBoxWidget.Cast(layoutRoot.FindAnyWidget("DifferentMapCheckBox"));
        m_DifferentMapText = TextWidget.Cast(layoutRoot.FindAnyWidget("DifferentMapText"));
        if (m_DifferentMapPanel)
            m_DifferentMapPanel.Show(false);
        m_RepairPanel = layoutRoot.FindAnyWidget("RepairPanel");
        m_RepairCheckBox = CheckBoxWidget.Cast(layoutRoot.FindAnyWidget("RepairCheckBox"));
        m_RepairText = TextWidget.Cast(layoutRoot.FindAnyWidget("RepairText"));
        m_PreviewRenderer = new BDE_VG_PreviewRenderer(m_PreviewViewport,m_VehicleFallbackPreview);
        m_NearbyTabButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("NearbyTabButton"));
        m_StoredTabButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("StoredTabButton"));
        m_RefreshButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("RefreshButton"));
        m_ActionButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ActionButton"));
        m_CloseButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("CloseButton"));
        m_ConfirmYesButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ConfirmYesButton"));
        m_ConfirmNoButton = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ConfirmNoButton"));
        m_ConfirmationPanel = layoutRoot.FindAnyWidget("ConfirmationPanel");

        if (m_ConfirmationPanel)
            m_ConfirmationPanel.Show(false);
        if (m_VersionText)
            m_VersionText.SetText("v2.10.0");
        if (m_FeedbackText)
            m_FeedbackText.SetText("Waiting for live garage data...");
        if (m_ActionButton)
            m_ActionButton.Enable(false);

        ApplyResponsiveLayout(true);
        return layoutRoot;
    }

    override void LockControls()
    {
        if (!m_MenuControlsLocked)
        {
            super.LockControls();
            m_MenuControlsLocked = true;
        }

        ApplyPlayerControlLock();
    }

    override void UnlockControls()
    {
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(ReassertMenuControlLock);
        ReleasePlayerControlLock();

        if (m_MenuControlsLocked)
        {
            super.UnlockControls();
            m_MenuControlsLocked = false;
        }
    }

    override void OnShow()
    {
        super.OnShow();
        Subscribe();
        ApplyResponsiveLayout(true);
        ApplyPlayerControlLock();
        SuppressPlayerHud(true);
        PPEffects.SetBlurMenu(0.0);

        if (!m_Module || !m_Module.GetClientAccessNetworkIDs(m_AccessNetworkIDLow,m_AccessNetworkIDHigh))
        {
            ShowResult(false,"The garage access object could not be resolved. Close the menu and interact with it again.");
            SetInteractionEnabled(false);
            return;
        }

        if (m_PreviewRenderer)
            m_PreviewRenderer.ShowEmptyHangar();
        RequestRefresh();
        SetFocus(m_VehicleList);
    }

    override void OnHide()
    {
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(RequestTimedOut);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(ReassertMenuControlLock);
        Unsubscribe();
        DestroyPreview();
        SuppressPlayerHud(false);
        PPEffects.SetBlurMenu(0.0);
        super.OnHide();
    }

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        ApplyResponsiveLayout(false);
        ApplyPlayerControlLock();
        if (m_PreviewRenderer)
            m_PreviewRenderer.Update(timeslice);
    }

    // Follows BDE MapUI: compute physical pixel rectangles from the live
    // workspace resolution. Every panel is independent; the centre has no dim
    // overlay so the local 3D scene is displayed at its normal brightness.
    protected void SetScreenRect(Widget widget, float x, float y, float width, float height)
    {
        if (!widget)
            return;
        widget.SetFlags(WidgetFlags.HEXACTSIZE | WidgetFlags.VEXACTSIZE | WidgetFlags.HEXACTPOS | WidgetFlags.VEXACTPOS);
        widget.SetScreenSize(Math.Max(width,1.0),Math.Max(height,1.0));
        widget.SetScreenPos(x,y);
    }

    protected void LayoutNamed(string widgetName, float x, float y, float width, float height)
    {
        if (!layoutRoot)
            return;
        SetScreenRect(layoutRoot.FindAnyWidget(widgetName),x,y,width,height);
    }

    protected void ApplyResponsiveLayout(bool force)
    {
        if (!layoutRoot)
            return;
        float sw;
        float sh;
        layoutRoot.GetScreenSize(sw,sh);
        if (sw < 640.0 || sh < 400.0)
            return;
        if (!force && Math.AbsFloat(m_LastLayoutWidth - sw) < 1.0 && Math.AbsFloat(m_LastLayoutHeight - sh) < 1.0)
            return;
        m_LastLayoutWidth = sw;
        m_LastLayoutHeight = sh;

        float scale = Math.Clamp(sh / 1080.0,0.63,1.6);
        float margin = 14.0 * scale;
        float gap = 12.0 * scale;
        float padding = 12.0 * scale;
        float headerX = margin;
        float headerY = margin;
        float headerWidth = sw - margin * 2.0;
        float headerHeight = 132.0 * scale;
        float sideY = headerY + headerHeight + gap;
        float sideHeight = sh - sideY - margin;
        float leftWidth = Math.Min(560.0 * scale,sw * 0.40);
        float rightWidth = Math.Min(415.0 * scale,sw * 0.29);
        float leftX = margin;
        float rightX = sw - margin - rightWidth;
        float centreX = leftX + leftWidth + gap;
        float centreWidth = rightX - gap - centreX;
        if (!m_CargoVisible)
            centreWidth = sw - margin - centreX;

        SetScreenRect(m_HeaderPanel,headerX,headerY,headerWidth,headerHeight);
        SetScreenRect(m_TitleText,headerX + padding,headerY + 3.0 * scale,headerWidth - 170.0 * scale,46.0 * scale);
        SetScreenRect(m_VersionText,headerX + headerWidth - 167.0 * scale,headerY + 6.0 * scale,102.0 * scale,35.0 * scale);
        SetScreenRect(m_CloseButton,headerX + headerWidth - 49.0 * scale,headerY + 3.0 * scale,46.0 * scale,43.0 * scale);
        SetScreenRect(m_AccessText,headerX + padding,headerY + 51.0 * scale,headerWidth - 275.0 * scale,27.0 * scale);
        SetScreenRect(m_SummaryText,headerX + padding,headerY + 77.0 * scale,headerWidth - 275.0 * scale,27.0 * scale);
        SetScreenRect(m_CountText,headerX + padding,headerY + 102.0 * scale,headerWidth - 275.0 * scale,25.0 * scale);
        SetScreenRect(m_RefreshButton,headerX + headerWidth - 246.0 * scale,headerY + 70.0 * scale,235.0 * scale,45.0 * scale);

        SetScreenRect(m_LeftPanel,leftX,sideY,leftWidth,sideHeight);
        float innerX = leftX + padding;
        float innerWidth = leftWidth - padding * 2.0;
        float tabsY = sideY + 9.0 * scale;
        float tabWidth = (innerWidth - 7.0 * scale) * 0.5;
        SetScreenRect(m_NearbyTabButton,innerX,tabsY,tabWidth,43.0 * scale);
        SetScreenRect(m_StoredTabButton,innerX + tabWidth + 7.0 * scale,tabsY,tabWidth,43.0 * scale);
        SetScreenRect(m_ListHeaderText,innerX,sideY + 56.0 * scale,innerWidth,26.0 * scale);
        float listY = sideY + 86.0 * scale;
        float listHeight = Math.Max(108.0 * scale,sideHeight - 482.0 * scale);
        SetScreenRect(m_VehicleList,innerX,listY,innerWidth,listHeight);
        float detailsTop = listY + listHeight + 16.0 * scale;
        SetScreenRect(m_SelectedSeparator,innerX,detailsTop - 8.0 * scale,innerWidth,2.0 * scale);
        SetScreenRect(m_NameText,innerX,detailsTop,innerWidth,33.0 * scale);
        SetScreenRect(m_ClassText,innerX,detailsTop + 38.0 * scale,innerWidth,26.0 * scale);
        SetScreenRect(m_OwnerText,innerX,detailsTop + 65.0 * scale,innerWidth,26.0 * scale);
        SetScreenRect(m_StatusText,innerX,detailsTop + 92.0 * scale,innerWidth,26.0 * scale);
        SetScreenRect(m_HealthText,innerX,detailsTop + 119.0 * scale,innerWidth,26.0 * scale);
        SetScreenRect(m_RetrievalText,innerX,detailsTop + 146.0 * scale,innerWidth,26.0 * scale);
        LayoutNamed("DifferentMapPanel",innerX,detailsTop + 178.0 * scale,innerWidth,34.0 * scale);
        LayoutNamed("DifferentMapCheckBox",innerX,detailsTop + 180.0 * scale,27.0 * scale,27.0 * scale);
        LayoutNamed("DifferentMapText",innerX + 34.0 * scale,detailsTop + 178.0 * scale,innerWidth - 34.0 * scale,34.0 * scale);
        LayoutNamed("RepairPanel",innerX,detailsTop + 216.0 * scale,innerWidth,39.0 * scale);
        LayoutNamed("RepairCheckBox",innerX,detailsTop + 218.0 * scale,27.0 * scale,27.0 * scale);
        LayoutNamed("RepairText",innerX + 34.0 * scale,detailsTop + 215.0 * scale,innerWidth - 34.0 * scale,39.0 * scale);
        SetScreenRect(m_ActionButton,innerX,detailsTop + 261.0 * scale,innerWidth,42.0 * scale);
        SetScreenRect(m_FeedbackText,innerX,sideY + sideHeight - 52.0 * scale,innerWidth,42.0 * scale);

        // The viewport is a transparent, pointer-aware rectangle over the world.
        // No full-screen black overlay is allowed over this area.
        SetScreenRect(m_PreviewViewport,centreX,sideY + 2.0 * scale,centreWidth,sideHeight - 4.0 * scale);
        SetScreenRect(m_VehicleFallbackPreview,centreX,sideY + 2.0 * scale,centreWidth,sideHeight - 4.0 * scale);
        SetScreenRect(m_CargoPanel,rightX,sideY,rightWidth,sideHeight);
        SetScreenRect(m_CargoHeaderText,rightX + padding,sideY + 11.0 * scale,rightWidth - padding * 2.0,36.0 * scale);
        SetScreenRect(m_CargoSeparator,rightX + padding,sideY + 52.0 * scale,rightWidth - padding * 2.0,2.0 * scale);
        SetScreenRect(m_CargoList,rightX + padding,sideY + 63.0 * scale,rightWidth - padding * 2.0,sideHeight - 77.0 * scale);
        if (m_CargoPanel)
            m_CargoPanel.Show(m_CargoVisible);

        // Confirmation overlays are the only elements permitted to dim the scene.
        SetScreenRect(m_ConfirmationPanel,0.0,0.0,sw,sh);
        float confirmW = Math.Min(720.0 * scale,sw * 0.85);
        float panelHeight = 315.0;
        if (m_ExclusivePrompt)
            panelHeight = 460.0;
        float confirmH = Math.Min(panelHeight * scale,sh * 0.8);
        float confirmX = (sw - confirmW) * 0.5;
        float confirmY = (sh - confirmH) * 0.5;
        SetScreenRect(m_ConfirmBorder,confirmX,confirmY,confirmW,confirmH);
        LayoutNamed("ConfirmTitle",confirmX + 16.0 * scale,confirmY + 14.0 * scale,confirmW - 32.0 * scale,36.0 * scale);
        SetScreenRect(m_ConfirmText,confirmX + 23.0 * scale,confirmY + 64.0 * scale,confirmW - 46.0 * scale,confirmH - 160.0 * scale);
        float buttonWidth = (confirmW - 66.0 * scale) * 0.5;
        SetScreenRect(m_ConfirmYesButton,confirmX + 22.0 * scale,confirmY + confirmH - 67.0 * scale,buttonWidth,49.0 * scale);
        SetScreenRect(m_ConfirmNoButton,confirmX + 44.0 * scale + buttonWidth,confirmY + confirmH - 67.0 * scale,buttonWidth,49.0 * scale);
    }

    // Vanilla inventory and map menus use these context flags. Clearing them
    // on close respects the player's global HUD/quickbar preferences.
    protected void SuppressPlayerHud(bool suppress)
    {
        if (!suppress && !m_HudSuppressed)
            return;
        MissionGameplay mission = MissionGameplay.Cast(g_Game.GetMission());
        if (!mission)
            return;
        Hud hud = mission.GetHud();
        if (!hud)
            return;
        hud.ShowHudUI(!suppress);
        hud.ShowQuickbarUI(!suppress);
        m_HudSuppressed = suppress;
    }

    protected void ApplyPlayerControlLock()
    {
        MissionGameplay mission = MissionGameplay.Cast(g_Game.GetMission());
        if (mission)
            mission.PlayerControlDisable(INPUT_EXCLUDE_ALL);

        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (player && player.GetInputController())
            player.GetInputController().SetDisabled(true);

        g_Game.GetUIManager().ShowUICursor(true);
    }

    protected void ReleasePlayerControlLock()
    {
        PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
        if (player && player.GetInputController())
            player.GetInputController().SetDisabled(false);

        MissionGameplay mission = MissionGameplay.Cast(g_Game.GetMission());
        if (mission)
            mission.PlayerControlEnable(false);
    }

    protected void ReassertMenuControlLock()
    {
        if (g_Game.GetUIManager().GetMenu() != this)
            return;

        g_Game.GetInput().ResetGameFocus();
        m_MenuControlsLocked = false;
        LockControls();
        SetFocus(m_VehicleList);
    }

    protected void DestroyPreviewAndKeepMenuLocked()
    {
        if (m_PreviewRenderer)
            m_PreviewRenderer.ShowEmptyHangar();
        ApplyPlayerControlLock();
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).Remove(ReassertMenuControlLock);
        g_Game.GetCallQueue(CALL_CATEGORY_GUI).CallLater(ReassertMenuControlLock,75,false);
    }

    protected void Subscribe()
    {
        if (m_Subscribed || !m_Module)
            return;

        m_Module.GetMenuInvoker().Insert(OnVehiclesReceived);
        m_Module.GetCallbackInvoker().Insert(OnCallbackReceived);
        m_Module.GetAlternatePromptInvoker().Insert(OnAlternatePromptReceived);
        m_Module.GetExclusivePromptInvoker().Insert(OnExclusivePromptReceived);
        m_Subscribed = true;
    }

    protected void Unsubscribe()
    {
        if (m_ExclusivePrompt && m_Module)
        {
            m_Module.CancelExclusiveRequest(m_ExclusivePrompt.Token);
            m_ExclusivePrompt = null;
        }
        if (!m_Subscribed || !m_Module)
            return;

        m_Module.GetMenuInvoker().Remove(OnVehiclesReceived);
        m_Module.GetCallbackInvoker().Remove(OnCallbackReceived);
        m_Module.GetAlternatePromptInvoker().Remove(OnAlternatePromptReceived);
        m_Module.GetExclusivePromptInvoker().Remove(OnExclusivePromptReceived);
        m_Subscribed = false;
    }

    void OnVehiclesReceived(int accessNetworkIDLow, int accessNetworkIDHigh, string accessName, bool isTraderAccess, float storeRangeMeters, int atmBalance, array<ref BDE_VG_VehicleData> worldVehicles, array<ref BDE_VG_VehicleData> storedVehicles)
    {
        m_AccessNetworkIDLow = accessNetworkIDLow;
        m_AccessNetworkIDHigh = accessNetworkIDHigh;
        m_AccessName = accessName;
        m_IsTraderAccess = isTraderAccess;
        m_StoreRangeMeters = storeRangeMeters;
        m_ATMBalance = atmBalance;
        m_WorldVehicles = worldVehicles;
        m_StoredVehicles = storedVehicles;

        if (!m_WorldVehicles)
            m_WorldVehicles = new array<ref BDE_VG_VehicleData>;
        if (!m_StoredVehicles)
            m_StoredVehicles = new array<ref BDE_VG_VehicleData>;

        EndRequest();
        if (m_AccessText)
            m_AccessText.SetText("STORE RANGE  " + Math.Round(m_StoreRangeMeters).ToString() + " m");
        if (m_SummaryText)
            m_SummaryText.SetText("BDE VEHICLE LOCKS OWNERSHIP  |  LIVE EXPANSION GROUP ACCESS");
        RenderList();
        ShowResult(true,"Garage data loaded. Select a vehicle to continue.");
    }

    void OnCallbackReceived(int callback, bool success, string message)
    {
        if (!success)
        {
            EndRequest();
            ShowResult(false,message);
            return;
        }

        ShowResult(true,message);
        if (callback == BDE_VG_CALLBACK_STORED || callback == BDE_VG_CALLBACK_RETRIEVED)
            return;

        EndRequest();
    }

    void OnAlternatePromptReceived(BDE_VG_AlternatePromptData promptData)
    {
        if (!promptData)
            return;

        EndRequest();
        m_PendingAlternativeStorageKey = promptData.m_StorageKey;
        string message = "ORIGINAL POSITION BLOCKED\n\nBlocking object: " + promptData.m_BlockingObjectName + "\nSafe position found: " + Math.Round(promptData.m_AlternativeDistanceMeters).ToString() + " m from the original location.\n\nRetrieve the vehicle at the alternate position?";
        if (m_ConfirmText)
            m_ConfirmText.SetText(message);
        if (m_ConfirmationPanel)
            m_ConfirmationPanel.Show(true);
        SetFocus(m_ConfirmYesButton);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (m_ExclusivePrompt && w != m_ConfirmYesButton && w != m_ConfirmNoButton && w != m_CloseButton)
            return true;
        if (w == m_DifferentMapCheckBox && m_VehicleList)
        {
            int selectedRow = m_VehicleList.GetSelectedRow();
            if (selectedRow >= 0 && selectedRow < m_VisibleVehicles.Count())
                UpdateRetrievalText(m_VisibleVehicles[selectedRow]);
        }
        if (w == m_NearbyTabButton)
        {
            m_ShowStoredVehicles = false;
            RenderList();
            return true;
        }
        if (w == m_StoredTabButton)
        {
            m_ShowStoredVehicles = true;
            RenderList();
            return true;
        }
        if (w == m_RefreshButton)
        {
            RequestRefresh();
            return true;
        }
        if (w == m_ActionButton)
        {
            ExecuteSelectedAction();
            return true;
        }
        if (w == m_CloseButton)
        {
            Close();
            return true;
        }
        if (w == m_ConfirmYesButton)
        {
            if (m_ExclusivePrompt)
                ConfirmExclusiveStore();
            else
                ConfirmAlternativeSpawn();
            return true;
        }
        if (w == m_ConfirmNoButton)
        {
            if (m_ExclusivePrompt)
            {
                CancelExclusiveStore();
                return true;
            }
            HideConfirmation();
            ShowResult(false,"Vehicle retrieval was cancelled.");
            return true;
        }

        return super.OnClick(w,x,y,button);
    }

    override bool OnKeyDown(Widget w, int x, int y, int key)
    {
        if (key == KeyCode.KC_ESCAPE)
        {
            if (m_ExclusivePrompt)
            {
                CancelExclusiveStore();
                return true;
            }
            Close();
            return true;
        }

        return super.OnKeyDown(w,x,y,key);
    }

    override bool OnItemSelected(Widget w, int x, int y, int row, int column, int oldRow, int oldColumn)
    {
        if (w == m_VehicleList)
        {
            if (m_ExclusivePrompt)
                return true;
            UpdateSelection(row);
            return true;
        }

        return super.OnItemSelected(w,x,y,row,column,oldRow,oldColumn);
    }

    override bool OnMouseWheel(Widget w, int x, int y, int wheel)
    {
        if (m_PreviewRenderer && m_PreviewRenderer.OwnsWidget(w))
        {
            m_PreviewRenderer.Rotate(wheel * 10.0);
            return true;
        }

        return super.OnMouseWheel(w,x,y,wheel);
    }

    protected void RenderList()
    {
        if (!m_VehicleList)
            return;

        m_VehicleList.ClearItems();
        m_VisibleVehicles.Clear();
        array<ref BDE_VG_VehicleData> sourceVehicles = m_WorldVehicles;
        if (m_ShowStoredVehicles)
            sourceVehicles = m_StoredVehicles;

        if (sourceVehicles)
        {
            foreach (BDE_VG_VehicleData vehicleData : sourceVehicles)
            {
                if (!vehicleData)
                    continue;

                m_VisibleVehicles.Insert(vehicleData);
                string rowText = vehicleData.m_DisplayName;
                rowText.Replace(" (Locked)", "");
                if (!vehicleData.m_IsStored)
                    rowText = rowText + "  |  " + Math.Round(vehicleData.m_DistanceMeters).ToString() + " m";
                m_VehicleList.AddItem(rowText,null,0);
            }
        }

        string countLabel = "NEARBY VEHICLES: ";
        if (m_ShowStoredVehicles)
            countLabel = "STORED VEHICLES: ";
        if (m_CountText)
            m_CountText.SetText(countLabel + m_VisibleVehicles.Count().ToString());
        if (m_ActionButton)
        {
            if (m_ShowStoredVehicles)
                m_ActionButton.SetText("RETRIEVE SELECTED VEHICLE");
            else
                m_ActionButton.SetText("STORE SELECTED VEHICLE");
        }

        if (m_DifferentMapPanel)
            m_DifferentMapPanel.Show(m_IsTraderAccess && !m_ShowStoredVehicles);
        if (m_ListHeaderText)
        {
            if (m_ShowStoredVehicles)
                m_ListHeaderText.SetText("STORED VEHICLES");
            else
                m_ListHeaderText.SetText("NEARBY VEHICLES");
        }
        if (m_DifferentMapCheckBox)
            m_DifferentMapCheckBox.SetChecked(false);
        if (m_RepairPanel)
            m_RepairPanel.Show(!m_ShowStoredVehicles);
        if (m_RepairCheckBox)
            m_RepairCheckBox.SetChecked(false);
        UpdateTabStyles();
        if (m_VisibleVehicles.Count() > 0)
        {
            m_VehicleList.SelectRow(0);
            UpdateSelection(0);
        }
        else
        {
            ClearSelection();
        }
    }

    protected void UpdateSelection(int row)
    {
        if (row < 0 || row >= m_VisibleVehicles.Count())
        {
            ClearSelection();
            return;
        }

        BDE_VG_VehicleData vehicleData = m_VisibleVehicles[row];
        if (m_NameText)
            m_NameText.SetText(vehicleData.m_DisplayName);
        if (m_ClassText)
            m_ClassText.SetText("CLASS  " + vehicleData.m_ClassName);
        if (m_OwnerText)
            m_OwnerText.SetText("OWNER  " + vehicleData.m_OwnerName);
        if (m_StatusText)
            m_StatusText.SetText("STATUS  " + vehicleData.m_StatusText);
        if (m_HealthText)
            m_HealthText.SetText("HEALTH  " + Math.Round(vehicleData.m_HealthPercent).ToString() + "%");

        PopulateCargoList(vehicleData);
        UpdateDifferentMapControls(vehicleData);
        UpdateRetrievalText(vehicleData);
        UpdateRepairControls(vehicleData);

        bool actionEnabled = !m_RequestPending;
        if (!vehicleData.m_IsStored && vehicleData.m_StatusText != "Ready to store")
            actionEnabled = false;
        if (m_ActionButton)
            m_ActionButton.Enable(actionEnabled);
        CreatePreview(vehicleData);
    }

    protected void ClearSelection()
    {
        if (m_NameText)
            m_NameText.SetText("NO VEHICLE SELECTED");
        if (m_ClassText)
            m_ClassText.SetText("");
        if (m_OwnerText)
            m_OwnerText.SetText("");
        if (m_StatusText)
            m_StatusText.SetText("");
        if (m_HealthText)
            m_HealthText.SetText("");
        if (m_RetrievalText)
            m_RetrievalText.SetText("");
        if (m_CargoHeaderText)
            m_CargoHeaderText.SetText("VEHICLE CARGO");
        if (m_CargoList)
            m_CargoList.ClearItems();
        SetCargoVisibility(false);
        if (m_DifferentMapPanel)
            m_DifferentMapPanel.Show(false);
        if (m_DifferentMapCheckBox)
        {
            m_DifferentMapCheckBox.SetChecked(false);
            m_DifferentMapCheckBox.Enable(false);
        }
        if (m_RepairCheckBox)
        {
            m_RepairCheckBox.SetChecked(false);
            m_RepairCheckBox.Enable(false);
        }
        if (m_RepairText)
            m_RepairText.SetText("");
        if (m_ActionButton)
            m_ActionButton.Enable(false);
        DestroyPreviewAndKeepMenuLocked();
    }

    protected void UpdateRetrievalText(BDE_VG_VehicleData vehicleData)
    {
        if (!m_RetrievalText || !vehicleData)
            return;
        bool allServers = vehicleData.m_StoreForDifferentMap;
        if (!vehicleData.m_IsStored)
            allServers = m_IsTraderAccess && m_DifferentMapCheckBox && m_DifferentMapCheckBox.IsChecked();
        string retrieval = "This Server Only";
        if (allServers)
            retrieval = "All Servers";
        m_RetrievalText.SetText("RETRIEVAL  " + retrieval);
    }

    protected void UpdateDifferentMapControls(BDE_VG_VehicleData vehicleData)
    {
        if (!m_DifferentMapPanel || !m_DifferentMapCheckBox)
            return;

        bool showOption = m_IsTraderAccess && vehicleData && !vehicleData.m_IsStored;
        m_DifferentMapPanel.Show(showOption);
        m_DifferentMapCheckBox.SetChecked(false);
        m_DifferentMapCheckBox.Enable(showOption && !m_RequestPending);
        if (m_DifferentMapText)
            m_DifferentMapText.SetText("Store for Retrieval on Different Map");
    }

    protected void UpdateRepairControls(BDE_VG_VehicleData vehicleData)
    {
        if (!m_RepairPanel || !m_RepairCheckBox || !m_RepairText)
            return;

        bool showRepair = vehicleData && !vehicleData.m_IsStored;
        m_RepairPanel.Show(showRepair);
        m_RepairCheckBox.SetChecked(false);
        if (!showRepair)
            return;

        m_RepairCheckBox.Enable(vehicleData.m_RepairAvailable && !m_RequestPending);
        if (vehicleData.m_RepairAvailable)
        {
            m_RepairText.SetText("REPAIR VEHICLE ON STORE  |  COST " + ExpansionStatic.FormatInt(vehicleData.m_RepairCost) + "  |  ATM " + ExpansionStatic.FormatInt(m_ATMBalance));
            m_RepairText.SetColor(ARGB(255,105,210,135));
        }
        else
        {
            string reason = vehicleData.m_RepairUnavailableReason;
            if (reason == string.Empty)
                reason = "Repair unavailable.";

            string unavailableText = "REPAIR UNAVAILABLE";
            if (vehicleData.m_RepairCost > 0)
                unavailableText = unavailableText + "  |  COST " + ExpansionStatic.FormatInt(vehicleData.m_RepairCost) + "  |  ATM " + ExpansionStatic.FormatInt(m_ATMBalance);
            unavailableText = unavailableText + "  |  " + reason;
            m_RepairText.SetText(unavailableText);
            m_RepairText.SetColor(ARGB(255,220,105,105));
        }
    }

    protected void SetCargoVisibility(bool visible)
    {
        if (m_CargoVisible == visible)
            return;
        m_CargoVisible = visible;
        if (m_CargoPanel)
            m_CargoPanel.Show(visible);
        ApplyResponsiveLayout(true);
    }

    protected void PopulateCargoList(BDE_VG_VehicleData vehicleData)
    {
        if (!m_CargoList)
            return;

        m_CargoList.ClearItems();
        bool hasCargo = vehicleData && vehicleData.m_InventorySnapshotVersion > 0 && vehicleData.m_CargoItemCount > 0 && vehicleData.m_CargoEntries && vehicleData.m_CargoEntries.Count() > 0;
        SetCargoVisibility(hasCargo);
        if (!hasCargo)
            return;

        if (m_CargoHeaderText)
        {
            if (vehicleData.m_InventorySnapshotVersion > 0)
                m_CargoHeaderText.SetText("VEHICLE CARGO  (" + vehicleData.m_CargoItemCount.ToString() + ")");
            else
                m_CargoHeaderText.SetText("VEHICLE CARGO");
        }

        if (vehicleData.m_InventorySnapshotVersion <= 0)
        {
            m_CargoList.AddItem("Cargo index unavailable for this older stored entry.",null,0);
            m_CargoList.AddItem("Retrieve and store it again to index its contents.",null,0);
            return;
        }

        if (!vehicleData.m_CargoEntries || vehicleData.m_CargoEntries.Count() == 0)
        {
            m_CargoList.AddItem("No cargo items.",null,0);
            return;
        }

        foreach (BDE_VG_CargoEntry cargoEntry : vehicleData.m_CargoEntries)
        {
            if (!cargoEntry)
                continue;

            string rowText = cargoEntry.m_Count.ToString() + "x  " + cargoEntry.m_DisplayName;
            if (cargoEntry.m_DisplayName != cargoEntry.m_ClassName)
                rowText = rowText + "  [" + cargoEntry.m_ClassName + "]";
            m_CargoList.AddItem(rowText,null,0);
        }
    }

    protected void ExecuteSelectedAction()
    {
        if (m_RequestPending || !m_Module || !m_VehicleList)
            return;

        int row = m_VehicleList.GetSelectedRow();
        if (row < 0 || row >= m_VisibleVehicles.Count())
            return;

        BDE_VG_VehicleData vehicleData = m_VisibleVehicles[row];
        bool repairVehicle = false;
        bool storeForDifferentMap = false;
        if (!vehicleData.m_IsStored && m_RepairCheckBox && m_RepairCheckBox.IsChecked())
            repairVehicle = true;
        if (!vehicleData.m_IsStored && m_IsTraderAccess && m_DifferentMapCheckBox && m_DifferentMapCheckBox.IsChecked())
            storeForDifferentMap = true;

        BeginRequest("Processing " + vehicleData.m_DisplayName + "...");
        if (vehicleData.m_IsStored)
        {
            m_Module.RetrieveVehicleRequest(m_AccessNetworkIDLow,m_AccessNetworkIDHigh,vehicleData.GetStorageKey(),false);
        }
        else
        {
            DestroyPreviewAndKeepMenuLocked();
            m_Module.StoreVehicleRequest(m_AccessNetworkIDLow,m_AccessNetworkIDHigh,vehicleData.m_NetworkIDLow,vehicleData.m_NetworkIDHigh,repairVehicle,storeForDifferentMap);
        }
    }

    protected void RequestRefresh()
    {
        if (m_RequestPending || !m_Module)
            return;

        BeginRequest("Refreshing live garage data...");
        m_Module.RequestRefresh(m_AccessNetworkIDLow,m_AccessNetworkIDHigh);
    }

    void OnExclusivePromptReceived(BDE_VG_ExclusivePromptData prompt)
    {
        if (!prompt || prompt.AccessLow != m_AccessNetworkIDLow || prompt.AccessHigh != m_AccessNetworkIDHigh)
            return;
        EndRequest();
        HideConfirmation();
        m_ExclusivePrompt = prompt;
        m_CargoList.ClearItems();
        SetCargoVisibility(true);
        m_CargoHeaderText.SetText("RESTRICTED ITEMS");
        foreach (string row : prompt.Rows)
            m_CargoList.AddItem(row,null,0);
        TextWidget title = TextWidget.Cast(layoutRoot.FindAnyWidget("ConfirmTitle"));
        if (title)
            title.SetText("SERVER-EXCLUSIVE ITEMS");
        m_ConfirmText.SetText("This vehicle contains items exclusive to this server.\n\nIf you continue, those items will be moved into a temporary storage container at your player's position before the vehicle is stored.\n\nReview the Restricted Items list on the right.\n\nWould you like to store this vehicle?");
        m_ConfirmYesButton.SetText("CONFIRM STORE");
        m_ConfirmNoButton.SetText("CANCEL");
        // Keep the restricted-items list readable behind the modal shade.
        ApplyResponsiveLayout(true);
        m_ConfirmationPanel.SetColor(ARGB(90,0,0,0));
        m_ConfirmationPanel.Show(true);
        SetFocus(m_ConfirmYesButton);
    }

    protected void ConfirmExclusiveStore()
    {
        ref BDE_VG_ExclusivePromptData prompt = m_ExclusivePrompt;
        if (!prompt || !m_Module)
            return;
        m_ExclusivePrompt = null; // Do not send Cancel for the approved token.
        HideConfirmation();
        BeginRequest("Rechecking restricted items and storing the vehicle...");
        DestroyPreviewAndKeepMenuLocked();
        m_Module.StoreVehicleRequest(prompt.AccessLow,prompt.AccessHigh,prompt.VehicleLow,prompt.VehicleHigh,prompt.Repair,true,prompt.Token);
    }

    protected void CancelExclusiveStore()
    {
        HideConfirmation();
        if (m_VehicleList)
            UpdateSelection(m_VehicleList.GetSelectedRow());
        ShowResult(true,"Storage cancelled. The vehicle and its items were not changed.");
    }

    protected void ConfirmAlternativeSpawn()
    {
        if (m_PendingAlternativeStorageKey == string.Empty || !m_Module)
            return;

        string storageKey = m_PendingAlternativeStorageKey;
        HideConfirmation();
        BeginRequest("Revalidating and restoring at the alternate position...");
        m_Module.RetrieveVehicleRequest(m_AccessNetworkIDLow,m_AccessNetworkIDHigh,storageKey,true);
    }

    protected void HideConfirmation()
    {
        if (m_ExclusivePrompt && m_Module)
            m_Module.CancelExclusiveRequest(m_ExclusivePrompt.Token);
        m_ExclusivePrompt = null;
        TextWidget title = TextWidget.Cast(layoutRoot.FindAnyWidget("ConfirmTitle"));
        if (title)
            title.SetText("ORIGINAL POSITION BLOCKED");
        if (m_ConfirmYesButton)
            m_ConfirmYesButton.SetText("RETRIEVE AT SAFE POSITION");
        if (m_ConfirmationPanel)
            m_ConfirmationPanel.SetColor(ARGB(194,0,0,0));
        m_PendingAlternativeStorageKey = string.Empty;
        if (m_ConfirmationPanel)
            m_ConfirmationPanel.Show(false);
        SetFocus(m_VehicleList);
    }

    protected void BeginRequest(string message)
    {
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(RequestTimedOut);
        m_RequestPending = true;
        SetInteractionEnabled(false);
        ShowResult(true,message);
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RequestTimedOut,30000,false);
    }

    protected void EndRequest()
    {
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(RequestTimedOut);
        m_RequestPending = false;
        SetInteractionEnabled(true);
    }

    protected void RequestTimedOut()
    {
        if (!m_RequestPending)
            return;

        EndRequest();
        ShowResult(false,"The garage request timed out. Refresh the data and try again.");
    }

    protected void SetInteractionEnabled(bool enabled)
    {
        if (m_NearbyTabButton)
            m_NearbyTabButton.Enable(enabled);
        if (m_StoredTabButton)
            m_StoredTabButton.Enable(enabled);
        if (m_RefreshButton)
            m_RefreshButton.Enable(enabled);
        if (m_VehicleList)
            m_VehicleList.Enable(enabled);
        if (m_RepairCheckBox && !enabled)
            m_RepairCheckBox.Enable(false);
        if (m_DifferentMapCheckBox && !enabled)
            m_DifferentMapCheckBox.Enable(false);

        if (!enabled)
        {
            if (m_ActionButton)
                m_ActionButton.Enable(false);
            return;
        }

        if (m_VehicleList)
            UpdateSelection(m_VehicleList.GetSelectedRow());
    }

    protected void ShowResult(bool success, string message)
    {
        if (!m_FeedbackText)
            return;

        m_FeedbackText.SetText(message);
        if (success)
            m_FeedbackText.SetColor(ARGB(255,80,205,120));
        else
            m_FeedbackText.SetColor(ARGB(255,235,90,90));
    }

    protected void UpdateTabStyles()
    {
        int activeColor = ARGB(255,11,80,126);
        int inactiveColor = ARGB(255,3,5,8);
        if (m_ShowStoredVehicles)
        {
            if (m_NearbyTabButton)
                m_NearbyTabButton.SetColor(inactiveColor);
            if (m_StoredTabButton)
                m_StoredTabButton.SetColor(activeColor);
        }
        else
        {
            if (m_NearbyTabButton)
                m_NearbyTabButton.SetColor(activeColor);
            if (m_StoredTabButton)
                m_StoredTabButton.SetColor(inactiveColor);
        }
    }

    protected void CreatePreview(BDE_VG_VehicleData vehicleData)
    {
        if (!m_Module || !m_PreviewRenderer || !vehicleData)
            return;
        m_PreviewRenderer.ShowVehicle(vehicleData);
    }

    protected void DestroyPreview()
    {
        if (m_PreviewRenderer)
            m_PreviewRenderer.Clear();
    }
};
