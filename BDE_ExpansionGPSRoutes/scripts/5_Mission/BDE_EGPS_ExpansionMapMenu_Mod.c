// Draws the active route over the Expansion big map.

modded class ExpansionMapMenu
{
    protected CanvasWidget m_BDEEGPS_RouteCanvas;
    protected ref array<vector> m_BDEEGPS_EmptyPath;
    protected bool m_BDEEGPS_HadRouteLastFrame;

    protected vector m_BDEEGPS_LastRouteMapPos;
    protected float  m_BDEEGPS_LastRouteMapScale;
    protected int    m_BDEEGPS_LastRouteTicks;
    protected int    m_BDEEGPS_LastRouteIdx;
    protected vector m_BDEEGPS_LastRoutePlayerPos;
    protected float  m_BDEEGPS_LastRouteCanvasW;
    protected float  m_BDEEGPS_LastRouteCanvasH;
    protected bool   m_BDEEGPS_RouteDrawCached;

    override void Update(float timeslice)
    {
        super.Update(timeslice);
        BDEEGPS_UpdateRouteOverlay();
    }

    void BDEEGPS_ForceRouteRedraw()
    {
        m_BDEEGPS_RouteDrawCached = false;
        if (m_BDEEGPS_RouteCanvas)
            m_BDEEGPS_RouteCanvas.Clear();
    }

    protected void BDEEGPS_UpdateRouteOverlay()
    {
        if (!m_MapWidget)
            return;

        BDE_EGPS_Manager mgr = BDE_EGPS_Manager.GetInstance();
        if (!mgr)
            return;

        BDE_EGPS_Route route = mgr.GetRoute();
        bool hasRouteNow = (route && route.HasWaypoints());

        if (!hasRouteNow)
        {
            if (m_BDEEGPS_HadRouteLastFrame)
            {
                if (m_BDEEGPS_RouteCanvas)
                    m_BDEEGPS_RouteCanvas.Clear();
                m_BDEEGPS_HadRouteLastFrame = false;
                m_BDEEGPS_RouteDrawCached = false;
            }
            return;
        }

        if (!m_BDEEGPS_RouteCanvas)
        {
            m_BDEEGPS_RouteCanvas = BDEEGPS_CreateRouteCanvas(m_MapWidget);
            if (!m_BDEEGPS_RouteCanvas)
                return;
            m_BDEEGPS_RouteDrawCached = false;
        }

        m_BDEEGPS_HadRouteLastFrame = true;

        array<vector> path = route.GetCurrentPathPositions();
        if (!path)
        {
            if (!m_BDEEGPS_EmptyPath)
                m_BDEEGPS_EmptyPath = new array<vector>;
            else
                m_BDEEGPS_EmptyPath.Clear();
            path = m_BDEEGPS_EmptyPath;
        }

        int currentTraversedIdx = route.GetTraversedNodeIdx();
        vector currentPlayerPos = vector.Zero;
        if (GetGame() && GetGame().GetPlayer())
            currentPlayerPos = GetGame().GetPlayer().GetPosition();

        float ox, oy;
        m_BDEEGPS_RouteCanvas.GetScreenPos(ox, oy);

        float canvasW;
        float canvasH;
        m_BDEEGPS_RouteCanvas.GetScreenSize(canvasW, canvasH);

        vector currentMapPos = m_MapWidget.GetMapPos();
        float currentMapScale = m_MapWidget.GetScale();
        int currentPathTicks = route.GetLastRecomputeTicks();

        bool routeDirty = !m_BDEEGPS_RouteDrawCached;
        if (!routeDirty && currentPathTicks != m_BDEEGPS_LastRouteTicks)
            routeDirty = true;
        if (!routeDirty && currentTraversedIdx != m_BDEEGPS_LastRouteIdx)
            routeDirty = true;
        if (!routeDirty && currentMapScale != m_BDEEGPS_LastRouteMapScale)
            routeDirty = true;
        if (!routeDirty && vector.Distance(currentMapPos, m_BDEEGPS_LastRouteMapPos) > 0.01)
            routeDirty = true;
        if (!routeDirty && vector.DistanceSq(currentPlayerPos, m_BDEEGPS_LastRoutePlayerPos) > 1.0)
            routeDirty = true;
        if (!routeDirty && (canvasW != m_BDEEGPS_LastRouteCanvasW || canvasH != m_BDEEGPS_LastRouteCanvasH))
            routeDirty = true;

        if (!routeDirty)
            return;

        m_BDEEGPS_RouteCanvas.Clear();

        BDE_EGPS_RouteDrawer.DrawFullRoute(
            m_MapWidget,
            m_BDEEGPS_RouteCanvas,
            path,
            currentTraversedIdx,
            currentPlayerPos,
            route.GetWaypoints(),
            route.GetSegmentEndIndices(),
            route.GetTailStartPath(),
            route.GetTailEndPath(),
            BDE_EGPS_DrawStyle.GetForBigMap(),
            ox, oy,
            canvasW, canvasH);

        m_BDEEGPS_LastRouteMapPos = currentMapPos;
        m_BDEEGPS_LastRouteMapScale = currentMapScale;
        m_BDEEGPS_LastRouteTicks = currentPathTicks;
        m_BDEEGPS_LastRouteIdx = currentTraversedIdx;
        m_BDEEGPS_LastRoutePlayerPos = currentPlayerPos;
        m_BDEEGPS_LastRouteCanvasW = canvasW;
        m_BDEEGPS_LastRouteCanvasH = canvasH;
        m_BDEEGPS_RouteDrawCached = true;
    }

    protected CanvasWidget BDEEGPS_CreateRouteCanvas(Widget parentWidget)
    {
        if (!GetGame() || !GetGame().GetWorkspace() || !parentWidget)
            return null;

        Widget root = GetGame().GetWorkspace().CreateWidgets("BDE_ExpansionGPSRoutes/gui/layouts/route_canvas.layout", parentWidget);
        CanvasWidget canvas = CanvasWidget.Cast(root);
        if (!canvas)
            canvas = CanvasWidget.Cast(BDE_EGPS_WidgetUtils.FindChildByName(root, "bdeegps_route_canvas"));

        if (canvas)
        {
            canvas.SetSort(397);
            canvas.Show(true);
            canvas.Clear();
        }
        else
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " ExpansionMapMenu: failed to create route canvas.");
        }

        return canvas;
    }
}
