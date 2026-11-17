// Shared tuning values for route search, drawing and UI state.

class BDE_EGPS_Constants
{

    // Strings

    static const string LOG_TAG = "[BDE_EGPS]";

    static const string MODE_LABEL_DRIVABLE = "MODE: DRIVABLE";
    static const string MODE_LABEL_ROAD     = "MODE: ROAD";

    // Pathfinding

    static float GetFindNearestRadiusM()   { return 500.0; }

    // Player-side bridge search radius. Waypoint-side bridge lookup is
    // intentionally unbounded so ROAD mode can reuse any road on the map.

    static float GetBridgeSearchRadiusM()  { return 1000.0; }

    // Route line styling

    static int   GetRouteLineColorArgb()   { return 0xFFFFAA00; }

    static int   GetTrailColorArgb()       { return 0x80808080; }

    static int   GetDashedColorArgb()      { return 0xC0FFAA00; }

    static float GetRouteLineThicknessPx() { return 6.0; }
    static float GetDashedLineThicknessPx() { return 4.0; }

    static float GetBigMapRouteThicknessPx() { return 8.0; }
    static float GetBigMapDashedThicknessPx() { return 6.0; }

    // Dashed tails

    static float GetDashedSegmentLengthPx() { return 12.0; }

    static float GetDashedGapLengthPx()     { return 8.0; }

    static float GetDashedTailThresholdM()  { return 15.0; }

    // Trail/reroute cadence

    static int   GetTraversedAdvanceTickMs() { return 500; }

    static float GetRerouteFullThresholdM() { return 300.0; }

    static int   GetRerouteCooldownMs()     { return 2000; }

    // Multi-waypoint

    static int   GetMaxWaypoints()         { return 10; }
}
