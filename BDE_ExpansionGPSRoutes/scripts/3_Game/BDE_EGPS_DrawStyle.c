// Visual parameters passed to BDE_EGPS_RouteDrawer.DrawFullRoute.
// Shared instances keep HUD and big-map styling consistent.

class BDE_EGPS_DrawStyle
{

    // Style fields

    int   m_ColorPending;     // pending segment of the road path (vivid orange)
    int   m_ColorTraversed;   // already-walked segment (semi-transparent grey)
    int   m_ColorDashed;      // dashed lines (player->path tail and tails to off-road waypoints)

    float m_ThicknessMain;    // road path
    float m_ThicknessDashed;  // dashed lines

    float m_DashSegPx;        // stroke length
    float m_DashGapPx;        // gap length

    float m_DashedTailThresholdM;

    // Cached instances

    protected static ref BDE_EGPS_DrawStyle s_HudInstance;
    protected static ref BDE_EGPS_DrawStyle s_BigMapInstance;

    // Factory methods

    static BDE_EGPS_DrawStyle GetForHud()
    {
        if (!s_HudInstance)
        {
            s_HudInstance = new BDE_EGPS_DrawStyle();
            s_HudInstance.LoadCommonFromConstants();
            s_HudInstance.m_ThicknessMain   = BDE_EGPS_Constants.GetRouteLineThicknessPx();
            s_HudInstance.m_ThicknessDashed = BDE_EGPS_Constants.GetDashedLineThicknessPx();
        }
        return s_HudInstance;
    }

    static BDE_EGPS_DrawStyle GetForBigMap()
    {
        if (!s_BigMapInstance)
        {
            s_BigMapInstance = new BDE_EGPS_DrawStyle();
            s_BigMapInstance.LoadCommonFromConstants();
            s_BigMapInstance.m_ThicknessMain   = BDE_EGPS_Constants.GetBigMapRouteThicknessPx();
            s_BigMapInstance.m_ThicknessDashed = BDE_EGPS_Constants.GetBigMapDashedThicknessPx();
        }
        return s_BigMapInstance;
    }

    static void Invalidate()
    {
        s_HudInstance    = null;
        s_BigMapInstance = null;
    }

    // Shared setup

    protected void LoadCommonFromConstants()
    {
        m_ColorPending          = BDE_EGPS_Constants.GetRouteLineColorArgb();
        m_ColorTraversed        = BDE_EGPS_Constants.GetTrailColorArgb();
        m_ColorDashed           = BDE_EGPS_Constants.GetDashedColorArgb();
        m_DashSegPx             = BDE_EGPS_Constants.GetDashedSegmentLengthPx();
        m_DashGapPx             = BDE_EGPS_Constants.GetDashedGapLengthPx();
        m_DashedTailThresholdM  = BDE_EGPS_Constants.GetDashedTailThresholdM();
    }
}
