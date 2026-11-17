// Connected-component tables for ANY, DRIVABLE and ROAD.
// Pathfinder uses these to detect disconnected starts/goals before running A*.

class BDE_EGPS_GraphComponents
{

    // ANY mode: full graph connectivity.
    protected static ref map<int, int> s_NodeToCompANY;
    protected static ref map<int, int> s_ComponentSizeANY;
    protected static int s_MainCompANY;
    protected static int s_TotalCompsANY;

    // DRIVABLE mode: roads, gravel and dirt.
    protected static ref map<int, int> s_NodeToCompDRIVABLE;
    protected static ref map<int, int> s_ComponentSizeDRIVABLE;
    protected static int s_MainCompDRIVABLE;
    protected static int s_TotalCompsDRIVABLE;

    // ROAD mode: strict road network.
    protected static ref map<int, int> s_NodeToCompROAD;
    protected static ref map<int, int> s_ComponentSizeROAD;
    protected static int s_MainCompROAD;
    protected static int s_TotalCompsROAD;

    // Shared graph state.
    protected static int s_TotalNodes;

    protected static bool s_Built;

    // Builds the three component maps from the current RGC graph.
    static void Build(RGC_Graph graph)
    {
        Invalidate();

        if (!graph)
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " GraphComponents.Build: graph is null, skipping.");
            return;
        }

        array<ref RGC_Node> nodes = graph.GetNodes();
        if (!nodes)
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " GraphComponents.Build: graph.GetNodes() is null, skipping.");
            return;
        }
        s_TotalNodes = nodes.Count();
        if (s_TotalNodes == 0)
        {
            Print(BDE_EGPS_Constants.LOG_TAG + " GraphComponents.Build: graph has 0 nodes, skipping.");
            s_Built = true;
            return;
        }

        int t0 = TickCount(0);

        // BFS per mode.
        int mainAny, mainDrv, mainRoad;
        int totalAny, totalDrv, totalRoad;

        s_NodeToCompANY      = new map<int, int>;
        s_ComponentSizeANY   = new map<int, int>;
        BuildForMode(graph, RGC_DrivabilityMode.ANY, s_NodeToCompANY, s_ComponentSizeANY, mainAny, totalAny);
        s_MainCompANY      = mainAny;
        s_TotalCompsANY    = totalAny;

        s_NodeToCompDRIVABLE    = new map<int, int>;
        s_ComponentSizeDRIVABLE = new map<int, int>;
        BuildForMode(graph, RGC_DrivabilityMode.DRIVABLE, s_NodeToCompDRIVABLE, s_ComponentSizeDRIVABLE, mainDrv, totalDrv);
        s_MainCompDRIVABLE   = mainDrv;
        s_TotalCompsDRIVABLE = totalDrv;

        s_NodeToCompROAD    = new map<int, int>;
        s_ComponentSizeROAD = new map<int, int>;
        BuildForMode(graph, RGC_DrivabilityMode.ROAD, s_NodeToCompROAD, s_ComponentSizeROAD, mainRoad, totalRoad);
        s_MainCompROAD   = mainRoad;
        s_TotalCompsROAD = totalRoad;

        s_Built = true;

        int t1 = TickCount(0);
        int elapsedMs = (t1 - t0) / 10000;

        // Startup diagnostics.
        int mainSizeAny = 0;
        if (s_ComponentSizeANY && s_ComponentSizeANY.Contains(mainAny))
            mainSizeAny = s_ComponentSizeANY.Get(mainAny);
        int mainSizeDrv = 0;
        if (s_ComponentSizeDRIVABLE && s_ComponentSizeDRIVABLE.Contains(mainDrv))
            mainSizeDrv = s_ComponentSizeDRIVABLE.Get(mainDrv);
        int mainSizeRoad = 0;
        if (s_ComponentSizeROAD && s_ComponentSizeROAD.Contains(mainRoad))
            mainSizeRoad = s_ComponentSizeROAD.Get(mainRoad);
        int pctAny  = 0;
        int pctDrv  = 0;
        int pctRoad = 0;
        if (s_TotalNodes > 0)
        {
            pctAny  = (mainSizeAny  * 100) / s_TotalNodes;
            pctDrv  = (mainSizeDrv  * 100) / s_TotalNodes;
            pctRoad = (mainSizeRoad * 100) / s_TotalNodes;
        }

        string compMsg = string.Format(
            " GraphComponents built in %1ms (per-mode v3): ANY=%2 comps main=#%3 (%4 nodes %5 pct), DRIVABLE=%6 comps main=#%7 (%8 nodes), ROAD see next line.",
            elapsedMs, totalAny, mainAny, mainSizeAny, pctAny, totalDrv, mainDrv, mainSizeDrv);
        Print(BDE_EGPS_Constants.LOG_TAG + compMsg);
        string roadMsg = string.Format(
            " GraphComponents ROAD: %1 comps main=#%2 (%3 nodes %4 pct of %5).",
            totalRoad, mainRoad, mainSizeRoad, pctRoad, s_TotalNodes);
        Print(BDE_EGPS_Constants.LOG_TAG + roadMsg);
    }

    protected static void BuildForMode(RGC_Graph graph, RGC_DrivabilityMode mode,
        map<int, int> nodeToComp, map<int, int> componentSize,
        out int mainCompOut, out int totalCompsOut)
    {

        array<int> queue = new array<int>;

        int compId = 0;
        int largestSize = 0;
        int largestComp = -1;

        int i;
        int head;
        int curId;
        int compSize;
        int t;
        int transN;
        int neighbour;
        array<ref RGC_Transition> trans;
        RGC_Transition tr;

        for (i = 0; i < s_TotalNodes; i++)
        {
            if (nodeToComp.Contains(i)) continue;

            queue.Clear();
            queue.Insert(i);
            nodeToComp.Set(i, compId);
            compSize = 0;
            head = 0;

            while (head < queue.Count())
            {
                curId = queue.Get(head);
                head++;
                compSize++;

                trans = graph.GetTransitions(curId, mode);
                if (!trans) continue;
                transN = trans.Count();
                for (t = 0; t < transN; t++)
                {
                    tr = trans.Get(t);
                    if (!tr) continue;
                    neighbour = tr.GetTargetNodeId();
                    if (nodeToComp.Contains(neighbour)) continue;
                    nodeToComp.Set(neighbour, compId);
                    queue.Insert(neighbour);
                }
            }

            componentSize.Set(compId, compSize);
            if (compSize > largestSize)
            {
                largestSize = compSize;
                largestComp = compId;
            }
            compId++;
        }

        queue.Clear();
        mainCompOut   = largestComp;
        totalCompsOut = compId;
    }

    static int GetComponentId(int nodeId, RGC_DrivabilityMode mode)
    {
        if (!s_Built) return -1;
        map<int, int> nodeToComp = GetNodeToCompMap(mode);
        if (!nodeToComp) return -1;
        if (!nodeToComp.Contains(nodeId)) return -1;
        return nodeToComp.Get(nodeId);
    }

    static int GetMainComponentId(RGC_DrivabilityMode mode)
    {
        if (mode == RGC_DrivabilityMode.ROAD)     return s_MainCompROAD;
        if (mode == RGC_DrivabilityMode.DRIVABLE) return s_MainCompDRIVABLE;
        return s_MainCompANY;
    }

    static int GetComponentSize(int compId, RGC_DrivabilityMode mode)
    {
        if (!s_Built) return 0;
        map<int, int> componentSize = GetComponentSizeMap(mode);
        if (!componentSize) return 0;
        if (!componentSize.Contains(compId)) return 0;
        return componentSize.Get(compId);
    }

    static bool SameComponent(int nodeIdA, int nodeIdB, RGC_DrivabilityMode mode)
    {
        if (!s_Built) return true;
        int compA = GetComponentId(nodeIdA, mode);
        int compB = GetComponentId(nodeIdB, mode);
        if (compA == -1 || compB == -1) return true;  // unknown -> safe fallback
        return compA == compB;
    }

    static int GetTotalComponents(RGC_DrivabilityMode mode)
    {
        if (mode == RGC_DrivabilityMode.ROAD)     return s_TotalCompsROAD;
        if (mode == RGC_DrivabilityMode.DRIVABLE) return s_TotalCompsDRIVABLE;
        return s_TotalCompsANY;
    }

    static bool IsBuilt()         { return s_Built; }
    static int  GetTotalNodes()   { return s_TotalNodes; }

    protected static map<int, int> GetNodeToCompMap(RGC_DrivabilityMode mode)
    {
        if (mode == RGC_DrivabilityMode.ROAD)     return s_NodeToCompROAD;
        if (mode == RGC_DrivabilityMode.DRIVABLE) return s_NodeToCompDRIVABLE;
        return s_NodeToCompANY;
    }

    protected static map<int, int> GetComponentSizeMap(RGC_DrivabilityMode mode)
    {
        if (mode == RGC_DrivabilityMode.ROAD)     return s_ComponentSizeROAD;
        if (mode == RGC_DrivabilityMode.DRIVABLE) return s_ComponentSizeDRIVABLE;
        return s_ComponentSizeANY;
    }

    static RGC_Node SnapToNearestRoad(RGC_Graph graph, vector pos)
    {
        if (!graph) return null;
        RGC_Node fast = graph.FindNearestNode(pos, 1000.0);
        if (fast) return fast;
        return FindNearestNodeUnbounded(graph, pos);
    }

    static RGC_Node FindNearestNodeUnbounded(RGC_Graph graph, vector pos)
    {
        if (!graph) return null;
        array<ref RGC_Node> nodes = graph.GetNodes();
        if (!nodes) return null;
        int n = nodes.Count();
        if (n == 0) return null;

        RGC_Node best = nodes.Get(0);
        if (!best) return null;
        float bestDist2 = vector.DistanceSq(pos, best.GetPos());

        int i;
        RGC_Node nd;
        float d2;
        for (i = 1; i < n; i++)
        {
            nd = nodes.Get(i);
            if (!nd) continue;
            d2 = vector.DistanceSq(pos, nd.GetPos());
            if (d2 < bestDist2)
            {
                bestDist2 = d2;
                best = nd;
            }
        }
        return best;
    }

    static RGC_Node FindNearestNodeInComponent(RGC_Graph graph, vector pos, int compId, RGC_DrivabilityMode mode, float maxRadiusM)
    {
        if (!graph) return null;
        if (!s_Built) return null;
        map<int, int> nodeToComp = GetNodeToCompMap(mode);
        if (!nodeToComp) return null;

        array<ref RGC_Node> nodes = graph.GetNodes();
        if (!nodes) return null;
        int n = nodes.Count();
        if (n == 0) return null;

        float maxR2 = maxRadiusM * maxRadiusM;
        float bestDist2 = maxR2;
        RGC_Node best = null;

        int i;
        RGC_Node nd;
        int nodeComp;
        float d2;
        for (i = 0; i < n; i++)
        {
            nd = nodes.Get(i);
            if (!nd) continue;

            d2 = vector.DistanceSq(pos, nd.GetPos());
            if (d2 >= bestDist2) continue;

            if (!nodeToComp.Contains(i)) continue;
            nodeComp = nodeToComp.Get(i);
            if (nodeComp != compId) continue;

            bestDist2 = d2;
            best = nd;
        }
        return best;
    }

    static void Invalidate()
    {
        s_NodeToCompANY      = null;
        s_ComponentSizeANY   = null;
        s_MainCompANY        = -1;
        s_TotalCompsANY      = 0;

        s_NodeToCompDRIVABLE    = null;
        s_ComponentSizeDRIVABLE = null;
        s_MainCompDRIVABLE      = -1;
        s_TotalCompsDRIVABLE    = 0;

        s_NodeToCompROAD    = null;
        s_ComponentSizeROAD = null;
        s_MainCompROAD      = -1;
        s_TotalCompsROAD    = 0;

        s_TotalNodes = 0;
        s_Built      = false;
    }
}
