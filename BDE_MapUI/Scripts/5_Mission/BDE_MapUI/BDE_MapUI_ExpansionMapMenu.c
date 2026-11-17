modded class ExpansionMapMenu
{
    protected bool m_BDE_TextInputLocked;
    protected Widget m_BDE_ReportsHost;
    protected int m_BDE_TicketUnread;
    protected bool m_BDE_TicketAdmin;
    protected Widget m_BDE_GroupsHost;
    protected Widget m_BDE_InfoTabs;
    protected ref array<ref BDE_MapUI_TabConfig> m_BDE_InfoPages;
    protected ref BDE_MapUI_ReportsDisplay m_BDE_ReportsDisplay;
    protected ref BDE_MapUI_GroupsDisplay m_BDE_GroupsDisplay;
    protected bool m_BDE_PortalStatusRequested;
    protected bool m_BDE_AutoChangeHandled;
    protected int m_BDE_ChangelogRevision;
    protected Widget m_BDE_Root;
    protected Widget m_BDE_ContentPanel;
    protected Widget m_BDE_MapMarkerLayer;
    protected Widget m_BDE_StaticScroll;
    protected Widget m_BDE_BountyHost;
    protected Widget m_BDE_LeaderboardHost;
    protected Widget m_BDE_PointsHost;
    protected Widget m_BDE_CraftingHost;
    protected Widget m_BDE_PlayerHost;
    protected Widget m_BDE_VIPHost;
    protected ref BDE_MapUI_PlayerInfoDisplay m_BDE_PlayerDisplay;
    protected ref BDE_MapUI_VIPAdminDisplay m_BDE_VIPDisplay;
    protected bool m_BDE_VIPCanView;
    protected bool m_BDE_VIPCanManage;
    protected ref array<int> m_BDE_TabIndices;
    protected int m_BDE_TextFitPasses;
    protected float m_BDE_PreviewDiagnosticDelay;
    protected bool m_BDE_PreviewMode;
    protected float m_BDE_PlayerRefreshElapsed;

    protected TextWidget m_BDE_ContentTitle;
    protected RichTextWidget m_BDE_ContentText;
    protected ButtonWidget m_BDE_ContentClose;

    protected Widget m_BDE_CurrencyPanel;
    protected TextWidget m_BDE_BankMoney;
    protected TextWidget m_BDE_InventoryMoney;

    protected Widget m_BDE_DonationPanel;
    protected ButtonWidget m_BDE_DonationAdminButton;
    protected MultilineTextWidget m_BDE_DonationMessage;
    protected TextWidget m_BDE_DonationAmount;
    protected ProgressBarWidget m_BDE_DonationProgress;

    protected ref array<ButtonWidget> m_BDE_TabButtons;
    protected ref array<ButtonWidget> m_BDE_LinkButtons;

    protected ref BDE_MapUI_BountyDisplay m_BDE_BountyDisplay;
    protected ref BDE_MapUI_LeaderboardDisplay m_BDE_LeaderboardDisplay;
    protected ref BDE_MapUI_PointsStoreDisplay m_BDE_PointsDisplay;
    protected ref BDE_MapUI_CraftingDisplay m_BDE_CraftingDisplay;

    protected int m_BDE_ActiveTab = -1;
    protected string m_BDE_ActiveType = "";
    protected bool m_BDE_LayoutApplied;
    protected float m_BDE_LayoutWidth;
    protected float m_BDE_LayoutHeight;
    protected float m_BDE_LayoutX;
    protected float m_BDE_LayoutY;
    protected float m_BDE_MarkerFitElapsed;
    protected bool m_BDE_FeaturesInitialised;
    protected float m_BDE_SidebarRefreshElapsed;

    override Widget Init()
    {
        Widget root = super.Init();

        if (!m_BDE_Root && root)
        {
            layoutRoot.SetSort(1);

            // Keep feature tabs outside the native map/window render branch.
            m_BDE_Root = g_Game.GetWorkspace().CreateWidgets("BDE_MapUI/GUI/layouts/BDE_MapUI_Overlay.layout", root);
            BDE_MapUI_BindWidgets();
            BDE_MapUI_ApplyLayout();
            BDE_MapUI_InitFeatureDisplays();
            BDE_MapUI_RefreshConfig();

            // Request again on each map-open so a late client RPC registration or
            // reconnect cannot leave the menu stuck on client-side defaults.
            BDE_MapUI_Module.Get().RequestConfig();
            BDE_MapUI_Module.Get().RequestSidebarData();
            m_BDE_SidebarRefreshElapsed = 0.0;
        }

        return root;
    }

    protected void BDE_MapUI_BindWidgets()
    {
        if (!m_BDE_Root)
            return;

        m_BDE_ReportsHost = m_BDE_Root.FindAnyWidget("BDE_ReportsHost");
        m_BDE_GroupsHost = m_BDE_Root.FindAnyWidget("BDE_GroupsHost");
        m_BDE_InfoTabs = m_BDE_Root.FindAnyWidget("BDE_InfoTabs");
        m_BDE_ContentPanel = m_BDE_Root.FindAnyWidget("BDE_ContentPanel");
        m_BDE_StaticScroll = m_BDE_Root.FindAnyWidget("BDE_StaticScroll");
        m_BDE_BountyHost = m_BDE_Root.FindAnyWidget("Tab_0");
        m_BDE_LeaderboardHost = m_BDE_Root.FindAnyWidget("Tab_2");
        m_BDE_PointsHost = m_BDE_Root.FindAnyWidget("Tab_3");
        m_BDE_CraftingHost = m_BDE_Root.FindAnyWidget("Tab_4");
        m_BDE_PlayerHost = m_BDE_Root.FindAnyWidget("Tab_5");
        m_BDE_VIPHost = m_BDE_Root.FindAnyWidget("Tab_VIP");

        m_BDE_ContentTitle = TextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_ContentTitle"));
        m_BDE_ContentText = RichTextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_ContentText"));
        m_BDE_ContentClose = ButtonWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_ContentClose"));

        m_BDE_CurrencyPanel = m_BDE_Root.FindAnyWidget("BDE_CurrencyPanel");
        m_BDE_BankMoney = TextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_BankMoney"));
        m_BDE_InventoryMoney = TextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_InventoryMoney"));

        m_BDE_DonationPanel = m_BDE_Root.FindAnyWidget("BDE_DonationPanel");
        m_BDE_DonationAdminButton = ButtonWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_DonationAdminButton"));
        m_BDE_DonationMessage = MultilineTextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_DonationMessage"));
        m_BDE_DonationAmount = TextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_DonationAmount"));
        m_BDE_DonationProgress = ProgressBarWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_DonationProgress"));

        m_BDE_TabButtons = new array<ButtonWidget>();
        m_BDE_TabIndices = new array<int>();
        m_BDE_LinkButtons = new array<ButtonWidget>();

        for (int i = 0; i < 10; i++)
        {
            ButtonWidget tabButton = ButtonWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_Tab" + i.ToString()));
            m_BDE_TabButtons.Insert(tabButton);
            m_BDE_TabIndices.Insert(-1);
        }

        for (int j = 0; j < 3; j++)
        {
            ButtonWidget linkButton = ButtonWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_Link" + j.ToString()));
            m_BDE_LinkButtons.Insert(linkButton);
        }
    }

    protected void BDE_MapUI_SetScreenRect(Widget widget, float x, float y, float width, float height, bool relativeToRoot = true)
    {
        if (!widget)
        {
            return;
        }

        // Screen APIs account for UI scaling and the widget's alignment flags.
        widget.SetScreenSize(width, height);
        float originX = 0;
        float originY = 0;
        if (relativeToRoot)
            layoutRoot.GetScreenPos(originX, originY);
        widget.SetScreenPos(originX + x, originY + y);
    }

    protected void BDE_MapUI_ApplyLayout()
    {
        if (!layoutRoot || !m_BDE_Root)
        {
            return;
        }

        float screenW, screenH;
        layoutRoot.GetScreenSize(screenW, screenH);
        if (screenW < 1 || screenH < 1)
        {
            return;
        }

        float rootX, rootY;
        layoutRoot.GetScreenPos(rootX, rootY);
        if (m_BDE_LayoutApplied && Math.AbsFloat(screenW - m_BDE_LayoutWidth) < 1 && Math.AbsFloat(screenH - m_BDE_LayoutHeight) < 1 && Math.AbsFloat(rootX - m_BDE_LayoutX) < 1 && Math.AbsFloat(rootY - m_BDE_LayoutY) < 1)
            return;

        m_BDE_LayoutWidth = screenW;
        m_BDE_LayoutHeight = screenH;
        m_BDE_LayoutX = rootX;
        m_BDE_LayoutY = rootY;
        // Scale with viewport height, with a width cap for narrow displays.
        float scale = Math.Clamp(screenH / 1080.0, 0.65, 1.5);
        float sidebarWidth = Math.Min(394 * scale, screenW * 0.32);
        float padding = 14 * scale;
        float innerWidth = sidebarWidth - 2 * padding;
        float topHeight = 60 * scale;
        float contentX = sidebarWidth + 16 * scale;
        BDE_MapUI_SetScreenRect(m_BDE_CurrencyPanel, padding, 12 * scale, innerWidth, 66 * scale);
        BDE_MapUI_SetScreenRect(m_BDE_DonationPanel, padding, 86 * scale, innerWidth, 112 * scale);
        BDE_MapUI_SetScreenRect(m_BDE_DonationAdminButton, padding, 86 * scale, innerWidth, 112 * scale);
        BDE_MapUI_SetScreenRect(m_BDE_Root.FindAnyWidget("BDE_LinkBar"), padding, 202 * scale, innerWidth, 52 * scale);
        BDE_MapUI_SetScreenRect(m_MapWidget, sidebarWidth, topHeight, screenW - sidebarWidth, screenH - topHeight);
        BDE_MapUI_SetScreenRect(m_BDE_Root.FindAnyWidget("BDE_TopBackdrop"), sidebarWidth, 0, screenW - sidebarWidth, topHeight);
        BDE_MapUI_SetScreenRect(m_BDE_Root.FindAnyWidget("BDE_LeftBackdrop"), 0, 0, sidebarWidth, screenH);
        BDE_MapUI_SetScreenRect(m_BDE_Root.FindAnyWidget("BDE_TopTabsGrid"), contentX, 12 * scale, screenW - contentX - 16 * scale, 36 * scale);
        BDE_MapUI_SetScreenRect(m_BDE_ContentPanel, contentX, 74 * scale, screenW - contentX - 16 * scale, screenH - 94 * scale);

        Widget sideTabs = m_BDE_Root.FindAnyWidget("BDE_SideTabsGrid");
        BDE_MapUI_SetScreenRect(sideTabs, padding, screenH - 162 * scale, innerWidth, 112 * scale);
        if (sideTabs)
        {
            sideTabs.SetSort(3);
        }

        // Detach the actual panel from Expansion's bottom-anchored, size-to-
        // content wrap spacer. Its cached controls and event handler stay live.
        Widget markerRoot = layoutRoot.FindAnyWidget("ExpansionMapUIMarkerList");
        Widget panel = layoutRoot.FindAnyWidget("MarkerListPanel");
        float markerTop = 262 * scale;
        float markerHeight = Math.Max(64 * scale, screenH - markerTop - 178 * scale);
        if (panel)
        {
            if (panel.GetParent() != m_BDE_Root)
                m_BDE_Root.AddChild(panel);
            panel.SetHandler(m_MarkerList);
            panel.Show(true);
            panel.SetSort(1);
            panel.SetFlags(WidgetFlags.CLIPCHILDREN);
            BDE_MapUI_SetScreenRect(panel, padding, markerTop, innerWidth, markerHeight);
            panel.SetColor(ARGB(240, 1, 5, 10));
        }

        Widget spacer = layoutRoot.FindAnyWidget("MarkerListSpacer");
        BDE_MapUI_SetScreenRect(spacer, padding, markerTop, innerWidth, markerHeight);
        if (spacer)
        {
            spacer.SetColor(ARGB(242, 2, 8, 14));
        }
        Widget header = layoutRoot.FindAnyWidget("MarkerListPanelHeader");
        BDE_MapUI_SetScreenRect(header, padding, markerTop, innerWidth, 32 * scale);
        if (header)
        {
            header.SetColor(ARGB(255, 3, 11, 20));
        }
        BDE_MapUI_SetScreenRect(layoutRoot.FindAnyWidget("MarkerListPanelScroller"), padding, markerTop + 36 * scale, innerWidth, markerHeight - 36 * scale);

        Widget controls = m_BDE_Root.FindAnyWidget("BDE_SidebarControls");
        Widget toggleHost = m_BDE_Root.FindAnyWidget("BDE_MarkerToggleHost");
        BDE_MapUI_SetScreenRect(controls, 0, 0, sidebarWidth, screenH);
        Widget toggle = layoutRoot.FindAnyWidget("MarkerListShow");
        if (toggleHost && toggle)
        {
            if (toggle.GetParent() != toggleHost)
                toggleHost.AddChild(toggle);
            toggle.SetHandler(m_MarkerList);
            BDE_MapUI_SetScreenRect(toggleHost, sidebarWidth - padding - 42 * scale, 264 * scale, 42 * scale, 32 * scale);
            toggle.Show(false);
            toggleHost.Show(false);
        }

        Widget footer = layoutRoot.FindAnyWidget("MarkerListPanelFooter");
        if (footer && controls)
        {
            if (footer.GetParent() != controls)
                controls.AddChild(footer);
            footer.SetHandler(m_MarkerList);
            BDE_MapUI_SetScreenRect(footer, padding, screenH - 40 * scale, innerWidth, 36 * scale);
        }
        if (markerRoot)
        {
            markerRoot.Show(false);
        }

        // Keep native markers' own visibility settings intact. Hiding their
        // common parent also suppresses markers that Update() shows again.
        if (!m_BDE_MapMarkerLayer)
            m_BDE_MapMarkerLayer = g_Game.GetWorkspace().CreateWidget(FrameWidgetTypeID, 0, 0, 1, 1, WidgetFlags.VISIBLE | WidgetFlags.IGNOREPOINTER, ARGB(255, 255, 255, 255), 2, layoutRoot);
        BDE_MapUI_UpdateMarkerLayer();
        BDE_MapUI_FitMarkerChildren(panel);
        BDE_MapUI_FitMarkerChildren(footer);
        if (m_BDE_ActiveType == "SERVER_INFO")
        {
            BDE_MapUI_StaticRect(true);
            m_BDE_TextFitPasses = 3;
        }
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_STATIC)
        {
            BDE_MapUI_StaticRect(false);
            m_BDE_TextFitPasses = 3;
        }
        m_BDE_LayoutApplied = true;
    }

    // Snapshot before resizing: changing a parent immediately moves/resizes
    // relative children. Applying a common transform preserves button spacing.
    protected void BDE_MapUI_CaptureMarkerGeometry(Widget widget, array<Widget> widgets, array<float> geometry, int depth = 0)
    {
        if (!widget || depth > 20)
            return;
        float x, y, w, h;
        widget.GetScreenPos(x, y);
        widget.GetScreenSize(w, h);
        widgets.Insert(widget);
        geometry.Insert(x);
        geometry.Insert(y);
        geometry.Insert(w);
        geometry.Insert(h);
        Widget child = widget.GetChildren();
        while (child)
        {
            BDE_MapUI_CaptureMarkerGeometry(child, widgets, geometry, depth + 1);
            child = child.GetSibling();
        }
    }

    protected void BDE_MapUI_FitMarkerChildren(Widget parent, int depth = 0)
    {
        if (!parent || depth > 20)
            return;
        float px, py, pw, ph;
        parent.GetScreenPos(px, py);
        parent.GetScreenSize(pw, ph);
        if (pw <= 1)
            return;
        Widget child = parent.GetChildren();
        while (child)
        {
            float x, y, w, h;
            child.GetScreenPos(x, y);
            child.GetScreenSize(w, h);
            if (w > 1)
            {
                float fittedWidth = Math.Min(w, pw);
                float fittedX = Math.Clamp(x, px, px + pw - fittedWidth);
                if (w - fittedWidth > 0.5)
                {
                    ref array<Widget> widgets = new array<Widget>;
                    ref array<float> geometry = new array<float>;
                    BDE_MapUI_CaptureMarkerGeometry(child, widgets, geometry);
                    float ratio = fittedWidth / w;
                    for (int i = 0; i < widgets.Count(); i++)
                    {
                        Widget entry = widgets[i];
                        int offset = i * 4;
                        // Retain vertical scrolling and row heights. Horizontal
                        // fractions then follow future parent-width changes.
                        entry.ClearFlags(WidgetFlags.HEXACTSIZE | WidgetFlags.HEXACTPOS);
                        entry.SetScreenSize(geometry[offset + 2] * ratio, geometry[offset + 3]);
                        entry.SetScreenPos(fittedX + (geometry[offset] - x) * ratio, geometry[offset + 1]);
                    }
                }
                else if (Math.AbsFloat(x - fittedX) > 0.5)
                {
                    // Moving the container also moves its children together.
                    child.SetScreenPos(fittedX, y);
                }
            }
            BDE_MapUI_FitMarkerChildren(child, depth + 1);
            child = child.GetSibling();
        }
    }

    protected void BDE_MapUI_UpdateMarkerLayer()
    {
        if (!m_BDE_MapMarkerLayer)
        {
            return;
        }

        if (m_Markers)
        {
            foreach (ExpansionMapWidgetBase marker : m_Markers)
            {
                if (!marker)
                {
                    continue;
                }

                Widget markerWidget = marker.GetLayoutRoot();
                if (markerWidget && markerWidget.GetParent() != m_BDE_MapMarkerLayer)
                {
                    m_BDE_MapMarkerLayer.AddChild(markerWidget);
                    markerWidget.SetSort(1);
                }
            }
        }
        m_BDE_MapMarkerLayer.Show(!m_BDE_PreviewMode);
    }

    protected void BDE_MapUI_InitFeatureDisplays()
    {
        if (m_BDE_FeaturesInitialised || !m_BDE_Root)
            return;

        m_BDE_BountyDisplay = new BDE_MapUI_BountyDisplay();
        m_BDE_BountyDisplay.Init(m_BDE_Root);

        m_BDE_LeaderboardDisplay = new BDE_MapUI_LeaderboardDisplay();
        m_BDE_LeaderboardDisplay.Init(m_BDE_Root);

        m_BDE_PointsDisplay = new BDE_MapUI_PointsStoreDisplay();
        m_BDE_PointsDisplay.Init(m_BDE_Root);

        m_BDE_CraftingDisplay = new BDE_MapUI_CraftingDisplay();
        m_BDE_CraftingDisplay.Init(m_BDE_Root);

        m_BDE_PlayerDisplay = new BDE_MapUI_PlayerInfoDisplay(PlayerBase.Cast(g_Game.GetPlayer()));
        m_BDE_PlayerDisplay.Init(m_BDE_Root);

        m_BDE_VIPDisplay = new BDE_MapUI_VIPAdminDisplay();
        m_BDE_VIPDisplay.Init(m_BDE_Root);
        m_BDE_VIPDisplay.RequestAccess();
        m_BDE_ReportsDisplay = new BDE_MapUI_ReportsDisplay();
        m_BDE_ReportsDisplay.Init(m_BDE_ReportsHost);
        m_BDE_GroupsDisplay = new BDE_MapUI_GroupsDisplay();
        m_BDE_GroupsDisplay.Init(m_BDE_GroupsHost);
        BDE_MapUI_HideAllContentHosts();
        m_BDE_FeaturesInitialised = true;
    }

    void BDE_MapUI_RefreshConfig()
    {
        if (!m_BDE_Root)
            return;

        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        if (!cfg)
            return;

        Widget mapTools = layoutRoot.FindAnyWidget("Map_Tools_Container");
        if (mapTools)
            mapTools.Show(cfg.ShowMapTools && !m_BDE_PreviewMode);

        if (m_BDE_CurrencyPanel)
            m_BDE_CurrencyPanel.Show(cfg.ShowCurrency);

        if (m_BDE_DonationPanel && !cfg.ShowDonationGoal)
            m_BDE_DonationPanel.Show(false);
        if (m_BDE_DonationAdminButton)
            m_BDE_DonationAdminButton.Show(m_BDE_VIPCanView && cfg.ShowDonationGoal && m_BDE_DonationPanel && m_BDE_DonationPanel.IsVisible());

        int featureIndex;
        m_BDE_InfoPages = new array<ref BDE_MapUI_TabConfig>();
        for (int reset = 0; reset < m_BDE_TabButtons.Count(); reset++)
        {
            m_BDE_TabIndices.Set(reset, -1);
            m_BDE_TabButtons[reset].Show(false);
        }
        for (int i = 0; i < cfg.Tabs.Count(); i++)
        {
            BDE_MapUI_TabConfig tab = cfg.Tabs[i];
            if (!tab)
            {
                continue;
            }
            if (tab.Type == BDE_MAPUI_TAB_STATIC)
            {
                if (BDE_MapUI_PortalService.IsChangelog(tab))
                {
                    BDE_MapUI_BindNavigation(6, i, "Changelog");
                }
                else if (m_BDE_InfoPages.Count() < 5)
                {
                    m_BDE_InfoPages.Insert(tab);
                }
            }
            else if (tab.Type == BDE_MAPUI_TAB_VIP_ADMIN)
            {
                // VIP Manager is intentionally not exposed as a visible top navigation tab.
                // Whitelisted admins open it through the transparent donation-panel hitbox.
            }
            else if (tab.Enabled && featureIndex < 5)
            {
                BDE_MapUI_BindNavigation(featureIndex, i, tab.Label);
                featureIndex++;
            }
        }
        BDE_MapUI_BindNavigation(5, -10, "Server Info");
        string ticketLabel = "Report";
        if (m_BDE_TicketAdmin && m_BDE_TicketUnread > 0)
            ticketLabel = "Ticket (" + m_BDE_TicketUnread.ToString() + ")";
        BDE_MapUI_BindNavigation(7, -12, ticketLabel);
        BDE_MapUI_BindNavigation(8, -13, "Groups");
        for (int page = 0; page < 5; page++)
        {
            Widget pageButton = m_BDE_Root.FindAnyWidget("BDE_InfoTab" + page.ToString());
            bool available = page < m_BDE_InfoPages.Count();
            pageButton.Show(available);
            if (available)
            {
                TextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_InfoTab" + page.ToString() + "Label")).SetText(m_BDE_InfoPages[page].Label);
                pageButton.Enable(m_BDE_InfoPages[page].Enabled);
            }
        }

        for (int j = 0; j < m_BDE_LinkButtons.Count(); j++)
        {
            ButtonWidget linkButton = m_BDE_LinkButtons.Get(j);
            if (!linkButton)
                continue;

            bool showLink = false;
            if (j < cfg.Links.Count())
            {
                BDE_MapUI_LinkConfig link = cfg.Links.Get(j);
                if (link && link.Enabled && link.Label != "" && link.URL != "")
                {
                    TextWidget linkLabel = TextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_LinkLabel" + j.ToString()));
                    if (linkLabel) linkLabel.SetText(link.Label);
                    showLink = true;
                }
            }
            linkButton.Show(showLink);
        }

        BDE_MapUI_UpdateTabColors();


    }

    

    protected string BDE_MapUI_BuildContent(BDE_MapUI_TabConfig tab)
    {
        if (!tab || !tab.Content)
            return "";

        string output = "";
        foreach (string line : tab.Content)
        {
            // Content already includes paragraph/heading markup. Extra newlines
            // add a second blank line between every configured entry.
            output += line;
        }
        return output;
    }

    protected void BDE_MapUI_HideAllContentHosts()
    {
        if (m_BDE_InfoTabs)
        {
            m_BDE_InfoTabs.Show(false);
        }
        if (m_BDE_ReportsHost)
        {
            m_BDE_ReportsHost.Show(false);
        }
        if (m_BDE_GroupsHost)
        {
            m_BDE_GroupsHost.Show(false);
        }
        if (m_BDE_StaticScroll)
            m_BDE_StaticScroll.Show(false);
        if (m_BDE_BountyHost)
            m_BDE_BountyHost.Show(false);
        if (m_BDE_LeaderboardHost)
            m_BDE_LeaderboardHost.Show(false);
        if (m_BDE_PointsHost)
            m_BDE_PointsHost.Show(false);
        if (m_BDE_CraftingHost)
            m_BDE_CraftingHost.Show(false);
        if (m_BDE_PlayerHost) m_BDE_PlayerHost.Show(false);
        if (m_BDE_VIPHost) m_BDE_VIPHost.Show(false);
    }

    protected void BDE_MapUI_CloseActiveFeature()
    {
        if (m_BDE_ActiveType == BDE_MAPUI_TAB_BOUNTIES && m_BDE_BountyDisplay)
            m_BDE_BountyDisplay.OnMenuHide();
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_LEADERBOARD && m_BDE_LeaderboardDisplay)
            m_BDE_LeaderboardDisplay.OnMenuHide();
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_POINTS && m_BDE_PointsDisplay)
            m_BDE_PointsDisplay.OnMenuHide();
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_PLAYER && m_BDE_PlayerDisplay)
            m_BDE_PlayerDisplay.OnMenuHide();
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_CRAFTING && m_BDE_CraftingDisplay)
            m_BDE_CraftingDisplay.OnMenuHide();
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_VIP_ADMIN && m_BDE_VIPDisplay)
            m_BDE_VIPDisplay.OnMenuHide();
    }

    protected void BDE_MapUI_OpenTab(int index)
    {
        if (index <= -10)
        {
            BDE_MapUI_OpenPortal(index);
            return;
        }
        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        if (!cfg || index < 0 || index >= cfg.Tabs.Count())
            return;

        BDE_MapUI_TabConfig tab = cfg.Tabs.Get(index);
        if (!tab || !tab.Enabled)
            return;

        if (m_BDE_ActiveTab == index && m_BDE_ContentPanel && m_BDE_ContentPanel.IsVisible())
        {
            BDE_MapUI_CloseContent();
            return;
        }

        BDE_MapUI_CloseActiveFeature();
        BDE_MapUI_HideAllContentHosts();

        m_BDE_ActiveTab = index;
        m_BDE_ActiveType = tab.Type;
        BDE_MapUI_SetPreviewMode(true);

        // Establish the visible preview viewport before SetItem/SetPlayer.
        if (m_BDE_ContentPanel)
            m_BDE_ContentPanel.Show(true);

        if (m_BDE_ContentTitle)
        {
            string heading = tab.Heading;
            if (heading == "")
                heading = tab.Label;
            m_BDE_ContentTitle.SetText(heading);
        }

        if (tab.Type == BDE_MAPUI_TAB_BOUNTIES)
        {
            if (m_BDE_BountyHost)
                m_BDE_BountyHost.Show(true);
            if (m_BDE_BountyDisplay)
                m_BDE_BountyDisplay.OnMenuShow();
        }
        else if (tab.Type == BDE_MAPUI_TAB_LEADERBOARD)
        {
            if (m_BDE_LeaderboardHost)
                m_BDE_LeaderboardHost.Show(true);
            if (m_BDE_LeaderboardDisplay)
                m_BDE_LeaderboardDisplay.OnMenuShow();
        }
        else if (tab.Type == BDE_MAPUI_TAB_POINTS)
        {
            if (m_BDE_PointsHost)
                m_BDE_PointsHost.Show(true);
            if (m_BDE_PointsDisplay)
                m_BDE_PointsDisplay.OnMenuShow();
        }
        else if (tab.Type == BDE_MAPUI_TAB_CRAFTING)
        {
            if (m_BDE_CraftingHost)
                m_BDE_CraftingHost.Show(true);
            if (m_BDE_CraftingDisplay)
                m_BDE_CraftingDisplay.OnMenuShow();
        }
        else if (tab.Type == BDE_MAPUI_TAB_PLAYER)
        {
            if (m_BDE_PlayerHost) m_BDE_PlayerHost.Show(true);
            if (m_BDE_PlayerDisplay) m_BDE_PlayerDisplay.OnMenuShow();
            BDE_MapUI_Module.Get().RequestPlayerStats();
            m_BDE_PlayerRefreshElapsed = 0;
        }
        else if (tab.Type == BDE_MAPUI_TAB_VIP_ADMIN)
        {
            if (!m_BDE_VIPCanView)
            {
                BDE_MapUI_CloseContent();
                return;
            }
            if (m_BDE_VIPHost) m_BDE_VIPHost.Show(true);
            if (m_BDE_VIPDisplay) m_BDE_VIPDisplay.OnMenuShow();
        }
        else
        {
            m_BDE_ActiveType = BDE_MAPUI_TAB_STATIC;
            BDE_MapUI_StaticRect(false);
            if (BDE_MapUI_PortalService.IsChangelog(tab))
            {
                BDE_MapUI_AcknowledgeChangelog();
            }
            m_BDE_TextFitPasses = 3;
            ScrollWidget scroll = ScrollWidget.Cast(m_BDE_StaticScroll);
            if (scroll) scroll.VScrollToPos01(0);
            if (m_BDE_ContentText)
                m_BDE_ContentText.SetText(BDE_MapUI_BuildContent(tab));
            if (m_BDE_StaticScroll)
                m_BDE_StaticScroll.Show(true);
        }

        BDE_MapUI_UpdateTabColors();
    }

    protected void BDE_MapUI_CloseContent()
    {
        BDE_MapUI_CloseActiveFeature();
        BDE_MapUI_HideAllContentHosts();

        m_BDE_ActiveTab = -1;
        m_BDE_ActiveType = "";
        BDE_MapUI_SetPreviewMode(false);

        if (m_BDE_ContentPanel)
            m_BDE_ContentPanel.Show(false);

        BDE_MapUI_UpdateTabColors();
    }

    protected void BDE_MapUI_SetPreviewMode(bool enabled)
    {
        m_BDE_PreviewMode = enabled;
        BDE_MapUI_UpdateMarkerLayer();

        // A feature page owns the viewport until closed. Do not keep the map's
        // native render pass active beneath the object-preview render pass.
        if (m_MapWidget)
        {
            m_MapWidget.Show(!enabled);
        }

        Widget mapTools = layoutRoot.FindAnyWidget("Map_Tools_Container");
        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        if (mapTools && cfg)
        {
            mapTools.Show(cfg.ShowMapTools && !enabled);
        }

        m_BDE_PreviewDiagnosticDelay = 0;
        if (enabled)
        {
            m_BDE_PreviewDiagnosticDelay = 2.0;
            Print("[BDE_MapUI layout-v6] Open tab=" + m_BDE_ActiveType + " map renderer hidden");
        }
    }

    protected void BDE_MapUI_LogPreviewWidgets(Widget parent)
    {
        if (!parent || !parent.IsVisibleHierarchy()) return;
        ItemPreviewWidget itemWidget = ItemPreviewWidget.Cast(parent);
        PlayerPreviewWidget playerWidget = PlayerPreviewWidget.Cast(parent);
        if (itemWidget || playerWidget)
        {
            float width, height;
            parent.GetScreenSize(width, height);
            float screenX, screenY;
            parent.GetScreenPos(screenX, screenY);
            int sortOrder = parent.GetSort();
            float alpha = parent.GetAlpha();
            string hierarchy = "";
            Widget ancestor = parent;
            while (ancestor)
            {
                hierarchy += ancestor.GetName() + ":" + ancestor.GetSort().ToString() + " ";
                ancestor = ancestor.GetParent();
            }
            string model = "none";
            if (itemWidget && itemWidget.GetItem()) model = itemWidget.GetItem().GetType();
            if (playerWidget && playerWidget.GetDummyPlayer()) model = playerWidget.GetDummyPlayer().GetType();
            Print("[BDE_MapUI layout-v6] " + parent.GetName() + " model=" + model + " viewport=" + width.ToString() + "x" + height.ToString() + " position=" + screenX.ToString() + "," + screenY.ToString() + " sort=" + sortOrder.ToString() + " alpha=" + alpha.ToString() + " layers=" + hierarchy);
        }
        Widget child = parent.GetChildren();
        while (child)
        {
            BDE_MapUI_LogPreviewWidgets(child);
            child = child.GetSibling();
        }
    }

    protected void BDE_MapUI_UpdateTabColors()
    {
        if (!m_BDE_TabButtons)
            return;

        for (int i = 0; i < m_BDE_TabButtons.Count(); i++)
        {
            ButtonWidget button = m_BDE_TabButtons.Get(i);
            if (!button)
                continue;

            if (m_BDE_TabIndices[i] == m_BDE_ActiveTab && m_BDE_ContentPanel && m_BDE_ContentPanel.IsVisible())
                button.SetColor(BDE_MapUI_Theme.ACTIVE);
            else
                button.SetColor(BDE_MapUI_Theme.BUTTON);
        }
    }

    protected bool BDE_MapUI_HandleLinkClick(Widget w)
    {
        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        if (!cfg)
            return false;

        for (int i = 0; i < m_BDE_LinkButtons.Count(); i++)
        {
            if (w != m_BDE_LinkButtons.Get(i))
                continue;

            if (i < cfg.Links.Count())
            {
                BDE_MapUI_LinkConfig link = cfg.Links.Get(i);
                if (link && link.Enabled && link.URL != "")
                {
                    g_Game.OpenURL(link.URL);
                    return true;
                }
            }
        }

        return false;
    }

    protected string BDE_MapUI_FormatMoney(int amount)
    {
        if (amount < 0)
            amount = 0;
        return "$" + ExpansionStatic.IntToCurrencyString(amount, ",");
    }

    void BDE_MapUI_ReceiveMoney(int bankMoney, int inventoryMoney)
    {
        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        if (!cfg)
            return;

        if (m_BDE_BankMoney)
            m_BDE_BankMoney.SetText(cfg.BankLabel + ": " + BDE_MapUI_FormatMoney(bankMoney));
        if (m_BDE_InventoryMoney)
            m_BDE_InventoryMoney.SetText(cfg.InventoryLabel + ": " + BDE_MapUI_FormatMoney(inventoryMoney));
    }

    void BDE_MapUI_ReceiveDonation(BDE_MapUI_DonationProgressData data)
    {
        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        if (!m_BDE_DonationPanel || !cfg)
            return;

        if (!cfg.ShowDonationGoal || !data || data.Enabled != 1)
        {
            m_BDE_DonationPanel.Show(false);
            if (m_BDE_DonationAdminButton)
                m_BDE_DonationAdminButton.Show(false);
            return;
        }

        m_BDE_DonationPanel.Show(true);
        if (m_BDE_DonationAdminButton)
            m_BDE_DonationAdminButton.Show(m_BDE_VIPCanView);

        if (m_BDE_DonationMessage)
        {
            m_BDE_DonationMessage.Show(cfg.ShowDonationMessage);
            if (cfg.ShowDonationMessage)
                m_BDE_DonationMessage.SetText(data.Message);
        }

        float percent = 0.0;
        string amountText = "Monthly goal not configured";
        string symbol = data.CurrencySymbol;
        if (symbol == "")
            symbol = "$";

        if (data.MonthlyGoal > 0)
        {
            percent = (data.CurrentAmount * 100.0) / data.MonthlyGoal;
            if (percent < 0.0)
                percent = 0.0;
            else if (percent > 100.0)
                percent = 100.0;

            amountText = symbol + ExpansionStatic.IntToCurrencyString(data.CurrentAmount, ",") + " / " + symbol + ExpansionStatic.IntToCurrencyString(data.MonthlyGoal, ",");
        }

        if (m_BDE_DonationAmount)
            m_BDE_DonationAmount.SetText(amountText);
        if (m_BDE_DonationProgress)
            m_BDE_DonationProgress.SetCurrent(percent);
    }

    void BDE_MapUI_ReceiveBountyList(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_BountyDisplay)
            m_BDE_BountyDisplay.ReceiveBountyList(type, ctx, sender, target);
    }

    void BDE_MapUI_ReceiveBountyDetails(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_BountyDisplay)
            m_BDE_BountyDisplay.ReceiveBountyDetails(type, ctx, sender, target);
    }

    void BDE_MapUI_ReceiveBountyActionResult(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_BountyDisplay)
            m_BDE_BountyDisplay.ReceiveBountyActionResult(type, ctx, sender, target);
    }

    void BDE_MapUI_ReceiveLeaderboardList(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_LeaderboardDisplay)
            m_BDE_LeaderboardDisplay.ReceiveLeaderboardList(type, ctx, sender, target);
    }

    void BDE_MapUI_ReceiveLeaderboardDetails(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_LeaderboardDisplay)
            m_BDE_LeaderboardDisplay.ReceiveLeaderboardDetails(type, ctx, sender, target);
    }

    void BDE_MapUI_ReceivePointsStoreData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_PointsDisplay)
            m_BDE_PointsDisplay.ReceivePointsStoreData(type, ctx, sender, target);
    }

    void BDE_MapUI_ReceivePointsStoreAction(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_PointsDisplay)
            m_BDE_PointsDisplay.ReceivePointsStoreAction(type, ctx, sender, target);
    }

    void BDE_MapUI_ReceiveVIPAccess(bool canView, bool canManage)
    {
        m_BDE_VIPCanView = canView;
        m_BDE_VIPCanManage = canManage;
        if (m_BDE_VIPDisplay)
            m_BDE_VIPDisplay.ReceiveAccess(canView, canManage);
        BDE_MapUI_RefreshConfig();
    }

    void BDE_MapUI_ReceiveVIPData(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_VIPDisplay)
            m_BDE_VIPDisplay.ReceiveData(type, ctx, sender, target);
    }

    void BDE_MapUI_ReceiveVIPMessage(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (m_BDE_VIPDisplay)
            m_BDE_VIPDisplay.ReceiveMessage(type, ctx, sender, target);
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (m_BDE_ActiveType == "REPORTS" && m_BDE_ReportsDisplay.OnClick(w))
        {
            return true;
        }
        if (m_BDE_ActiveType == "GROUPS" && m_BDE_GroupsDisplay.OnClick(w))
        {
            return true;
        }
        if (m_BDE_ActiveType == "SERVER_INFO")
        {
            for (int page = 0; page < m_BDE_InfoPages.Count(); page++)
            {
                if (w == m_BDE_Root.FindAnyWidget("BDE_InfoTab" + page.ToString()))
                {
                    BDE_MapUI_ShowInfoPage(page);
                    return true;
                }
            }
        }
        if (w == m_BDE_DonationAdminButton)
        {
            if (!m_BDE_VIPCanView)
                return true;

            BDE_MapUI_Config vipCfg = BDE_MapUI_ConfigManager.Get();
            if (vipCfg && vipCfg.Tabs)
            {
                for (int vipTabIndex = 0; vipTabIndex < vipCfg.Tabs.Count(); vipTabIndex++)
                {
                    BDE_MapUI_TabConfig vipTab = vipCfg.Tabs[vipTabIndex];
                    if (vipTab && vipTab.Enabled && vipTab.Type == BDE_MAPUI_TAB_VIP_ADMIN)
                    {
                        BDE_MapUI_OpenTab(vipTabIndex);
                        return true;
                    }
                }
            }
            return true;
        }

        if (w == m_BDE_ContentClose)
        {
            BDE_MapUI_CloseContent();
            return true;
        }

        if (m_BDE_ActiveType == BDE_MAPUI_TAB_BOUNTIES && m_BDE_BountyDisplay && m_BDE_BountyDisplay.OnClick(w, x, y, button))
            return true;
        if (m_BDE_ActiveType == BDE_MAPUI_TAB_LEADERBOARD && m_BDE_LeaderboardDisplay && m_BDE_LeaderboardDisplay.OnClick(w, x, y, button))
            return true;
        if (m_BDE_ActiveType == BDE_MAPUI_TAB_POINTS && m_BDE_PointsDisplay && m_BDE_PointsDisplay.OnClick(w, x, y, button))
            return true;
        if (m_BDE_ActiveType == BDE_MAPUI_TAB_VIP_ADMIN && m_BDE_VIPDisplay && m_BDE_VIPDisplay.OnClick(w, x, y, button))
            return true;

        if (m_BDE_TabButtons)
        {
            for (int i = 0; i < m_BDE_TabButtons.Count(); i++)
            {
                if (w == m_BDE_TabButtons.Get(i))
                {
                    BDE_MapUI_OpenTab(m_BDE_TabIndices[i]);
                    return true;
                }
            }
        }

        if (m_BDE_LinkButtons && BDE_MapUI_HandleLinkClick(w))
            return true;

        return super.OnClick(w, x, y, button);
    }

    protected bool BDE_MapUI_IsTyping()
    {
        Widget focus = GetFocus();
        if (!focus || !focus.IsVisibleHierarchy())
        {
            return false;
        }
        if (!focus.IsInherited(EditBoxWidget) && !focus.IsInherited(MultilineEditBoxWidget))
        {
            return false;
        }
        Widget parent = focus;
        while (parent)
        {
            if (parent == m_BDE_Root)
            {
                return true;
            }
            parent = parent.GetParent();
        }
        return false;
    }

    protected void BDE_MapUI_SetTextInputLock(bool locked)
    {
        if (m_BDE_TextInputLocked == locked)
        {
            return;
        }
        m_BDE_TextInputLocked = locked;
        if (locked)
        {
            g_Game.GetInput().ChangeGameFocus(1);
        }
        else
        {
            g_Game.GetInput().ChangeGameFocus(-1);
        }
    }

    override void Update(float timeslice)
    {
        bool typing = BDE_MapUI_IsTyping();
        BDE_MapUI_SetTextInputLock(typing);
        // Expansion polls map shortcuts directly, even when an edit box has focus.
        if (!typing)
        {
            super.Update(timeslice);
        }
        BDE_MapUI_ApplyLayout();
        BDE_MapUI_UpdateMarkerLayer();
        m_BDE_MarkerFitElapsed += timeslice;
        if (m_BDE_MarkerFitElapsed >= 0.25)
        {
            m_BDE_MarkerFitElapsed = 0;
            BDE_MapUI_FitMarkerChildren(layoutRoot.FindAnyWidget("MarkerListPanel"));
            BDE_MapUI_FitMarkerChildren(layoutRoot.FindAnyWidget("MarkerListPanelFooter"));
        }
        if (m_BDE_ActiveType == "REPORTS")
        {
            m_BDE_ReportsDisplay.Update();
        }
        if (m_BDE_ActiveType == "GROUPS")
        {
            m_BDE_GroupsDisplay.Update(timeslice);
        }

        if (m_BDE_PreviewDiagnosticDelay > 0)
        {
            m_BDE_PreviewDiagnosticDelay -= timeslice;
            if (m_BDE_PreviewDiagnosticDelay <= 0)
                BDE_MapUI_LogPreviewWidgets(m_BDE_ContentPanel);
        }


        if (m_BDE_ActiveType == BDE_MAPUI_TAB_BOUNTIES && m_BDE_BountyDisplay)
            m_BDE_BountyDisplay.Update(timeslice);
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_LEADERBOARD && m_BDE_LeaderboardDisplay)
            m_BDE_LeaderboardDisplay.Update(timeslice);
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_POINTS && m_BDE_PointsDisplay)
            m_BDE_PointsDisplay.Update(timeslice);
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_CRAFTING && m_BDE_CraftingDisplay)
            m_BDE_CraftingDisplay.Update(timeslice);
        else if (m_BDE_ActiveType == BDE_MAPUI_TAB_VIP_ADMIN && m_BDE_VIPDisplay)
            m_BDE_VIPDisplay.Update(timeslice);

        if ((m_BDE_ActiveType == BDE_MAPUI_TAB_STATIC || m_BDE_ActiveType == "SERVER_INFO") && m_BDE_TextFitPasses > 0)
        {
            BDE_MapUI_FitText();
            m_BDE_TextFitPasses--;
        }
        if (m_BDE_ActiveType == BDE_MAPUI_TAB_PLAYER && m_BDE_PlayerDisplay)
        {
            m_BDE_PlayerDisplay.Update(timeslice);
            m_BDE_PlayerRefreshElapsed += timeslice;
            if (m_BDE_PlayerRefreshElapsed >= 2.0)
            {
                m_BDE_PlayerRefreshElapsed = 0;
                BDE_MapUI_Module.Get().RequestPlayerStats();
            }
        }
        BDE_MapUI_Config cfg = BDE_MapUI_ConfigManager.Get();
        if (cfg)
        {
            m_BDE_SidebarRefreshElapsed += timeslice;
            if (m_BDE_SidebarRefreshElapsed >= cfg.SidebarRefreshSeconds)
            {
                m_BDE_SidebarRefreshElapsed = 0.0;
                BDE_MapUI_Module.Get().RequestSidebarData();
            }
        }
    }
    protected void BDE_MapUI_FitText()
    {
        RichTextWidget widget = m_BDE_ContentText;
        ScrollWidget scrollParent = ScrollWidget.Cast(m_BDE_StaticScroll);
        if (!widget)
            return;

        // Layout must not use "size to text v" or it overrides scripted height.
        float wPx;
        float hPx;
        widget.GetScreenSize(wPx, hPx);
        if (wPx <= 1)
            return;

        widget.ClearFlags(WidgetFlags.HEXACTSIZE | WidgetFlags.VEXACTSIZE);
        widget.SetFlags(WidgetFlags.HEXACTSIZE | WidgetFlags.VEXACTSIZE);
        widget.SetScreenSize(wPx, hPx);
        widget.Update();

        float contentH = widget.GetContentHeight();
        if (contentH <= 0)
            return;

        int numLines = widget.GetNumLines();
        float pad = 16.0;
        if (numLines > 0)
        {
            float lineH = contentH / numLines;
            float extra = lineH * 2.0;
            if (extra > pad)
                pad = extra;
        }

        float targetH = Math.Ceil(contentH + pad);
        widget.SetScreenSize(wPx, targetH);
        widget.Update();
        if (scrollParent)
            scrollParent.Update();
    }
    void BDE_MapUI_ReceivePlayerStats(TIntArray ints, TFloatArray floats, bool diseased)
    {
        if (m_BDE_PlayerDisplay && m_BDE_ActiveType == BDE_MAPUI_TAB_PLAYER)
            m_BDE_PlayerDisplay.UpdatePlayerInfo(ints, floats, diseased);
    }

    protected void BDE_MapUI_BindNavigation(int slot, int index, string label)
    {
        m_BDE_TabIndices.Set(slot, index);
        TextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_TabLabel" + slot.ToString())).SetText(label);
        m_BDE_TabButtons[slot].Show(true);
    }

    void BDE_MapUI_OnConfigReady()
    {
        if (m_BDE_PortalStatusRequested)
        {
            return;
        }
        m_BDE_PortalStatusRequested = true;
        if (BDE_MapUI_ConfigManager.Get().OpenInfoOnMapOpen && m_BDE_ActiveTab == -1)
        {
            BDE_MapUI_OpenPortal(-10);
        }
        BDE_MapUI_PortalRequest request = new BDE_MapUI_PortalRequest();
        request.Action = "STATUS";
        BDE_MapUI_PortalService.Get().Request(request);
    }

    protected void BDE_MapUI_StaticRect(bool info)
    {
        float x, y, width, height;
        m_BDE_ContentPanel.GetScreenPos(x, y);
        m_BDE_ContentPanel.GetScreenSize(width, height);
        float top = 72;
        if (info)
        {
            top = 128;
            BDE_MapUI_SetScreenRect(m_BDE_InfoTabs, x + 18, y + 68, width - 36, 46, false);
        }
        BDE_MapUI_SetScreenRect(m_BDE_StaticScroll, x + 18, y + top, width - 36, height - top - 16, false);
    }

    protected void BDE_MapUI_OpenPortal(int index)
    {
        if (m_BDE_ActiveTab == index)
        {
            BDE_MapUI_CloseContent();
            return;
        }
        BDE_MapUI_CloseActiveFeature();
        BDE_MapUI_HideAllContentHosts();
        m_BDE_ActiveTab = index;
        BDE_MapUI_SetPreviewMode(true);
        m_BDE_ContentPanel.Show(true);
        if (index == -10)
        {
            m_BDE_ActiveType = "SERVER_INFO";
            m_BDE_InfoTabs.Show(true);
            BDE_MapUI_StaticRect(true);
            for (int i = 0; i < m_BDE_InfoPages.Count(); i++)
            {
                if (m_BDE_InfoPages[i].Enabled)
                {
                    BDE_MapUI_ShowInfoPage(i);
                    break;
                }
            }
        }
        else if (index == -12)
        {
            m_BDE_ActiveType = "REPORTS";
            m_BDE_ContentTitle.SetText("Reports & Support");
            m_BDE_ReportsHost.Show(true);
            bool adminInbox = m_BDE_TicketAdmin && m_BDE_TicketUnread > 0;
            m_BDE_ReportsDisplay.Show(adminInbox);
        }
        else if (index == -13)
        {
            m_BDE_ActiveType = "GROUPS";
            m_BDE_ContentTitle.SetText("Groups");
            m_BDE_GroupsHost.Show(true);
            m_BDE_GroupsDisplay.Show();
        }
        BDE_MapUI_UpdateTabColors();
    }

    protected void BDE_MapUI_ShowInfoPage(int index)
    {
        BDE_MapUI_TabConfig page = m_BDE_InfoPages[index];
        if (!page.Enabled)
        {
            return;
        }
        m_BDE_ContentTitle.SetText("Server Info / " + page.Heading);
        m_BDE_ContentText.SetText(BDE_MapUI_BuildContent(page));
        m_BDE_StaticScroll.Show(true);
        ScrollWidget.Cast(m_BDE_StaticScroll).VScrollToPos01(0);
        m_BDE_TextFitPasses = 3;
        for (int i = 0; i < m_BDE_InfoPages.Count(); i++)
        {
            int color = BDE_MapUI_Theme.BUTTON;
            if (i == index)
            {
                color = BDE_MapUI_Theme.ACTIVE;
            }
            m_BDE_Root.FindAnyWidget("BDE_InfoTab" + i.ToString()).SetColor(color);
        }
    }

    protected void BDE_MapUI_AcknowledgeChangelog()
    {
        if (m_BDE_ChangelogRevision <= 0)
        {
            return;
        }
        BDE_MapUI_PortalRequest request = new BDE_MapUI_PortalRequest();
        request.Action = "CHANGELOG_READ";
        request.Version = m_BDE_ChangelogRevision;
        BDE_MapUI_PortalService.Get().Request(request);
    }

    void BDE_MapUI_ReceivePortal(BDE_MapUI_PortalReply reply)
    {
        m_BDE_TicketUnread = reply.AdminUnreadCount;
        m_BDE_TicketAdmin = reply.IsAdmin;
        if (m_BDE_Root)
        {
            string reportLabel = "Report";
            if (reply.IsAdmin && reply.AdminUnreadCount > 0)
                reportLabel = "Ticket (" + reply.AdminUnreadCount.ToString() + ")";
            TextWidget.Cast(m_BDE_Root.FindAnyWidget("BDE_TabLabel7")).SetText(reportLabel);
        }
        if (reply.Action == "TICKET_STATUS")
        {
            if (m_BDE_ActiveType == "REPORTS" && m_BDE_ReportsDisplay)
                m_BDE_ReportsDisplay.Poll();
            return;
        }
        if (reply.Action == "STATUS")
        {
            m_BDE_ChangelogRevision = reply.ChangelogRevision;
            if (reply.ChangelogPending && !m_BDE_AutoChangeHandled)
            {
                m_BDE_AutoChangeHandled = true;
                int index = m_BDE_TabIndices[6];
                if (index >= 0)
                {
                    if (m_BDE_ActiveTab != index)
                    {
                        BDE_MapUI_OpenTab(index);
                    }
                    else
                    {
                        BDE_MapUI_AcknowledgeChangelog();
                    }
                }
            }
            return;
        }
        if (reply.Action == "GROUP_PLAYERS")
        {
            m_BDE_GroupsDisplay.Receive(reply);
        }
        else if (m_BDE_ActiveType == "REPORTS")
        {
            m_BDE_ReportsDisplay.Receive(reply);
        }
    }

    override void OnHide()
    {
        BDE_MapUI_SetTextInputLock(false);
        BDE_MapUI_CloseContent();
        super.OnHide();
    }
};
