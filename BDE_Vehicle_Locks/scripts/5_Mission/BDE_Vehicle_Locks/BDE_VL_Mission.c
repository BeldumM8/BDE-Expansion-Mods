modded class MissionBase
{
    void MissionBase()
    {
        BDE_VL_RPC.Get().Register();
    }
};

modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();

        BDE_VL_Settings.Get();
        BDE_VL_RaidConfig.GenerateFiles();
        BDE_VL_Logger.Log("Loaded BDE Vehicle Locks v1.0.6");
    }

    override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
    {
        super.InvokeOnConnect(player, identity);

        if (player && identity)
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BDE_VL_MarkerManager.RestoreMarker, 1500, false, player);
    }
};

modded class MissionGameplay
{
    override void OnInit()
    {
        super.OnInit();
        BDE_VL_MarkerClient.DeleteMarker();
    }
};
