class BDE_VG_SpawnFinder
{
    static int GetVehicleKind(string className)
    {
        if (className == "ExpansionVodnik")
            return BDE_VG_KIND_LAND;
        if (g_Game.IsKindOf(className,"BoatScript") || g_Game.IsKindOf(className,"ExpansionBoatScript") || g_Game.IsKindOf(className,"ExpansionVehicleBoatBase"))
            return BDE_VG_KIND_BOAT;
        if (g_Game.IsKindOf(className,"HelicopterScript") || g_Game.IsKindOf(className,"ExpansionHelicopterScript") || g_Game.IsKindOf(className,"ExpansionVehicleHelicopterBase") || g_Game.IsKindOf(className,"ExpansionVehiclePlaneBase"))
            return BDE_VG_KIND_AIRCRAFT;
        return BDE_VG_KIND_LAND;
    }

    static bool IsOriginalPositionFree(BDE_VG_VehicleData vehicleData, out Object blockingObject)
    {
        blockingObject = null;
        if (!vehicleData)
            return false;
        return ExpansionItemSpawnHelper.IsSpawnPositionFree(vehicleData.m_Position,vehicleData.m_Orientation,vehicleData.m_ClassName,blockingObject);
    }

    // Check configured spawns closest to the actual access terminal, within 15 m.
    // Prefer the first unobstructed matching point. Direction is yaw/pitch/roll.
    static bool FindConfigured(EntityAI accessObject, BDE_VG_VehicleData vehicleData, out vector position, out vector orientation, out bool hasConfiguredPoint)
    {
        position = vector.Zero;
        orientation = vector.Zero;
        hasConfiguredPoint = false;
        if (!accessObject || !vehicleData)
            return false;
        // Global config is loaded first, which also migrates/loads the map-local spawn file.
        BDE_VG_Settings.Get();
        BDE_VG_SpawnSettings spawnSettings = BDE_VG_SpawnSettings.Get();
        if (!spawnSettings || !spawnSettings.SpawnPositions)
            return false;

        int kind = GetVehicleKind(vehicleData.m_ClassName);
        vector origin = accessObject.GetPosition();
        array<ref BDE_VG_SpawnPosition> ordered = new array<ref BDE_VG_SpawnPosition>;
        foreach (BDE_VG_SpawnPosition entry : spawnSettings.SpawnPositions)
        {
            if (!entry)
                continue;
            float distanceSq = vector.DistanceSq(origin,entry.Position);
            if (distanceSq > 225.0)
                continue;
            // Trust authored height. Only low-water points are boat-only.
            bool water = g_Game.SurfaceIsSea(entry.Position[0],entry.Position[2]) || g_Game.SurfaceIsPond(entry.Position[0],entry.Position[2]);
            bool boatOnly = water && entry.Position[1] < 1.0;
            if (kind == BDE_VG_KIND_BOAT && !boatOnly)
                continue;
            if (kind != BDE_VG_KIND_BOAT && boatOnly)
                continue;
            int insertIndex = 0;
            while (insertIndex < ordered.Count() && vector.DistanceSq(origin,ordered[insertIndex].Position) <= distanceSq)
                insertIndex++;
            ordered.InsertAt(entry,insertIndex);
        }
        hasConfiguredPoint = ordered.Count() > 0;
        // Try the nearest slot first. Do not evict vehicles from farther slots
        // merely because they occurred earlier in SpawnPositions.json.
        foreach (BDE_VG_SpawnPosition selected : ordered)
        {
            if (!ClearConfiguredSpawn(selected,vehicleData))
                continue;
            position = selected.Position;
            orientation = selected.Direction;
            return true;
        }
        return false;
    }

    // Move only parked, unoccupied vehicles. Players and loose cargo remain
    // real obstructions, and towing chains must not be split by a teleport.
    protected static bool ClearConfiguredSpawn(BDE_VG_SpawnPosition entry, BDE_VG_VehicleData requested)
    {
        if (!g_Game.IsServer())
            return false;
        array<Object> moved = new array<Object>;
        for (int attempt = 0; attempt < 32; attempt++)
        {
            Object blocker;
            if (ExpansionItemSpawnHelper.IsSpawnPositionFree(entry.Position,entry.Direction,requested.m_ClassName,blocker))
                return true;
            ExpansionVehicle blockingVehicle;
            if (!blocker || !ExpansionVehicle.Get(blockingVehicle,blocker) || moved.Find(blocker) >= 0)
                return false;
            EntityAI entity = blockingVehicle.GetEntity();
            if (!entity || blockingVehicle.GetCrew(false,true).Count() > 0 || blockingVehicle.IsTowing() || blockingVehicle.GetSpeedometerAbsolute() > 1.0)
            {
                Print("[BDE_VG Spawn] Cannot safely relocate occupied/moving/towing blocker " + blocker.GetType());
                return false;
            }
            // Also refuse vehicles being towed by something else.
            CarScript blockedCar = CarScript.Cast(entity);
            if (blockedCar)
            {
                for (int towIndex = 0; towIndex < blockedCar.Expansion_NumberTowConnections(); towIndex++)
                {
                    if (!blockedCar.Expansion_IsTowPointFree(towIndex))
                        return false;
                }
            }
            vector destination;
            vector destinationOrientation = entity.GetOrientation();
            destinationOrientation[1] = 0;
            destinationOrientation[2] = 0;
            if (!FindRelocationPosition(blockingVehicle,requested,entry.Position,destinationOrientation,destination))
            {
                Print("[BDE_VG Spawn] No safe relocation point for " + entity.GetType() + " blocking " + entry.Position.ToString());
                return false;
            }
            vector previousPosition = entity.GetPosition();
            vector previousOrientation = entity.GetOrientation();
            blockingVehicle.EngineStop();
            MoveParkedVehicle(entity,destination,destinationOrientation);
            vector actualPosition = entity.GetPosition();
            if (vector.DistanceSq(actualPosition,destination) > 0.25)
            {
                MoveParkedVehicle(entity,previousPosition,previousOrientation);
                Print("[BDE_VG Spawn] Blocker relocation did not stick; restored previous position.");
                return false;
            }
            moved.Insert(blocker);
            Print("[BDE_VG Spawn] Relocated blocking " + entity.GetType() + " from=" + previousPosition.ToString() + " to=" + destination.ToString() + " clearedSlot=" + entry.Position.ToString());
        }
        return false;
    }

    protected static bool FindRelocationPosition(ExpansionVehicle vehicle, BDE_VG_VehicleData requested, vector reservedPosition, vector orientation, out vector destination)
    {
        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        if (!settings || settings.AlternativeSpawnMaximumRadiusMeters <= 0)
            return false;
        EntityAI entity = vehicle.GetEntity();
        BDE_VG_VehicleData blockerData = new BDE_VG_VehicleData;
        blockerData.m_ClassName = entity.GetType();
        int kind = GetVehicleKind(entity.GetType());
        int supportKind = kind;
        if (kind == BDE_VG_KIND_AIRCRAFT)
            supportKind = BDE_VG_KIND_LAND;
        vector originalPosition = entity.GetPosition();
        vector origin = originalPosition - Vector(0,vehicle.GetModelZeroPointDistanceFromGround(),0);
        float bestDistanceSq = 1000000000.0;
        bool found;
        // One-metre rings, with denser angular samples than normal fallback.
        // Stop once even a flat point on the next ring cannot be closer.
        int directions = Math.Clamp(settings.AlternativeSpawnDirections,32,64);
        int rings = Math.Floor(settings.AlternativeSpawnMaximumRadiusMeters);
        for (int ring = 0; ring <= rings; ring++)
        {
            float radius = ring;
            if (radius * radius > bestDistanceSq)
                break;
            int samples = directions;
            if (ring == 0)
                samples = 1;
            for (int directionIndex = 0; directionIndex < samples; directionIndex++)
            {
                float angle = (360.0 / directions) * directionIndex * Math.DEG2RAD;
                vector candidate = origin + Vector(Math.Sin(angle) * radius,0,Math.Cos(angle) * radius);
                if (!ResolveSpawnSurface(blockerData,candidate,supportKind,true))
                    continue;
                if (!OutsideReservedSpawns(candidate,blockerData.m_ClassName,requested.m_ClassName,reservedPosition))
                    continue;
                vector entityPosition = candidate + Vector(0,vehicle.GetModelZeroPointDistanceFromGround(),0);
                float distanceSq = vector.DistanceSq(originalPosition,entityPosition);
                if (distanceSq > settings.AlternativeSpawnMaximumRadiusMeters * settings.AlternativeSpawnMaximumRadiusMeters)
                    continue;
                if (distanceSq >= bestDistanceSq || !IsRelocationSpaceFree(entity,entityPosition,orientation))
                    continue;
                destination = entityPosition;
                bestDistanceSq = distanceSq;
                found = true;
            }
        }
        return found;
    }

    protected static bool OutsideReservedSpawns(vector candidate, string movingClass, string requestedClass, vector reservedPosition)
    {
        vector movingBounds[2];
        vector requestedBounds[2];
        if (!ExpansionStatic.GetCollisionBox(movingClass,movingBounds) || !ExpansionStatic.GetCollisionBox(requestedClass,requestedBounds))
            return false;
        vector movingSize = movingBounds[1] - movingBounds[0];
        vector requestedSize = requestedBounds[1] - requestedBounds[0];
        float movingRadius = Math.Sqrt(movingSize[0] * movingSize[0] + movingSize[2] * movingSize[2]) * 0.5;
        float reservedRadius = Math.Sqrt(requestedSize[0] * requestedSize[0] + requestedSize[2] * requestedSize[2]) * 0.5;
        float clearance = movingRadius + reservedRadius + 1.0;
        float height = Math.Max(movingSize[1],requestedSize[1]) + 1.0;
        if (OverlapsReserved(candidate,reservedPosition,clearance,height))
            return false;
        BDE_VG_SpawnSettings spawnSettings = BDE_VG_SpawnSettings.Get();
        if (!spawnSettings || !spawnSettings.SpawnPositions)
            return false;
        foreach (BDE_VG_SpawnPosition slot : spawnSettings.SpawnPositions)
        {
            if (slot && OverlapsReserved(candidate,slot.Position,clearance,height))
                return false;
        }
        return true;
    }

    protected static bool OverlapsReserved(vector candidate, vector slot, float clearance, float height)
    {
        if (Math.AbsFloat(candidate[1] - slot[1]) >= height)
            return false;
        float dx = candidate[0] - slot[0];
        float dz = candidate[2] - slot[2];
        return dx * dx + dz * dz < clearance * clearance;
    }

    protected static bool IsRelocationSpaceFree(EntityAI entity, vector entityPosition, vector orientation)
    {
        vector bounds[2];
        if (!entity.GetCollisionBox(bounds))
            return false;
        vector size = (bounds[1] - bounds[0]) * 0.95;
        if (size[0] <= 0 || size[1] <= 0 || size[2] <= 0)
            return false;
        vector center = (bounds[0] + bounds[1]) * 0.5;
        vector rotation[3];
        Math3D.YawPitchRollMatrix(orientation,rotation);
        center = entityPosition + center.Multiply3(rotation);
        array<Object> excluded = new array<Object>;
        excluded.Insert(entity);
        array<EntityAI> hierarchy = new array<EntityAI>;
        entity.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER,hierarchy);
        foreach (EntityAI child : hierarchy)
            excluded.Insert(child);
        array<Object> collisions = new array<Object>;
        // Unlike the normal vehicle-only spawn test, also reject walls,
        // buildings and other geometry at the relocation destination.
        return !g_Game.IsBoxCollidingGeometry(center,orientation,size,ObjIntersectGeom,ObjIntersectGeom,excluded,collisions);
    }

    protected static void MoveParkedVehicle(EntityAI entity, vector position, vector orientation)
    {
        Transport transport = Transport.Cast(entity);
        if (transport)
            transport.DisableSimulation(true);
        dBodyActive(entity,ActiveState.INACTIVE);
        SetVelocity(entity,vector.Zero);
        dBodySetAngularVelocity(entity,vector.Zero);
        entity.SetPosition(position);
        entity.SetOrientation(orientation);
        // Expansion maintains its own replicated physics position. Updating
        // only EntityAI's transform can leave clients seeing the old location.
        CarScript car = CarScript.Cast(entity);
        if (car && car.m_State)
        {
            ExpansionPhysicsState state = car.m_State;
            state.Init();
            entity.GetTransform(state.m_Transform);
            entity.GetTransform(state.m_TargetTransform);
            state.m_LinearVelocity = vector.Zero;
            state.m_AngularVelocity = vector.Zero;
            state.m_LastLinearVelocity = vector.Zero;
            state.m_LastAngularVelocity = vector.Zero;
            state.m_SyncLinearVelocity = vector.Zero;
            state.m_SyncAngularVelocity = vector.Zero;
            state.m_LinearVelocityX = 0;
            state.m_LinearVelocityY = 0;
            state.m_LinearVelocityZ = 0;
            state.m_AngularVelocityX = 0;
            state.m_AngularVelocityY = 0;
            state.m_AngularVelocityZ = 0;
            state.m_SyncOrientation = orientation;
            state.m_Time = entity.GetSimulationTimeStamp();
            state.m_TimeSince = 0;
        }
        if (transport)
            transport.DisableSimulation(false);
        dBodyActive(entity,ActiveState.ACTIVE);
        entity.SetSynchDirty();
    }


    // Search around the requested origin, not always the vehicle's old world coordinates.
    static bool FindAlternative(BDE_VG_VehicleData vehicleData, vector origin, vector orientation, out vector alternativePosition, out Object blockingObject)
    {
        alternativePosition = vector.Zero;
        blockingObject = null;
        if (!vehicleData)
            return false;

        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        if (!settings || settings.AlternativeSpawnMaximumRadiusMeters <= 0.0)
            return false;

        int kind = GetVehicleKind(vehicleData.m_ClassName);
        int ringCount = Math.Ceil(settings.AlternativeSpawnMaximumRadiusMeters / settings.AlternativeSpawnStepMeters);
        for (int ring = 1; ring <= ringCount; ring++)
        {
            float radius = ring * settings.AlternativeSpawnStepMeters;
            if (radius > settings.AlternativeSpawnMaximumRadiusMeters)
                break;
            for (int directionIndex = 0; directionIndex < settings.AlternativeSpawnDirections; directionIndex++)
            {
                float angleDegrees = (360.0 / settings.AlternativeSpawnDirections) * directionIndex;
                float angleRadians = angleDegrees * Math.DEG2RAD;
                vector candidate = origin;
                candidate[0] = origin[0] + Math.Sin(angleRadians) * radius;
                candidate[2] = origin[2] + Math.Cos(angleRadians) * radius;
                // Keep the terminal's expected elevation while looking for a
                // deck/floor. Terrain/water below a platform is not its surface.
                if (!ResolveSpawnSurface(vehicleData,candidate,kind,true))
                    continue;
                Object candidateBlocker;
                if (ExpansionItemSpawnHelper.IsSpawnPositionFree(candidate,orientation,vehicleData.m_ClassName,candidateBlocker))
                {
                    alternativePosition = candidate;
                    return true;
                }
                if (!blockingObject && candidateBlocker)
                    blockingObject = candidateBlocker;
            }
        }
        string blockedBy = "none (support/water/slope rejected)";
        if (blockingObject)
            blockedBy = blockingObject.GetType();
        Print("[BDE_VG Spawn] No alternative for " + vehicleData.m_ClassName + " around " + origin.ToString() + " radius=" + settings.AlternativeSpawnMaximumRadiusMeters.ToString() + " firstBlocker=" + blockedBy);
        return false;
    }

    // Resolve a driveable surface close to the authored point/terminal floor.
    // The narrow vertical range avoids selecting a roof far above the player
    // or dropping vehicles into the sea/terrain beneath an elevated trader.
    protected static bool FindSolidFloor(vector sample, out float floorHeight)
    {
        vector start = sample + Vector(0, 1.5, 0);
        vector finish = sample - Vector(0, 2.5, 0);
        RaycastRVParams query = new RaycastRVParams(start, finish);
        query.type = ObjIntersectGeom;
        query.sorted = true;
        query.flags = CollisionFlags.ALLOBJECTS;
        array<ref RaycastRVResult> hits = new array<ref RaycastRVResult>;
        if (DayZPhysics.RaycastRVProxy(query, hits))
        {
            foreach (RaycastRVResult hit : hits)
            {
                if (!hit)
                    continue;
                Object support = hit.obj;
                if (hit.parent)
                    support = hit.parent;
                // Never use players, parked vehicles or loose cargo as floors.
                if (support && (support.IsInherited(Man) || support.IsInherited(Transport) || ExpansionVehicle.Get(support)))
                    continue;
                if (support && support.IsInherited(ItemBase) && !support.IsInherited(BaseBuildingBase))
                    continue;
                if (hit.pos[1] < finish[1] || hit.pos[1] > start[1])
                    continue;
                floorHeight = hit.pos[1];
                return true;
            }
        }
        // Terrain may not be returned by every proxy query. It is a valid
        // fallback only at this elevation, never a distant surface below it.
        float terrain = g_Game.SurfaceY(sample[0], sample[2]);
        if (terrain < finish[1] || terrain > start[1])
            return false;
        floorHeight = terrain;
        return true;
    }

    protected static bool ResolveSpawnSurface(BDE_VG_VehicleData vehicleData, inout vector candidate, int kind, bool projectAlternative = false)
    {
        bool water = g_Game.SurfaceIsSea(candidate[0],candidate[2]) || g_Game.SurfaceIsPond(candidate[0],candidate[2]);
        if (kind == BDE_VG_KIND_BOAT)
        {
            if (!water)
                return false;
            if (projectAlternative)
            {
                vector waterSurface = ExpansionStatic.GetSurfaceWaterPosition(candidate);
                candidate[1] = waterSurface[1];
            }
            return true;
        }
        // Preserve existing aircraft placement; this correction targets land
        // vehicles whose terrain checks ignored constructed trader floors.
        if (kind == BDE_VG_KIND_AIRCRAFT)
        {
            if (water)
                return false;
            if (projectAlternative)
            {
                vector airSurface = ExpansionStatic.GetSurfaceWaterPosition(candidate);
                candidate[1] = airSurface[1];
            }
            return true;
        }
        float centerHeight;
        if (!FindSolidFloor(candidate,centerHeight))
            return false;
        vector surface = ExpansionStatic.GetSurfaceWaterPosition(candidate);
        if (water)
        {
            if (centerHeight <= surface[1] + 0.1)
                return false;
        }
        candidate[1] = centerHeight;
        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        vector minMax[2];
        float sampleRadius = 3.0;
        if (ExpansionStatic.GetCollisionBox(vehicleData.m_ClassName,minMax))
        {
            vector size = minMax[1] - minMax[0];
            sampleRadius = Math.Max(size[0],size[2]) * 0.35;
            sampleRadius = Math.Clamp(sampleRadius,1.0,6.0);
        }
        float h1;
        float h2;
        float h3;
        float h4;
        if (!FindSolidFloor(candidate + Vector(-sampleRadius,0,-sampleRadius),h1))
            return false;
        if (!FindSolidFloor(candidate + Vector(sampleRadius,0,-sampleRadius),h2))
            return false;
        if (!FindSolidFloor(candidate + Vector(-sampleRadius,0,sampleRadius),h3))
            return false;
        if (!FindSolidFloor(candidate + Vector(sampleRadius,0,sampleRadius),h4))
            return false;
        float minimumHeight = Math.Min(centerHeight,Math.Min(Math.Min(h1,h2),Math.Min(h3,h4)));
        float maximumHeight = Math.Max(centerHeight,Math.Max(Math.Max(h1,h2),Math.Max(h3,h4)));
        // A submerged edge is unsafe even when the platform centre is dry.
        if (water && minimumHeight <= surface[1] + 0.1)
            return false;
        return maximumHeight - minimumHeight <= settings.AlternativeSpawnMaximumTerrainDeltaMeters;
    }
};
