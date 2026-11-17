// A* helper heap backed by parallel node/priority arrays.

class BDE_EGPS_MinHeap
{
    protected ref array<int>   m_NodeIds;
    protected ref array<float> m_Priorities;
    protected int              m_Size;

    void BDE_EGPS_MinHeap()
    {
        m_NodeIds    = new array<int>;
        m_Priorities = new array<float>;
        m_Size       = 0;
    }

    int  Count()   { return m_Size; }
    bool IsEmpty() { return m_Size == 0; }

    void Clear()
    {
        m_NodeIds.Clear();
        m_Priorities.Clear();
        m_Size = 0;
    }

    // Inserts or reuses one heap slot.
    void Push(int nodeId, float priority)
    {

        if (m_Size < m_NodeIds.Count())
        {
            m_NodeIds.Set(m_Size, nodeId);
            m_Priorities.Set(m_Size, priority);
        }
        else
        {
            m_NodeIds.Insert(nodeId);
            m_Priorities.Insert(priority);
        }
        SiftUp(m_Size);
        m_Size++;
    }

    // Returns the node id with the smallest priority, or -1 when empty.
    int PopMin()
    {
        if (m_Size == 0) return -1;
        int rootId = m_NodeIds.Get(0);
        m_Size--;
        if (m_Size > 0)
        {

            m_NodeIds.Set(0, m_NodeIds.Get(m_Size));
            m_Priorities.Set(0, m_Priorities.Get(m_Size));
            SiftDown(0);
        }
        return rootId;
    }

    // Bubble the item at idx upwards.
    protected void SiftUp(int idx)
    {

        int   orig_id  = m_NodeIds.Get(idx);
        float orig_pri = m_Priorities.Get(idx);
        int   parent;
        float pri_parent;

        while (idx > 0)
        {
            parent = (idx - 1) / 2;
            pri_parent = m_Priorities.Get(parent);
            if (orig_pri < pri_parent)
            {

                m_NodeIds.Set(idx, m_NodeIds.Get(parent));
                m_Priorities.Set(idx, pri_parent);
                idx = parent;
            }
            else
            {
                break;  // hole found its place
            }
        }

        m_NodeIds.Set(idx, orig_id);
        m_Priorities.Set(idx, orig_pri);
    }

    // Bubble the item at idx downwards.
    protected void SiftDown(int idx)
    {
        int   orig_id  = m_NodeIds.Get(idx);
        float orig_pri = m_Priorities.Get(idx);
        int   left;
        int   right;
        int   smallest;
        float pri_smallest;
        float pri_left;
        float pri_right;

        while (true)
        {
            left  = 2 * idx + 1;
            right = left + 1;
            smallest = idx;
            pri_smallest = orig_pri;  // current "hole" priority (the value being compared)

            if (left < m_Size)
            {
                pri_left = m_Priorities.Get(left);
                if (pri_left < pri_smallest)
                {
                    smallest = left;
                    pri_smallest = pri_left;
                }
            }
            if (right < m_Size)
            {
                pri_right = m_Priorities.Get(right);
                if (pri_right < pri_smallest)
                {
                    smallest = right;
                    pri_smallest = pri_right;
                }
            }

            if (smallest == idx)
                break;  // hole found its place

            m_NodeIds.Set(idx, m_NodeIds.Get(smallest));
            m_Priorities.Set(idx, pri_smallest);
            idx = smallest;
        }

        m_NodeIds.Set(idx, orig_id);
        m_Priorities.Set(idx, orig_pri);
    }

    // Development validator for the heap.
    static bool RunSelfTest()
    {
        BDE_EGPS_MinHeap h = new BDE_EGPS_MinHeap();

        if (!h.IsEmpty())                  { Print("[heap test FAIL] !IsEmpty initial"); return false; }
        if (h.Count() != 0)                { Print("[heap test FAIL] Count != 0 initial"); return false; }
        if (h.PopMin() != -1)              { Print("[heap test FAIL] PopMin on empty != -1"); return false; }

        h.Push(42, 5.0);
        if (h.Count() != 1)                { Print("[heap test FAIL] Count after 1 push"); return false; }
        if (h.PopMin() != 42)              { Print("[heap test FAIL] PopMin single"); return false; }
        if (h.Count() != 0)                { Print("[heap test FAIL] Count after pop"); return false; }

        h.Push(100, 7.0);
        h.Push(101, 3.0);
        h.Push(102, 1.0);
        h.Push(103, 9.0);
        h.Push(104, 5.0);
        h.Push(105, 2.0);
        h.Push(106, 8.0);
        h.Push(107, 4.0);
        if (h.Count() != 8) { Print("[heap test FAIL] Count after 8 pushes"); return false; }

        array<int> expected = new array<int>;
        expected.Insert(102); expected.Insert(105); expected.Insert(101); expected.Insert(107);
        expected.Insert(104); expected.Insert(100); expected.Insert(106); expected.Insert(103);
        int i;
        int got;
        int exp;
        for (i = 0; i < 8; i++)
        {
            got = h.PopMin();
            exp = expected.Get(i);
            if (got != exp)
            {
                Print(string.Format("[heap test FAIL] PopMin step %1 expected %2 got %3", i, exp, got));
                return false;
            }
        }
        if (!h.IsEmpty()) { Print("[heap test FAIL] not empty after all pops"); return false; }

        h.Push(200, 10.0);
        h.Push(201, 1.0);
        if (h.PopMin() != 201) { Print("[heap test FAIL] reuse 1"); return false; }
        if (h.PopMin() != 200) { Print("[heap test FAIL] reuse 2"); return false; }

        h.Push(300, 5.0);
        h.Push(301, 5.0);
        h.Push(302, 5.0);
        int popped = 0;
        while (!h.IsEmpty()) { h.PopMin(); popped++; }
        if (popped != 3) { Print("[heap test FAIL] equal priorities"); return false; }

        Print(BDE_EGPS_Constants.LOG_TAG + " [heap test] OK — all 5 cases passed");
        return true;
    }
}

// A* pathfinder over RGC_Graph, with bridge handling for disconnected
// components in the active drivability mode.

class BDE_EGPS_Pathfinder
{
    static const int   MAX_ITERATIONS = 100000;
    static const float G_INFINITY     = 100000000.0;

    static const bool  DEBUG_TRACE    = false;

    // Returns a node id path, or null when the drawer should use dashed fallback.
    static array<int> FindRoute(RGC_Graph graph, vector startPos, vector goalPos, RGC_DrivabilityMode mode)
    {
        if (!graph)
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " Pathfinder: graph is null.");
            return null;
        }

        RGC_Node startNode = BDE_EGPS_GraphComponents.SnapToNearestRoad(graph, startPos);
        if (!startNode)
        {
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Pathfinder: graph empty (no nodes), start %1.", startPos.ToString()));
            return null;
        }
        RGC_Node goalNode = BDE_EGPS_GraphComponents.SnapToNearestRoad(graph, goalPos);
        if (!goalNode)
        {
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Pathfinder: graph empty (no nodes), goal %1.", goalPos.ToString()));
            return null;
        }

        int startId = startNode.GetId();
        int goalId  = goalNode.GetId();

        if (startId == goalId)
        {
            array<int> trivial = new array<int>;
            trivial.Insert(startId);
            return trivial;
        }

        // Bridge into the main component when the snapped endpoints are disconnected.
        if (BDE_EGPS_GraphComponents.IsBuilt())
        {
            int startComp = BDE_EGPS_GraphComponents.GetComponentId(startId, mode);
            int goalComp  = BDE_EGPS_GraphComponents.GetComponentId(goalId, mode);
            if (startComp != -1 && goalComp != -1 && startComp != goalComp)
            {
                int   mainCompId   = BDE_EGPS_GraphComponents.GetMainComponentId(mode);
                float bridgeRadius = BDE_EGPS_Constants.GetBridgeSearchRadiusM();
                bool  startInMain  = (startComp == mainCompId);
                bool  goalInMain   = (goalComp  == mainCompId);

                // Keep existing endpoints unless a side needs bridging.
                RGC_Node bridgeStart = startNode;
                RGC_Node bridgeGoal  = goalNode;

                if (!startInMain)
                {
                    bridgeStart = BDE_EGPS_GraphComponents.FindNearestNodeInComponent(
                        graph, startPos, mainCompId, mode, bridgeRadius);
                }

                // Waypoint-side bridging is unbounded; the dashed tail covers the gap.
                if (!goalInMain)
                {
                    bridgeGoal = BDE_EGPS_GraphComponents.FindNearestNodeInComponent(
                        graph, goalPos, mainCompId, mode, 99999.0);
                }

                if (!bridgeStart || !bridgeGoal)
                {
                    int hasStartBridge = 0;
                    int hasGoalBridge  = 0;
                    if (bridgeStart) hasStartBridge = 1;
                    if (bridgeGoal)  hasGoalBridge  = 1;
                    string nbrMsg = string.Format(
                        " Pathfinder: bridge fallback FAILED — start (node %1, comp %2 / %3 nodes) goal (node %4, comp %5 / %6 nodes), main=#%7, hasBridgeStart=%8 hasBridgeGoal=%9. Fallback dashed.",
                        startId, startComp, BDE_EGPS_GraphComponents.GetComponentSize(startComp, mode),
                        goalId, goalComp, BDE_EGPS_GraphComponents.GetComponentSize(goalComp, mode),
                        mainCompId, hasStartBridge, hasGoalBridge);
                    Print(BDE_EGPS_Constants.LOG_TAG + nbrMsg);
                    return null;
                }

                int newStartId = bridgeStart.GetId();
                int newGoalId  = bridgeGoal.GetId();

                if (newStartId == newGoalId)
                {
                    string degMsg = string.Format(
                        " Pathfinder: bridge fallback degenerate — both sides land on same main node %1. Fallback dashed.",
                        newStartId);
                    Print(BDE_EGPS_Constants.LOG_TAG + degMsg);
                    return null;
                }

                float distPlayerToBridgeStart = vector.Distance(startPos, bridgeStart.GetPos());
                float distBridgeGoalToWp      = vector.Distance(bridgeGoal.GetPos(), goalPos);
                int oldStartId = startId;
                int oldGoalId  = goalId;
                startNode = bridgeStart;
                startId   = newStartId;
                goalNode  = bridgeGoal;
                goalId    = newGoalId;
                int reassignedStart = 0;
                int reassignedGoal  = 0;
                if (newStartId != oldStartId) reassignedStart = 1;
                if (newGoalId  != oldGoalId)  reassignedGoal  = 1;
                string brMsg = string.Format(
                    " Pathfinder: bridge fallback OK in main comp #%1 — start %2->%3 (reassigned=%4), goal %5->%6 (reassigned=%7). Tails: player->bridgeStart=%8m, bridgeGoal->wp=%9m.",
                    mainCompId, oldStartId, newStartId, reassignedStart,
                    oldGoalId, newGoalId, reassignedGoal,
                    distPlayerToBridgeStart, distBridgeGoalToWp);
                Print(BDE_EGPS_Constants.LOG_TAG + brMsg);

            }
        }

        vector goalPosCached = goalNode.GetPos();

        // A* state.
        if (DEBUG_TRACE)
            Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Pathfinder TRACE: startId=%1 goalId=%2 mode=%3",
                startId, goalId, mode));

        BDE_EGPS_MinHeap heap     = new BDE_EGPS_MinHeap();
        map<int, float> gScore   = new map<int, float>;
        map<int, int>   cameFrom = new map<int, int>;

        map<int, int>   closed   = new map<int, int>;

        gScore.Set(startId, 0.0);
        float h0 = vector.Distance(startNode.GetPos(), goalPosCached);
        heap.Push(startId, h0);

        int    iter = 0;
        int    current;
        int    closedFlag;
        bool   isClosed;
        int    t;
        int    transN;
        int    neighbour;
        float  currentG;
        float  tentative;
        float  existingG;
        float  newH;
        RGC_Transition tr;
        RGC_Node       nbNode;
        array<ref RGC_Transition> trans;

        while (!heap.IsEmpty())
        {
            iter++;
            if (iter > MAX_ITERATIONS)
            {
                Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Pathfinder: exceeded MAX_ITERATIONS (%1), aborting.", MAX_ITERATIONS));
                return null;
            }

            current = heap.PopMin();

            closedFlag = 0;
            isClosed = closed.Find(current, closedFlag);
            if (isClosed) continue;
            closed.Set(current, 1);

            if (current == goalId)
            {
                array<int> path = new array<int>;
                int node = current;
                path.InsertAt(node, 0);
                int from;
                while (cameFrom.Find(node, from))
                {
                    path.InsertAt(from, 0);
                    node = from;
                }
                Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Pathfinder: route found in %1 expansions, %2 nodes.",
                    iter, path.Count()));
                return path;
            }

            trans = graph.GetTransitions(current, mode);
            if (!trans) continue;
            transN = trans.Count();

            currentG = gScore.Get(current);

            for (t = 0; t < transN; t++)
            {
                tr = trans.Get(t);
                if (!tr) continue;
                neighbour = tr.GetTargetNodeId();

                closedFlag = 0;
                if (closed.Find(neighbour, closedFlag)) continue;

                tentative = currentG + tr.GetLengthM();

                if (gScore.Contains(neighbour))
                    existingG = gScore.Get(neighbour);
                else
                    existingG = G_INFINITY;

                if (tentative < existingG)
                {
                    gScore.Set(neighbour, tentative);
                    cameFrom.Set(neighbour, current);

                    nbNode = graph.GetNodeById(neighbour);
                    if (!nbNode) continue;
                    newH = vector.Distance(nbNode.GetPos(), goalPosCached);
                    heap.Push(neighbour, tentative + newH);
                }
            }
        }

        Print(string.Format(BDE_EGPS_Constants.LOG_TAG + " Pathfinder: no route found after %1 expansions.", iter));
        return null;
    }

}
