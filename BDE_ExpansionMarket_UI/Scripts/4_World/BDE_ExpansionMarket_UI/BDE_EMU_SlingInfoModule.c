#ifdef BDE_HelicopterSling
class BDE_EMU_SlingInfo
{
    float WeightKg = -1;
    float CapacityKg = -1;
};

[CF_RegisterModule(BDE_EMU_SlingInfoModule)]
class BDE_EMU_SlingInfoModule : CF_ModuleWorld
{
    static ref ScriptInvoker Changed = new ScriptInvoker;
    protected static BDE_EMU_SlingInfoModule s_Instance;
    protected ref map<string, ref BDE_EMU_SlingInfo> m_Cache = new map<string, ref BDE_EMU_SlingInfo>;
    protected ref map<string, int> m_Requested = new map<string, int>;

    override void OnInit()
    {
        super.OnInit();
        s_Instance = this;
        EnableMissionFinish();
        Expansion_EnableRPCManager();
        Expansion_RegisterServerRPC("RPC_RequestSlingInfo");
        Expansion_RegisterClientRPC("RPC_SendSlingInfo");
    }

    override void OnMissionFinish(Class sender, CF_EventArgs args)
    {
        m_Cache.Clear();
        m_Requested.Clear();
        Changed.Clear();
        s_Instance = null;
    }

    static BDE_EMU_SlingInfo Lookup(string className)
    {
        if (!s_Instance)
            return null;
        BDE_EMU_SlingInfo result;
        int requested;
        bool known = s_Instance.m_Requested.Find(className, requested);
        s_Instance.m_Cache.Find(className, result);
        int lifetime = 2000;
        if (result)
            lifetime = 30000;
        if (!known || GetGame().GetTime() - requested >= lifetime)
        {
            s_Instance.m_Requested.Set(className, GetGame().GetTime());
            if (GetGame().IsServer())
            {
                result = ReadConfigured(className);
                s_Instance.m_Cache.Set(className, result);
            }
            else
            {
                auto rpc = s_Instance.Expansion_CreateRPC("RPC_RequestSlingInfo");
                rpc.Write(className);
                rpc.Expansion_Send(true);
            }
        }
        return result;
    }

    // Read only: never generate profiles or use the client's local profile folder.
    protected static BDE_EMU_SlingInfo ReadConfigured(string className)
    {
        BDE_EMU_SlingInfo result = new BDE_EMU_SlingInfo;
        if (!GetGame().IsServer() || !GetGame().ConfigIsExisting("CfgVehicles " + className) || !BDE_HS_Settings.IsVehicle(className))
            return result;
        string family = BDE_HS_Settings.ResolveBase(className);
        bool helicopter = BDE_HS_Settings.IsHelicopter(className);
        string folder = "VehicleWeights";
        string field = "WeightTons";
        if (helicopter)
        {
            folder = "HeliCapacities";
            field = "MaximumWeightTons";
        }
        ExpansionJSON document = BDE_HS_ProfileIO.Read(BDE_HS_Settings.ROOT + "/" + folder + "/" + family + ".json");
        if (!document || document.Get("Version").GetInt() != 1 || document.Get("BaseClass").GetString() != family || !document.Get("Enabled").GetBool() || !BDE_HS_ProfileIO.Number(document.Get(field)))
            return result;
        float tons = document.Get(field).GetFloat();
        if (!(tons >= 0 && tons < 1000000))
            return result;
        if (helicopter)
            result.CapacityKg = tons * 1000;
        else if (tons > 0)
            result.WeightKg = tons * 1000;
        return result;
    }

    protected void RPC_RequestSlingInfo(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        string className;
        if (!GetGame().IsServer() || !identity || !ctx.Read(className) || className.Length() > 128 || !GetGame().ConfigIsExisting("CfgVehicles " + className))
            return;
        BDE_EMU_SlingInfo result = ReadConfigured(className);
        auto rpc = Expansion_CreateRPC("RPC_SendSlingInfo");
        rpc.Write(className);
        rpc.Write(result.WeightKg);
        rpc.Write(result.CapacityKg);
        rpc.Expansion_Send(true, identity);
    }

    protected void RPC_SendSlingInfo(PlayerIdentity identity, Object target, ParamsReadContext ctx)
    {
        string className;
        BDE_EMU_SlingInfo result = new BDE_EMU_SlingInfo;
        if (GetGame().IsServer() || !ctx.Read(className) || !m_Requested.Contains(className) || !ctx.Read(result.WeightKg) || !ctx.Read(result.CapacityKg))
            return;
        if (!(result.WeightKg >= -1 && result.WeightKg < 1000000000) || !(result.CapacityKg >= -1 && result.CapacityKg < 1000000000))
            return;
        m_Cache.Set(className, result);
        m_Requested.Set(className, GetGame().GetTime());
        Changed.Invoke(className);
    }
};
#endif
