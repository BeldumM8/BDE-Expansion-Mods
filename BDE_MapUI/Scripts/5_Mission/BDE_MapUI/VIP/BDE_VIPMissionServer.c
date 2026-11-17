// BDE_VIPMissionServer.c
// Initialises VIP system and starts timers on first player connection.

modded class MissionServer
{
    override void OnInit()
    {
        super.OnInit();

        BDE_VIPManager.Get().Init();
    }

    override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
    {
        super.InvokeOnConnect(player, identity);

#ifdef SERVER
        BDE_VIPManager.Get().ProcessPendingVIPForIdentity(identity);
        BDE_VIPManager.Get().QueuePointsStoreConfigSync(15000);
        BDE_VIPManager.Get().StartPassiveIncomeTimerOnce();
        BDE_VIPManager.Get().StartPassiveRepTimerOnce();
#endif
    }
}
