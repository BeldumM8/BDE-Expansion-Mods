// Player route state: queued waypoints plus the last computed road path.
// Positions are cached so UI drawing does not query the graph every frame.

class BDE_EGPS_Route
{
    protected ref array<ref BDE_EGPS_Waypoint> m_Waypoints;

    // Concatenated RGC node ids for all waypoint segments.
    protected ref array<int>    m_CurrentPath;

    // Position cache matching m_CurrentPath.
    protected ref array<vector> m_CurrentPathPositions;

    protected int m_LastRecomputeTicks;

    // Last path node the player has already passed.
    protected int m_TraversedNodeIdx;

    // End index of each waypoint subsegment inside m_CurrentPath.
    protected ref array<int> m_SegmentEndIndices;

    // Optional DRIVABLE polylines for ROAD-mode off-road gaps.
    protected ref array<vector> m_TailStartPath;
    protected ref array<vector> m_TailEndPath;

    void BDE_EGPS_Route()
    {
        m_Waypoints            = new array<ref BDE_EGPS_Waypoint>;
        m_CurrentPath          = new array<int>;
        m_CurrentPathPositions = new array<vector>;
        m_SegmentEndIndices    = new array<int>;
        m_TailStartPath        = new array<vector>;
        m_TailEndPath          = new array<vector>;
        m_LastRecomputeTicks   = 0;
        m_TraversedNodeIdx     = 0;
    }

    int  Count()         { return m_Waypoints.Count(); }
    bool HasWaypoints()  { return m_Waypoints.Count() > 0; }
    int  GetLastRecomputeTicks() { return m_LastRecomputeTicks; }
    int  GetTraversedNodeIdx()   { return m_TraversedNodeIdx; }
    array<int> GetSegmentEndIndices() { return m_SegmentEndIndices; }

    bool ContainsMarker(string markerUid)
    {
        int n = m_Waypoints.Count();
        int i;
        BDE_EGPS_Waypoint wp;
        for (i = 0; i < n; i++)
        {
            wp = m_Waypoints.Get(i);
            if (wp && wp.GetUid() == markerUid)
                return true;
        }
        return false;
    }

    array<ref BDE_EGPS_Waypoint> GetWaypoints()      { return m_Waypoints; }
    array<int>                  GetCurrentPath()    { return m_CurrentPath; }
    array<vector>               GetCurrentPathPositions() { return m_CurrentPathPositions; }
    array<vector>               GetTailStartPath()  { return m_TailStartPath; }
    array<vector>               GetTailEndPath()    { return m_TailEndPath; }

    bool AddWaypoint(BDE_EGPS_Waypoint wp)
    {
        if (!wp) return false;
        if (ContainsMarker(wp.GetUid()))
        {
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Route.AddWaypoint: marker uid %1 already in route, ignoring.",
                wp.GetUid()));
            return false;
        }
        int maxW = BDE_EGPS_Constants.GetMaxWaypoints();
        if (m_Waypoints.Count() >= maxW)
        {
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Route.AddWaypoint: max waypoints (%1) reached.", maxW));
            return false;
        }
        m_Waypoints.Insert(wp);
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Route.AddWaypoint: %1 (now %2 waypoints).",
            wp.ToDebugString(), m_Waypoints.Count()));
        return true;
    }

    bool RemoveWaypointByUid(string markerUid)
    {
        int n = m_Waypoints.Count();
        int i;
        BDE_EGPS_Waypoint wp;
        for (i = 0; i < n; i++)
        {
            wp = m_Waypoints.Get(i);
            if (wp && wp.GetUid() == markerUid)
            {
                m_Waypoints.Remove(i);
                Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Route.RemoveWaypointByUid: removed marker %1 (now %2 waypoints).",
                    markerUid, m_Waypoints.Count()));
                return true;
            }
        }
        return false;
    }

    void Clear()
    {
        m_Waypoints.Clear();
        m_CurrentPath.Clear();
        m_CurrentPathPositions.Clear();
        m_SegmentEndIndices.Clear();
        m_TailStartPath.Clear();
        m_TailEndPath.Clear();
        m_TraversedNodeIdx = 0;

        // Let overlays notice that the route disappeared.
        m_LastRecomputeTicks = TickCount(0);
        Print(BDE_EGPS_Constants.LOG_TAG + " Route.Clear");
    }

    // Advances while the next path node is closer than the current one.
    void AdvanceTraversedIdx(vector playerPos)
    {
        int n = m_CurrentPathPositions.Count();
        if (n < 2) return;

        int idx = m_TraversedNodeIdx;
        if (idx >= n - 1) return;  // already on the last node

        while (idx < n - 1)
        {
            float distCur  = vector.DistanceSq(playerPos, m_CurrentPathPositions.Get(idx));
            float distNext = vector.DistanceSq(playerPos, m_CurrentPathPositions.Get(idx + 1));
            if (distNext < distCur)
                idx++;
            else
                break;
        }

        m_TraversedNodeIdx = idx;
    }

    // Full recompute: A* per waypoint segment, then cache node positions.
    bool Recompute(RGC_Graph graph, vector startPos, RGC_DrivabilityMode mode)
    {
        m_CurrentPath.Clear();
        m_CurrentPathPositions.Clear();
        m_SegmentEndIndices.Clear();
        m_TailStartPath.Clear();
        m_TailEndPath.Clear();
        m_TraversedNodeIdx = 0;  // new route -> trail starts from the beginning

        // Update ticks even on failure so overlays repaint dashed fallbacks.
        int t0 = TickCount(0);
        m_LastRecomputeTicks = t0;

        if (!graph)
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " Route.Recompute: graph is null.");
            return false;
        }
        if (m_Waypoints.Count() == 0)
        {
            return true;  // empty path is valid
        }
        vector segStart = startPos;
        int n = m_Waypoints.Count();
        int i;
        int j;
        int subCount;
        int startIdx;
        BDE_EGPS_Waypoint wp;
        array<int> subPath;

        for (i = 0; i < n; i++)
        {
            wp = m_Waypoints.Get(i);
            if (!wp)
            {
                Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Route.Recompute: waypoint %1 is null, abort.", i));
                m_CurrentPath.Clear();
                m_CurrentPathPositions.Clear();
                m_SegmentEndIndices.Clear();
                return false;
            }
            subPath = BDE_EGPS_Pathfinder.FindRoute(graph, segStart, wp.GetPos(), mode);
            if (!subPath || subPath.Count() == 0)
            {
                Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Route.Recompute: NO ROUTE for segment %1 (to wp uid=%2 at %3).",
                    i, wp.GetUid(), wp.GetPos().ToString()));
                m_CurrentPath.Clear();
                m_CurrentPathPositions.Clear();
                m_SegmentEndIndices.Clear();
                return false;
            }
            subCount = subPath.Count();
            startIdx = 0;
            if (i > 0) startIdx = 1;  // skip the duplicate of the previous segment's last node
            for (j = startIdx; j < subCount; j++)
            {
                m_CurrentPath.Insert(subPath.Get(j));
            }

            m_SegmentEndIndices.Insert(m_CurrentPath.Count() - 1);
            segStart = wp.GetPos();
        }

        int pathN = m_CurrentPath.Count();
        int k;
        int nodeId;
        RGC_Node node;
        for (k = 0; k < pathN; k++)
        {
            nodeId = m_CurrentPath.Get(k);
            node = graph.GetNodeById(nodeId);
            if (node)
                m_CurrentPathPositions.Insert(node.GetPos());
            else
                m_CurrentPathPositions.Insert(vector.Zero);  // safety
        }

        if (mode == RGC_DrivabilityMode.ROAD && pathN > 0)
        {
            vector firstRoadPos = m_CurrentPathPositions.Get(0);
            if (vector.DistanceSq(startPos, firstRoadPos) > 2500.0)
                ComputeTailPath(graph, startPos, firstRoadPos, m_TailStartPath);

            vector lastRoadPos = m_CurrentPathPositions.Get(pathN - 1);
            BDE_EGPS_Waypoint lastWp = m_Waypoints.Get(m_Waypoints.Count() - 1);
            if (lastWp && vector.DistanceSq(lastRoadPos, lastWp.GetPos()) > 2500.0)
                ComputeTailPath(graph, lastRoadPos, lastWp.GetPos(), m_TailEndPath);
        }

        int elapsed = TickCount(0) - t0;
        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Route.Recompute OK: %1 waypoints, %2 nodes path, %3 ticks (%4ms). Tails: start=%5 end=%6.",
            n, pathN, elapsed, elapsed / 10000,
            m_TailStartPath.Count(), m_TailEndPath.Count()));
        return true;
    }

    // Builds a DRIVABLE tail polyline bracketed by the real source/target.
    protected void ComputeTailPath(RGC_Graph graph, vector from, vector to, array<vector> outArr)
    {
        outArr.Clear();
        array<int> nodeIds = BDE_EGPS_Pathfinder.FindRoute(graph, from, to, RGC_DrivabilityMode.DRIVABLE);
        if (!nodeIds || nodeIds.Count() < 1) return;

        outArr.Insert(from);

        int n = nodeIds.Count();
        int i;
        int nodeId;
        RGC_Node node;
        for (i = 0; i < n; i++)
        {
            nodeId = nodeIds.Get(i);
            node = graph.GetNodeById(nodeId);
            if (node) outArr.Insert(node.GetPos());
        }

        outArr.Insert(to);
    }
}
