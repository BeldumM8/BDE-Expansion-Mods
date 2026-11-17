class BDE_VL_RPC
{
    protected static ref BDE_VL_RPC s_Instance;

    static BDE_VL_RPC Get()
    {
        if (!s_Instance)
            s_Instance = new BDE_VL_RPC();

        return s_Instance;
    }

    void Register()
    {
        GetRPCManager().AddRPC(BDE_VL_RPC_NAMESPACE, BDE_VL_RPC_MARKER_UPDATE, this, SingeplayerExecutionType.Both);
    }

    void RPC_MarkerUpdate(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Client)
            return;

        Param4<vector, string, string, int> data;
        if (!ctx.Read(data))
            return;

        if (data.param1 == vector.Zero)
            BDE_VL_MarkerClient.DeleteMarker(data.param2);
        else
            BDE_VL_MarkerClient.CreateMarker(data.param1, data.param2, data.param3, data.param4);
    }
};
