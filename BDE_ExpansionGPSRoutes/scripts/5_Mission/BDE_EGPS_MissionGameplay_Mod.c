// Client-side lifecycle hook for manager init, input and route ticks.

modded class MissionGameplay
{
    protected float m_BDEEGPS_TickAccum;
    protected float m_BDEEGPS_InitRetryAccum;

    override void OnInit()
    {
        super.OnInit();

        Widget.SetLV(0);
        Widget.SetTextLV(0);

        Print(BDE_EGPS_Constants.LOG_TAG + " MissionGameplay.OnInit -> initializing GPS manager...");
        BDE_EGPS_Manager.GetInstance().Init();
        m_BDEEGPS_TickAccum = 0.0;
        m_BDEEGPS_InitRetryAccum = 0.0;
    }

    override void OnUpdate(float timeslice)
    {
        super.OnUpdate(timeslice);

        BDE_EGPS_Manager manager = BDE_EGPS_Manager.GetInstance();
        if (manager && !manager.IsReady())
        {
            m_BDEEGPS_InitRetryAccum = m_BDEEGPS_InitRetryAccum + timeslice;
            if (m_BDEEGPS_InitRetryAccum >= 2.0)
            {
                m_BDEEGPS_InitRetryAccum = 0.0;
                manager.EnsureReady(false);
            }
        }

        Input input = GetGame().GetInput();
        if (input && input.LocalPress("UABDEEGPS_ToggleMode", false))
        {
            BDE_EGPS_Manager mgrToggle = BDE_EGPS_Manager.GetInstance();
            if (mgrToggle && mgrToggle.IsReady())
            {
                mgrToggle.ToggleMode();

                MissionGameplay mission = MissionGameplay.Cast(GetGame().GetMission());
                if (mission)
                {
                    IngameHud hud = IngameHud.Cast(mission.GetHud());
                    if (hud)
                        hud.BDEEGPS_ShowModeToast();
                }
            }
        }

        m_BDEEGPS_TickAccum = m_BDEEGPS_TickAccum + timeslice;
        float tickIntervalSec = BDE_EGPS_Constants.GetTraversedAdvanceTickMs() / 1000.0;
        if (m_BDEEGPS_TickAccum < tickIntervalSec)
            return;

        m_BDEEGPS_TickAccum = 0.0;

        BDE_EGPS_Manager mgr = BDE_EGPS_Manager.GetInstance();
        if (!mgr || !mgr.IsReady())
            return;

        BDE_EGPS_Route route = mgr.GetRoute();
        if (!route || !route.HasWaypoints())
            return;

        if (!GetGame() || !GetGame().GetPlayer())
            return;

        vector playerPos = GetGame().GetPlayer().GetPosition();
        route.AdvanceTraversedIdx(playerPos);
        mgr.OnTickReroute(playerPos);
    }

    void ~MissionGameplay()
    {
        BDE_EGPS_Manager.SafeCleanup();
    }
}
