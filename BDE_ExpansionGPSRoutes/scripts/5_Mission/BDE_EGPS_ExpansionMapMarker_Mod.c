// Adds route controls to Expansion marker edit panels.

modded class ExpansionMapMarker
{
    protected Widget       m_BDEEGPS_RouteButtonsRoot;
    protected ButtonWidget m_BDEEGPS_BtnRouteToggle;
    protected ButtonWidget m_BDEEGPS_BtnRouteClear;

    override protected void OnEditInit(Widget layoutRoot)
    {
        super.OnEditInit(layoutRoot);
        BDEEGPS_InjectRouteButtons(layoutRoot);
    }

    override void ShowEditPanel()
    {
        super.ShowEditPanel();
        BDEEGPS_UpdateRouteButtons();
    }

    override void CloseEditPanel()
    {
        super.CloseEditPanel();
    }

    override bool OnClick(Widget w, int x, int y, int button)
    {
        if (button == MouseState.LEFT)
        {
            if (w == m_BDEEGPS_BtnRouteToggle)
            {
                BDEEGPS_ToggleRouteMarker();
                return true;
            }

            if (w == m_BDEEGPS_BtnRouteClear)
            {
                BDE_EGPS_Manager mgrClear = BDE_EGPS_Manager.GetInstance();
                if (mgrClear)
                    mgrClear.ClearRoute();

                BDEEGPS_UpdateRouteButtons();
                BDEEGPS_ForceMapRedraw();
                return true;
            }
        }

        return super.OnClick(w, x, y, button);
    }

    protected void BDEEGPS_InjectRouteButtons(Widget layoutRoot)
    {
        if (!layoutRoot || !GetGame() || !GetGame().GetWorkspace())
            return;

        m_BDEEGPS_RouteButtonsRoot = GetGame().GetWorkspace().CreateWidgets("BDE_ExpansionGPSRoutes/gui/layouts/route_marker_buttons.layout", layoutRoot);
        if (!m_BDEEGPS_RouteButtonsRoot)
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " Marker: failed to create route buttons layout.");
            return;
        }

        m_BDEEGPS_BtnRouteToggle = ButtonWidget.Cast(BDE_EGPS_WidgetUtils.FindChildByName(m_BDEEGPS_RouteButtonsRoot, "bdeegps_route_toggle"));
        m_BDEEGPS_BtnRouteClear = ButtonWidget.Cast(BDE_EGPS_WidgetUtils.FindChildByName(m_BDEEGPS_RouteButtonsRoot, "bdeegps_route_clear"));

        BDEEGPS_UpdateRouteButtons();
    }

    protected void BDEEGPS_UpdateRouteButtons()
    {
        bool showToggle = IsCreating() || m_Data;
        bool showClear = false;

        BDE_EGPS_Manager mgr = BDE_EGPS_Manager.GetInstance();
        BDE_EGPS_Route route;
        if (mgr)
            route = mgr.GetRoute();

        if (route && route.HasWaypoints())
            showClear = true;

        if (m_BDEEGPS_BtnRouteToggle)
        {
            m_BDEEGPS_BtnRouteToggle.Show(showToggle);

            if (showToggle)
            {
                if (!IsCreating() && mgr && m_Data && mgr.IsMarkerInRoute(m_Data.GetUID()))
                    m_BDEEGPS_BtnRouteToggle.SetText("REMOVE ROUTE");
                else
                    m_BDEEGPS_BtnRouteToggle.SetText("ADD ROUTE");
            }
        }

        if (m_BDEEGPS_BtnRouteClear)
            m_BDEEGPS_BtnRouteClear.Show(showClear);
    }

    protected void BDEEGPS_ToggleRouteMarker()
    {
        BDE_EGPS_Manager mgr = BDE_EGPS_Manager.GetInstance();
        if (!mgr || !mgr.EnsureReady(true))
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " Add Route aborted because RoadGraph_Core is not ready for the current world.");
            if (m_BDEEGPS_BtnRouteToggle)
                m_BDEEGPS_BtnRouteToggle.SetText("ROAD GRAPH NOT READY");
            return;
        }

        bool removeExistingRoute = false;
        string uid = string.Empty;

        if (!IsCreating() && m_Data)
        {
            uid = m_Data.GetUID();
            if (uid != string.Empty && mgr.IsMarkerInRoute(uid))
                removeExistingRoute = true;
        }

        // Re-use Expansion's normal create/update flow so the marker edit menu
        // closes the same way the CREATE/UPDATE button does. In create mode this
        // creates the marker first, then immediately adds that new marker to the route.
        OnCreateButtonClick();

        if (!m_Data)
            return;

        if (removeExistingRoute)
        {
            uid = m_Data.GetUID();
            if (uid != string.Empty)
                mgr.RemoveMarkerFromRoute(uid);
        }
        else
        {
            bool routeAdded = mgr.AddMarkerToRoute(m_Data);
            if (!routeAdded)
            {
                Print(BDE_EGPS_Constants.LOG_TAG + " Add Route failed; check the RPT for the RoadGraph/pathfinder diagnostic immediately above this line.");
                if (m_BDEEGPS_BtnRouteToggle)
                    m_BDEEGPS_BtnRouteToggle.SetText("NO ROUTE - CHECK LOG");
                return;
            }
        }

        BDEEGPS_UpdateRouteButtons();
        BDEEGPS_ForceMapRedraw();
    }

    protected void BDEEGPS_ForceMapRedraw()
    {
        ExpansionMapMenu menu = ExpansionMapMenu.Cast(g_Game.GetUIManager().GetMenu());
        if (menu)
            menu.BDEEGPS_ForceRouteRedraw();
    }
}
