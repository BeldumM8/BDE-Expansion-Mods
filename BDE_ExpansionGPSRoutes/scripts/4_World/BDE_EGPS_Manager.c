// Singleton entry point for graph access, pathing mode and the active route.

class BDE_EGPS_Manager
{
    static const bool RUN_DEV_TESTS = false;

    private static ref BDE_EGPS_Manager s_Instance;

    protected RGC_Graph m_Graph;
    protected bool m_Initialized;
    protected RGC_DrivabilityMode m_CurrentMode;
    protected ref BDE_EGPS_Route m_Route;
    protected int m_LastRerouteTicks;
    protected int m_LastInitializationAttemptTicks;
    protected int m_InitializationAttempts;

    static BDE_EGPS_Manager GetInstance()
    {
        if (!s_Instance)
            s_Instance = new BDE_EGPS_Manager();
        return s_Instance;
    }

    static void SafeCleanup()
    {
        if (s_Instance)
        {
            s_Instance.Cleanup();
            s_Instance = null;
        }
    }

    void BDE_EGPS_Manager()
    {
        m_Initialized = false;
        m_Graph = null;
        m_CurrentMode = RGC_DrivabilityMode.DRIVABLE;
        m_Route = new BDE_EGPS_Route();
        m_LastRerouteTicks = 0;
        m_LastInitializationAttemptTicks = 0;
        m_InitializationAttempts = 0;
    }

    void Init()
    {
        EnsureReady(true);
    }

    bool EnsureReady(bool forceRetry = false)
    {
        if (IsReady())
            return true;

        int now = TickCount(0);
        if (!forceRetry && m_LastInitializationAttemptTicks != 0)
        {
            int elapsedMs = (now - m_LastInitializationAttemptTicks) / 10000;
            if (elapsedMs < 2000)
                return false;
        }

        m_LastInitializationAttemptTicks = now;
        m_InitializationAttempts++;
        m_Initialized = false;
        m_Graph = null;

        Print(BDE_EGPS_Constants.LOG_TAG + " ======================================");
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Initializing GPS manager attempt %1", m_InitializationAttempts));

        RGC_Manager rgc = RGC_Manager.GetInstance();
        if (!rgc)
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " ERROR: RGC_Manager singleton is null. RoadGraph_Core is a required dependency.");
            return false;
        }

        string world = "<unknown>";
        if (GetGame())
            world = GetGame().GetWorldName();

        m_Graph = rgc.GetGraph();
        if (!m_Graph)
        {
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Graph is not ready for world '%1'; initialization will retry.", world));
            // RoadGraph_Core reports load/schema errors directly with [RGC].
            Print(BDE_EGPS_Constants.LOG_TAG + " See preceding [RGC] log entries for graph loading errors.");

            array<string> available = rgc.ListAvailableMaps();
            string availStr = "<empty>";
            if (available && available.Count() > 0)
            {
                availStr = "";
                int availableCount = available.Count();
                for (int availableIndex = 0; availableIndex < availableCount; availableIndex++)
                {
                    if (availableIndex > 0)
                        availStr = availStr + ", ";
                    availStr = availStr + available[availableIndex];
                }
            }
            Print(BDE_EGPS_Constants.LOG_TAG + " Available maps in RoadGraph_Core: " + availStr);
            Print(BDE_EGPS_Constants.LOG_TAG + " ======================================");
            return false;
        }

        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Manager initialized for world '%1'. Graph: %2 (%3 nodes, %4 edges).", world, m_Graph.GetMapName(), m_Graph.GetNodes().Count(), m_Graph.GetEdges().Count()));
        BDE_EGPS_GraphComponents.Build(m_Graph);
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Initial pathing mode: %1.", GetModeLabel()));
        Print(BDE_EGPS_Constants.LOG_TAG + " ======================================");

        m_Initialized = true;

        if (RUN_DEV_TESTS)
        {
            BDE_EGPS_MinHeap.RunSelfTest();
            RunM3aTest();
        }

        return true;
    }

    void Cleanup()
    {
        Print(BDE_EGPS_Constants.LOG_TAG + " Manager Cleanup().");
        if (m_Route)
            m_Route.Clear();
        m_Route = null;
        m_Graph = null;
        m_Initialized = false;
        m_LastInitializationAttemptTicks = 0;
        m_InitializationAttempts = 0;
        BDE_EGPS_GraphComponents.Invalidate();
    }

    RGC_Graph GetGraph() { return m_Graph; }
    bool IsReady() { return m_Initialized && m_Graph != null; }
    BDE_EGPS_Route GetRoute() { return m_Route; }

    bool AddMarkerToRoute(ExpansionMarkerData markerData)
    {
        if (!EnsureReady(true) || !m_Route || !markerData)
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " AddMarkerToRoute rejected because the graph, route, or marker is unavailable.");
            return false;
        }

        string uid = markerData.GetUID();
        if (uid == string.Empty)
            uid = MakeSyntheticWaypointUid(markerData.GetPosition(), markerData.GetName());

        BDE_EGPS_Waypoint waypoint = new BDE_EGPS_Waypoint(uid, markerData.GetType(), markerData.GetPosition(), markerData.GetName());
        if (!m_Route.AddWaypoint(waypoint))
            return false;

        if (!RecomputeFromPlayer())
        {
            m_Route.RemoveWaypointByUid(uid);
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " AddMarkerToRoute failed to calculate a route to marker '%1'.", markerData.GetName()));
            return false;
        }

        return true;
    }

    bool AddRoutePoint(vector pos, string name = "Route Point")
    {
        if (!EnsureReady(true) || !m_Route)
            return false;

        string uid = MakeSyntheticWaypointUid(pos, name);
        BDE_EGPS_Waypoint waypoint = new BDE_EGPS_Waypoint(uid, -1, pos, name);
        if (!m_Route.AddWaypoint(waypoint))
            return false;

        if (!RecomputeFromPlayer())
        {
            m_Route.RemoveWaypointByUid(uid);
            return false;
        }

        return true;
    }

    bool RemoveMarkerFromRoute(string markerUid)
    {
        if (!EnsureReady(true) || !m_Route)
            return false;

        bool removed = m_Route.RemoveWaypointByUid(markerUid);
        if (removed)
            RecomputeFromPlayer();
        return removed;
    }

    bool RemoveRoutePoint(vector pos, string name = "Route Point")
    {
        return RemoveMarkerFromRoute(MakeSyntheticWaypointUid(pos, name));
    }

    bool IsMarkerInRoute(string markerUid)
    {
        if (!m_Route)
            return false;
        return m_Route.ContainsMarker(markerUid);
    }

    bool IsRoutePointInRoute(vector pos, string name = "Route Point")
    {
        return IsMarkerInRoute(MakeSyntheticWaypointUid(pos, name));
    }

    protected string MakeSyntheticWaypointUid(vector pos, string name)
    {
        return string.Format("BDE_ROUTE_%1_%2_%3_%4", name, Math.Round(pos[0]), Math.Round(pos[1]), Math.Round(pos[2]));
    }

    void ClearRoute()
    {
        if (!m_Route || !m_Route.HasWaypoints())
            return;
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Manager.ClearRoute: removing all %1 waypoints.", m_Route.Count()));
        m_Route.Clear();
    }

    bool RecomputeFromPlayer()
    {
        if (!m_Route || !m_Graph)
            return false;

        vector startPos = vector.Zero;
        if (GetGame() && GetGame().GetPlayer())
            startPos = GetGame().GetPlayer().GetPosition();
        else
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " RecomputeFromPlayer: local player is not available.");
            return false;
        }

        return m_Route.Recompute(m_Graph, startPos, m_CurrentMode);
    }

    void OnTickReroute(vector playerPos)
    {
        if (!IsReady() || !m_Route || !m_Route.HasWaypoints())
            return;

        array<vector> path = m_Route.GetCurrentPathPositions();
        if (!path || path.Count() == 0)
            return;

        float distToStart = vector.Distance(playerPos, path.Get(0));
        if (distToStart < BDE_EGPS_Constants.GetRerouteFullThresholdM())
            return;

        int now = TickCount(0);
        int elapsedMs = (now - m_LastRerouteTicks) / 10000;
        if (m_LastRerouteTicks != 0 && elapsedMs < BDE_EGPS_Constants.GetRerouteCooldownMs())
            return;

        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " OnTickReroute: dist=%1m > threshold, recomputing.", distToStart));
        m_LastRerouteTicks = now;
        RecomputeFromPlayer();
    }

    // Pathing mode

    RGC_DrivabilityMode GetMode() { return m_CurrentMode; }

    bool SetMode(RGC_DrivabilityMode newMode)
    {
        if (newMode != RGC_DrivabilityMode.DRIVABLE && newMode != RGC_DrivabilityMode.ROAD)
        {
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " SetMode: invalid mode %1, ignoring.", newMode));
            return false;
        }
        if (newMode == m_CurrentMode)
            return false;
        m_CurrentMode = newMode;
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Pathing mode changed to %1.", GetModeLabel()));

        if (m_Route && m_Route.HasWaypoints())
            RecomputeFromPlayer();
        return true;
    }

    RGC_DrivabilityMode ToggleMode()
    {
        RGC_DrivabilityMode next;
        if (m_CurrentMode == RGC_DrivabilityMode.DRIVABLE)
            next = RGC_DrivabilityMode.ROAD;
        else
            next = RGC_DrivabilityMode.DRIVABLE;
        SetMode(next);
        return m_CurrentMode;
    }

    string GetModeLabel()
    {
        if (m_CurrentMode == RGC_DrivabilityMode.ROAD)
            return BDE_EGPS_Constants.MODE_LABEL_ROAD;
        return BDE_EGPS_Constants.MODE_LABEL_DRIVABLE;
    }

    // Development-only smoke test for the route data layer.
    protected void RunM3aTest()
    {
        if (!m_Graph) return;
        if (m_Graph.GetMapName() != "chernarusplus")
        {
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " M3a test: skipping (only chernarusplus, got '%1').",
                m_Graph.GetMapName()));
            return;
        }

        Print(BDE_EGPS_Constants.LOG_TAG + " ======================================");
        Print(BDE_EGPS_Constants.LOG_TAG + " M3a test: data layer (Route + Waypoint)");

        vector startPos = Vector(14040.611, 12.398, 13208.730);  // NE
        vector wp1Pos   = Vector(10000.0,    0.0,  10000.0);     // chernarus centre
        vector wp2Pos   = Vector(2083.178,  188.155, 5343.495);  // Pavlovo

        BDE_EGPS_Waypoint wp1 = new BDE_EGPS_Waypoint("1001", 5 /* PRIVATE_MARKER */, wp1Pos, "TestWP_Center");
        BDE_EGPS_Waypoint wp2 = new BDE_EGPS_Waypoint("1002", 5 /* PRIVATE_MARKER */, wp2Pos, "TestWP_Pavlovo");
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + "  Adding %1", wp1.ToDebugString()));
        m_Route.AddWaypoint(wp1);
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + "  Adding %1", wp2.ToDebugString()));
        m_Route.AddWaypoint(wp2);

        Print(BDE_EGPS_Constants.LOG_TAG + "  Recompute from NE start...");
        bool ok = m_Route.Recompute(m_Graph, startPos, m_CurrentMode);
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + "  Recompute OK? %1, path nodes=%2",
            ok, m_Route.GetCurrentPath().Count()));

        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + "  ContainsMarker(1001)? %1", m_Route.ContainsMarker("1001")));
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + "  ContainsMarker(9999)? %1", m_Route.ContainsMarker("9999")));
        Print(BDE_EGPS_Constants.LOG_TAG + "  Removing waypoint uid=1001...");
        m_Route.RemoveWaypointByUid("1001");
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + "  Now %1 waypoints.", m_Route.Count()));

        Print(BDE_EGPS_Constants.LOG_TAG + "  Recompute again (1 waypoint left)...");
        m_Route.Recompute(m_Graph, startPos, m_CurrentMode);
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + "  path nodes after re-recompute=%1", m_Route.GetCurrentPath().Count()));

        Print(BDE_EGPS_Constants.LOG_TAG + "  Clearing route...");
        m_Route.Clear();
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + "  Count after clear: %1, HasWaypoints: %2",
            m_Route.Count(), m_Route.HasWaypoints()));

        Print(BDE_EGPS_Constants.LOG_TAG + " M3a test done.");
        Print(BDE_EGPS_Constants.LOG_TAG + " ======================================");
    }
}
