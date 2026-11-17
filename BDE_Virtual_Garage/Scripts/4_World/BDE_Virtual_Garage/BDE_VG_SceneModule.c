[CF_RegisterModule(BDE_VG_SceneModule)]
class BDE_VG_SceneModule : CF_ModuleWorld
{
    static ref BDE_VG_SceneSettings Settings;
    static bool Ready;

    override void OnInit()
    {
        super.OnInit();
        EnableMissionStart();
        EnableMissionFinish();
        Expansion_EnableRPCManager();
        Expansion_RegisterServerRPC("RPC_RequestScenes");
        Expansion_RegisterClientRPC("RPC_SendScenes");
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        Settings = new BDE_VG_SceneSettings;
        Ready = false;
        if (g_Game.IsServer())
        {
            string path = "$profile:BDE/VehiclePreviewScenes.json";
            MakeDirectory("$profile:BDE");
            if (FileExist(path))
                JsonFileLoader<BDE_VG_SceneSettings>.JsonLoadFile(path, Settings);
            Settings.Normalize();
            JsonFileLoader<BDE_VG_SceneSettings>.JsonSaveFile(path, Settings);
            Ready = true;
        }
        else
        {
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RequestScenes, 2000, true);
        }
    }

    override void OnMissionFinish(Class sender, CF_EventArgs args)
    {
        g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(RequestScenes);
        Settings = null;
        Ready = false;
    }

    protected void RequestScenes()
    {
        if (Ready)
        {
            g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(RequestScenes);
            return;
        }
        if (!g_Game.GetPlayer())
            return;
        auto rpc = Expansion_CreateRPC("RPC_RequestScenes");
        rpc.Expansion_Send(true);
    }

    protected void RPC_RequestScenes(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        if (!identity || !Ready)
            return;
        auto rpc = Expansion_CreateRPC("RPC_SendScenes");
        rpc.Write(Settings);
        rpc.Expansion_Send(true, identity);
    }

    protected void RPC_SendScenes(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        BDE_VG_SceneSettings received;
        if (!ctx.Read(received))
            return;
        if (!received)
            return;
        received.Normalize();
        Settings = received;
        Ready = true;
    }
}
