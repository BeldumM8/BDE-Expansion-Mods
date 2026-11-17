modded class PlayerBase
{
    protected string m_BDE_VL_LastVehiclePersistentID = "";

    void BDE_VL_SetLastVehiclePersistentID(string persistentID)
    {
        m_BDE_VL_LastVehiclePersistentID = persistentID;

        if (g_Game.IsServer())
            Save();
    }

    string BDE_VL_GetLastVehiclePersistentID()
    {
        return m_BDE_VL_LastVehiclePersistentID;
    }

    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);

        auto ctx = storage[BDE_VL_MOD_STORAGE];
        if (!ctx)
            return;

        ctx.Write(m_BDE_VL_LastVehiclePersistentID);
    }

    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage))
            return false;

        auto ctx = storage[BDE_VL_MOD_STORAGE];
        if (!ctx)
            return true;

        if (ctx.GetVersion() >= 1)
        {
            if (!ctx.Read(m_BDE_VL_LastVehiclePersistentID))
                return false;
        }

        return true;
    }
};
