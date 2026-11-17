// Draws the active route over the Expansion GPS HUD/minimap.

modded class IngameHud
{
    protected CanvasWidget m_BDEEGPS_GPSRouteCanvas;
    protected ref array<vector> m_BDEEGPS_GPSEmptyPath;
    protected bool m_BDEEGPS_GPSHadRouteLastFrame;

    protected vector m_BDEEGPS_GPSLastRouteMapPos;
    protected float  m_BDEEGPS_GPSLastRouteMapScale;
    protected int    m_BDEEGPS_GPSLastRouteTicks;
    protected int    m_BDEEGPS_GPSLastRouteIdx;
    protected vector m_BDEEGPS_GPSLastRoutePlayerPos;
    protected float  m_BDEEGPS_GPSLastRouteCanvasW;
    protected float  m_BDEEGPS_GPSLastRouteCanvasH;
    protected bool   m_BDEEGPS_GPSRouteDrawCached;

    protected ButtonWidget m_BDEEGPS_ModeToast;
    protected TextWidget   m_BDEEGPS_ModeToastText;
    protected bool         m_BDEEGPS_ModeToastInjected;
    protected int          m_BDEEGPS_ModeToastHideAtMs;

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        BDEEGPS_UpdateGPSRouteOverlay();
        BDEEGPS_TickModeToast();
    }

    void BDEEGPS_ShowModeToast()
    {
        BDEEGPS_TryInjectModeToast();

        if (!m_BDEEGPS_ModeToast || !m_BDEEGPS_ModeToastText)
            return;

        BDE_EGPS_Manager mgr = BDE_EGPS_Manager.GetInstance();
        if (!mgr)
            return;

        if (mgr.GetMode() == RGC_DrivabilityMode.ROAD)
            m_BDEEGPS_ModeToastText.SetText("MODE: ROAD");
        else
            m_BDEEGPS_ModeToastText.SetText("MODE: DRIVABLE");

        m_BDEEGPS_ModeToast.Show(true);
        m_BDEEGPS_ModeToastHideAtMs = GetGame().GetTime() + 5000;
    }

    protected void BDEEGPS_UpdateGPSRouteOverlay()
    {
        if (!m_GPSPanel || !m_GPSPanel.IsVisible())
        {
            BDEEGPS_ClearGPSRouteOverlay();
            return;
        }

        if (!m_GPSMapPanel || !m_GPSMapPanel.IsVisible() || !m_MapWidget)
        {
            BDEEGPS_ClearGPSRouteOverlay();
            return;
        }

        BDE_EGPS_Manager mgr = BDE_EGPS_Manager.GetInstance();
        if (!mgr)
            return;

        BDE_EGPS_Route route = mgr.GetRoute();
        bool hasRouteNow = (route && route.HasWaypoints());

        if (!hasRouteNow)
        {
            BDEEGPS_ClearGPSRouteOverlay();
            return;
        }

        if (!m_BDEEGPS_GPSRouteCanvas)
        {
            Widget parentWidget = m_MapMarkerPanel;
            if (!parentWidget)
                parentWidget = m_MapWidget;

            m_BDEEGPS_GPSRouteCanvas = BDEEGPS_CreateGPSRouteCanvas(parentWidget);
            if (!m_BDEEGPS_GPSRouteCanvas)
                return;
            m_BDEEGPS_GPSRouteDrawCached = false;
        }

        m_BDEEGPS_GPSHadRouteLastFrame = true;

        array<vector> path = route.GetCurrentPathPositions();
        if (!path)
        {
            if (!m_BDEEGPS_GPSEmptyPath)
                m_BDEEGPS_GPSEmptyPath = new array<vector>;
            else
                m_BDEEGPS_GPSEmptyPath.Clear();
            path = m_BDEEGPS_GPSEmptyPath;
        }

        int currentTraversedIdx = route.GetTraversedNodeIdx();
        vector currentPlayerPos = vector.Zero;
        if (GetGame() && GetGame().GetPlayer())
            currentPlayerPos = GetGame().GetPlayer().GetPosition();

        float ox, oy;
        m_BDEEGPS_GPSRouteCanvas.GetScreenPos(ox, oy);

        float canvasW;
        float canvasH;
        m_BDEEGPS_GPSRouteCanvas.GetScreenSize(canvasW, canvasH);

        vector currentMapPos = m_MapWidget.GetMapPos();
        float currentMapScale = m_MapWidget.GetScale();
        int currentPathTicks = route.GetLastRecomputeTicks();

        bool routeDirty = !m_BDEEGPS_GPSRouteDrawCached;
        if (!routeDirty && currentPathTicks != m_BDEEGPS_GPSLastRouteTicks)
            routeDirty = true;
        if (!routeDirty && currentTraversedIdx != m_BDEEGPS_GPSLastRouteIdx)
            routeDirty = true;
        if (!routeDirty && currentMapScale != m_BDEEGPS_GPSLastRouteMapScale)
            routeDirty = true;
        if (!routeDirty && vector.Distance(currentMapPos, m_BDEEGPS_GPSLastRouteMapPos) > 0.01)
            routeDirty = true;
        if (!routeDirty && vector.DistanceSq(currentPlayerPos, m_BDEEGPS_GPSLastRoutePlayerPos) > 1.0)
            routeDirty = true;
        if (!routeDirty && (canvasW != m_BDEEGPS_GPSLastRouteCanvasW || canvasH != m_BDEEGPS_GPSLastRouteCanvasH))
            routeDirty = true;

        if (!routeDirty)
            return;

        m_BDEEGPS_GPSRouteCanvas.Clear();

        BDE_EGPS_RouteDrawer.DrawFullRoute(
            m_MapWidget,
            m_BDEEGPS_GPSRouteCanvas,
            path,
            currentTraversedIdx,
            currentPlayerPos,
            route.GetWaypoints(),
            route.GetSegmentEndIndices(),
            route.GetTailStartPath(),
            route.GetTailEndPath(),
            BDE_EGPS_DrawStyle.GetForHud(),
            ox, oy,
            canvasW, canvasH);

        m_BDEEGPS_GPSLastRouteMapPos = currentMapPos;
        m_BDEEGPS_GPSLastRouteMapScale = currentMapScale;
        m_BDEEGPS_GPSLastRouteTicks = currentPathTicks;
        m_BDEEGPS_GPSLastRouteIdx = currentTraversedIdx;
        m_BDEEGPS_GPSLastRoutePlayerPos = currentPlayerPos;
        m_BDEEGPS_GPSLastRouteCanvasW = canvasW;
        m_BDEEGPS_GPSLastRouteCanvasH = canvasH;
        m_BDEEGPS_GPSRouteDrawCached = true;
    }

    protected void BDEEGPS_ClearGPSRouteOverlay()
    {
        if (m_BDEEGPS_GPSHadRouteLastFrame)
        {
            if (m_BDEEGPS_GPSRouteCanvas)
                m_BDEEGPS_GPSRouteCanvas.Clear();

            m_BDEEGPS_GPSHadRouteLastFrame = false;
            m_BDEEGPS_GPSRouteDrawCached = false;
        }
    }

    protected CanvasWidget BDEEGPS_CreateGPSRouteCanvas(Widget parentWidget)
    {
        if (!GetGame() || !GetGame().GetWorkspace() || !parentWidget)
            return null;

        Widget root = GetGame().GetWorkspace().CreateWidgets("BDE_ExpansionGPSRoutes/gui/layouts/route_canvas.layout", parentWidget);
        CanvasWidget canvas = CanvasWidget.Cast(root);
        if (!canvas)
            canvas = CanvasWidget.Cast(BDE_EGPS_WidgetUtils.FindChildByName(root, "bdeegps_route_canvas"));

        if (canvas)
        {
            canvas.SetSort(50);
            canvas.Show(true);
            canvas.Clear();
        }
        else
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " IngameHud: failed to create GPS route canvas.");
        }

        return canvas;
    }

    protected void BDEEGPS_TryInjectModeToast()
    {
        if (m_BDEEGPS_ModeToastInjected)
            return;

        if (!GetGame() || !GetGame().GetWorkspace() || !m_GPSPanel)
            return;

        Widget root = GetGame().GetWorkspace().CreateWidgets("BDE_ExpansionGPSRoutes/gui/layouts/route_mode_button.layout", m_GPSPanel);
        if (!root)
            return;

        m_BDEEGPS_ModeToast = ButtonWidget.Cast(root);
        if (!m_BDEEGPS_ModeToast)
            m_BDEEGPS_ModeToast = ButtonWidget.Cast(BDE_EGPS_WidgetUtils.FindChildByName(root, "btn_bdeegps_route_mode"));

        if (!m_BDEEGPS_ModeToast)
            return;

        m_BDEEGPS_ModeToastText = TextWidget.Cast(BDE_EGPS_WidgetUtils.FindChildByName(m_BDEEGPS_ModeToast, "btn_bdeegps_route_mode_text"));
        m_BDEEGPS_ModeToast.SetSort(2000);
        m_BDEEGPS_ModeToast.Show(false);
        m_BDEEGPS_ModeToastInjected = true;
    }

    protected void BDEEGPS_TickModeToast()
    {
        if (m_BDEEGPS_ModeToastHideAtMs == 0)
            return;

        if (!m_BDEEGPS_ModeToast)
            return;

        if (GetGame().GetTime() >= m_BDEEGPS_ModeToastHideAtMs)
        {
            m_BDEEGPS_ModeToast.Show(false);
            m_BDEEGPS_ModeToastHideAtMs = 0;
        }
    }
}
