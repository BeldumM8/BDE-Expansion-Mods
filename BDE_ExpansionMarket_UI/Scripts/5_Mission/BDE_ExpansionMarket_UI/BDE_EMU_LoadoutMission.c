modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BDE_LM_Start, 10000, false);
    }

    protected void BDE_LM_Start()
    {
        ExpansionMarketModule module = ExpansionMarketModule.GetInstance();
        if (module)
        {
            module.BDE_LM_InitStations();
        }
    }

    override void OnMissionFinish()
    {
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(BDE_LM_Start);
        super.OnMissionFinish();
    }
}

modded class MissionGameplay
{
    override void OnInit()
    {
        super.OnInit();
        ExpansionMarketModule.SI_BDELoadoutOpen.Insert(BDE_LM_Open);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BDE_LM_Sync, 5000, false);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BDE_LM_Sync, 30000, true);
    }

    protected void BDE_LM_Sync()
    {
        ExpansionMarketModule module = ExpansionMarketModule.GetInstance();
        if (module && GetGame().GetPlayer())
        {
            module.BDE_LM_RequestStations();
        }
    }

    protected void BDE_LM_Open(Object station)
    {
        BDE_EMU_LoadoutMenu.Open(station);
    }

    override void OnMissionFinish()
    {
        BDE_EMU_LoadoutMenu.ReleaseMenu();
        ExpansionMarketModule.SI_BDELoadoutOpen.Remove(BDE_LM_Open);
        GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(BDE_LM_Sync);
        super.OnMissionFinish();
    }
}
