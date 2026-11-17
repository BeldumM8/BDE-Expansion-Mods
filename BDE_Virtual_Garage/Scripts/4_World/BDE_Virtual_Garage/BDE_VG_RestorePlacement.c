// Explicit root restore orchestration is required: Expansion's internal static
// calls do not dispatch a modded Restore_Phase1 override. Nested inventory still
// uses Expansion's standard loader. The original payload is never deleted here.
class BDE_VG_RestorePlacement
{
    static bool RestoreAtFile(BDE_VG_VehicleData data, vector basePosition, vector orientation, out EntityAI entity)
    {
        entity = null;
        if (!g_Game.IsServer() || !data)
            return false;
        string fileName = data.GetEntityStorageFileName();
        FileSerializer reader = new FileSerializer;
        if (!reader.Open(fileName,FileMode.READ))
            return false;
        ExpansionEntityStorageModule.Reset();
        ExpansionEntityStorageModule.ResetState();
        ExpansionEntityStorageModule.Now = CF_Date.Now(true).DateToEpoch();
        bool restored = RestoreRoot(reader,data,basePosition,orientation,entity);
        reader.Close();
        // Close child contexts on failure as well; keep all .bin payloads.
        ExpansionEntityStorageModule.Reset(false);
        ExpansionEntityStorageModule.s_CurrentEntity = null;
        ExpansionEntityStorageModule.s_CurrentRootEntity = null;
        if (!restored)
        {
            if (entity)
                g_Game.ObjectDelete(entity);
            entity = null;
            Print("[BDE_VG] Explicit root restore failed for " + data.m_ClassName + "; payload retained.");
        }
        return restored;
    }

    protected static bool RestoreRoot(FileSerializer reader, BDE_VG_VehicleData data, vector basePosition, vector orientation, out EntityAI entity)
    {
        int version;
        if (!reader.Read(version) || version < 4 || version > ExpansionEntityStorageModule.VERSION)
            return false;
        ExpansionEntityStorageModule.s_LastRestoredVersion = version;
        int elapsed;
        if (version >= 5)
        {
            int timestamp;
            if (!reader.Read(timestamp))
                return false;
            elapsed = ExpansionEntityStorageModule.Now - timestamp;
        }
        string storedType;
        InventoryLocationType locationType;
        vector storedPosition;
        vector storedOrientation;
        if (!reader.Read(storedType) || storedType != data.m_ClassName)
            return false;
        if (!reader.Read(locationType) || locationType != InventoryLocationType.GROUND)
            return false;
        if (!reader.Read(storedPosition) || !reader.Read(storedOrientation))
            return false;

        float groundOffset = storedPosition[1] - data.m_Position[1];
        vector spawnPosition = basePosition + Vector(0,groundOffset,0);
        if (!Class.CastTo(entity,g_Game.CreateObjectEx(storedType,spawnPosition,ECE_OBJECT_SWAP,RF_DEFAULT)))
            return false;
        ExpansionVehicle vehicle;
        if (ExpansionVehicle.Get(vehicle,entity))
            spawnPosition = basePosition + Vector(0,vehicle.GetModelZeroPointDistanceFromGround(),0);
        entity.SetPosition(spawnPosition);
        entity.SetOrientation(orientation);
        SetVelocity(entity,vector.Zero);
        dBodySetAngularVelocity(entity,vector.Zero);
        Transport transport = Transport.Cast(entity);
        if (transport)
            transport.DisableSimulation(true);
        dBodyActive(entity,ActiveState.INACTIVE);
        ExpansionEntityStorageModule.s_CurrentEntity = entity;
        ExpansionEntityStorageModule.s_CurrentRootEntity = entity;

        // Match Expansion's version-dependent ordering exactly. A pre-created
        // root passed to its ordinary RestoreFromFile skips Phase3a/3b entirely.
        if (version == 10 && !ExpansionEntityStorageModule.Restore_Phase3a(reader,entity,version))
            return false;
        string fileName = data.GetEntityStorageFileName();
        string basePath = fileName.Substring(0,fileName.Length() - ExpansionEntityStorageModule.EXT.Length());
        if (!ExpansionEntityStorageModule.Restore_Phase2(reader,basePath,entity,null,null,version,string.Empty,0,elapsed,false))
            return false;
        if (version != 10 && !ExpansionEntityStorageModule.Restore_Phase3a(reader,entity,version))
            return false;
        ExpansionEntityStorageModule.Restore_Phase3b(entity,elapsed);
        Print("[BDE_VG] Explicit restore " + storedType + " saved=" + storedPosition.ToString() + " selected=" + spawnPosition.ToString() + " actual=" + entity.GetPosition().ToString());
        // A mod moving the root back to saved coordinates must not turn a
        // misplaced vehicle into a successfully committed retrieval.
        vector actual = entity.GetPosition();
        if (Math.AbsFloat(actual[0] - spawnPosition[0]) > 0.5 || Math.AbsFloat(actual[2] - spawnPosition[2]) > 0.5)
            return false;
        return true;
    }
};
