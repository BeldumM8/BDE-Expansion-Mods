class BDE_VG_SceneBuilder
{
    static int GetKind(string className)
    {
        // ExpansionVodnik inherits ExpansionBoatScript for its amphibious driving,
        // but belongs in the ground-vehicle preview scene.
        if (className == "ExpansionVodnik")
            return 0;
        if (g_Game.IsKindOf(className, "HelicopterScript") || g_Game.IsKindOf(className, "ExpansionHelicopterScript") || g_Game.IsKindOf(className, "ExpansionVehicleHelicopterBase") || g_Game.IsKindOf(className, "ExpansionVehiclePlaneBase"))
            return 1;
        if (g_Game.IsKindOf(className, "BoatScript") || g_Game.IsKindOf(className, "ExpansionBoatScript") || g_Game.IsKindOf(className, "ExpansionVehicleBoatBase"))
            return 2;
        return 0;
    }

    // Selected vehicle orientation comes from the exported scene's dummy.
    // The boat dummy is a raw P3D with a -pi/2 orientation (radians),
    // converted to the -90 degree yaw required by EntityAI.SetOrientation().
    static vector GetOrientation(int kind)
    {
        if (kind == 1)
            return "90.026001 0 0";
        if (kind == 2)
            return "-90 0 0";
        return "89.841537 0 0";
    }

    static float GetFloorOffset(int kind)
    {
        if (kind == 1)
            return -6.781341;
        return 0.243530;
    }

    // Sample suspension wheel centres in MODEL space. Attachment world
    // transforms can still be at the origin on the first local preview frame.
    // Validate all samples against the root model before using them.
    static bool GetStandingOffset(EntityAI vehicle, out float standingOffset, out float levelPitch, bool wheelContacts = true)
    {
        standingOffset = 0.0;
        levelPitch = 0.0;
        if (!vehicle)
            return false;
        vector bounds[2];
        if (!vehicle.GetCollisionBox(bounds))
            return false;
        vector size = bounds[1] - bounds[0];
        if (size[0] <= 0 || size[1] <= 0 || size[2] <= 0 || size.Length() > 200.0)
            return false;

        Car car = Car.Cast(vehicle);
        Motorbike bike = Motorbike.Cast(vehicle);
        int wheelCount;
        if (wheelContacts && car)
            wheelCount = car.WheelCount();
        else if (wheelContacts && bike)
            wheelCount = bike.WheelCount();
        ref array<vector> centres = new array<vector>;
        ref array<float> radii = new array<float>;
        ref array<CarWheel> contactWheels = new array<CarWheel>;
        float sumZ;
        float sumY;
        float sumZZ;
        float sumZY;
        for (int i = 0; i < wheelCount; i++)
        {
            CarWheel wheel;
            vector centre;
            if (car)
            {
                wheel = CarWheel.Cast(car.WheelGetEntity(i));
                centre = car.WheelGetPositionLS(i);
            }
            else
            {
                wheel = CarWheel.Cast(bike.WheelGetEntity(i));
                centre = bike.WheelGetPositionLS(i);
            }
            if (!wheel)
                continue;
            float radius = wheel.GetRadius();
            if (radius <= 0.01 || radius > size.Length() * 0.25 || centre == vector.Zero)
                continue;
            bool valid = true;
            // Hull collision does not include the attached tyres. Suspension
            // hubs legitimately extend below that box (M3S, HEMTT, Vodnik).
            for (int axis = 0; axis < 3; axis++)
            {
                float envelope = Math.Max(Math.AbsFloat(bounds[0][axis]), Math.AbsFloat(bounds[1][axis])) + size[axis] * 0.5 + radius;
                if (Math.AbsFloat(centre[axis]) > envelope)
                    valid = false;
            }
            if (!valid)
            {
                Print("[BDE_VG Preview] Rejected wheel " + vehicle.GetType() + " local=" + centre.ToString() + " radius=" + radius.ToString());
                continue;
            }
            // Prefer actual rendered tyre geometry once attachment transforms
            // are valid. Native suspension hubs can differ from the visible pose.
            vector visualCentre = vehicle.WorldToModel(wheel.GetPosition());
            if (vector.Distance(visualCentre, centre) <= radius * 3.0 && wheel.GetPosition() != vector.Zero)
                centre = visualCentre;
            else
                wheel = null;
            centres.Insert(centre);
            radii.Insert(radius);
            contactWheels.Insert(wheel);
            float bottom = centre[1] - radius;
            sumZ += centre[2];
            sumY += bottom;
            sumZZ += centre[2] * centre[2];
            sumZY += centre[2] * bottom;
        }

        if (centres.Count() >= 2)
        {
            float denominator = centres.Count() * sumZZ - sumZ * sumZ;
            if (denominator > 0.25)
                levelPitch = -Math.Atan((centres.Count() * sumZY - sumZ * sumY) / denominator) * Math.RAD2DEG;
            if (Math.AbsFloat(levelPitch) <= 15.0)
            {
                vector orientation = vehicle.GetOrientation();
                orientation[1] = levelPitch;
                vehicle.SetOrientation(orientation);
                vehicle.Update();
                for (int j = 0; j < centres.Count(); j++)
                {
                    vector worldCentre = vehicle.ModelToWorld(centres[j]);
                    float contact = worldCentre[1] - vehicle.GetPosition()[1] - radii[j];
                    if (contactWheels[j])
                    {
                        contactWheels[j].Update();
                        float visualBottom;
                        if (GetVisualBottom(contactWheels[j], vehicle.GetPosition()[1], visualBottom))
                            contact = visualBottom;
                    }
                    if (j == 0 || contact < standingOffset)
                        standingOffset = contact;
                }
                return true;
            }
        }
        // Skids, tracks and missing/uninitialized wheels: use collision support
        // at level attitude. No vehicle-specific height tables.
        levelPitch = 0;
        standingOffset = bounds[0][1];
        if (wheelContacts)
        {
            float bodyBottom;
            if (GetVisualBottom(vehicle, vehicle.GetPosition()[1], bodyBottom))
            {
                standingOffset = bodyBottom;
                return true;
            }
        }
        string supportPath = "CfgVehicles " + vehicle.GetType() + " modelZeroPointDistanceFromGround";
        if (g_Game.ConfigIsExisting(supportPath))
        {
            float height = g_Game.ConfigGetFloat(supportPath);
            if (height >= 0 && height <= size.Length())
                standingOffset = -height;
        }
        return true;
    }

    static bool GetVisualBottom(Object object, float rootY, out float bottom)
    {
        vector visualBounds[2];
        if (object.ClippingInfo(visualBounds) <= 0)
            return false;
        bool found;
        for (int x = 0; x < 2; x++)
        {
            for (int y = 0; y < 2; y++)
            {
                for (int z = 0; z < 2; z++)
                {
                    vector corner = object.ModelToWorld(Vector(visualBounds[x][0], visualBounds[y][1], visualBounds[z][2]));
                    float height = corner[1] - rootY;
                    if (!found || height < bottom)
                        bottom = height;
                    found = true;
                }
            }
        }
        return found;
    }

    static void AddPart(EntityAI parent, string className)
    {
        // Attachment-only: never create failed attachments in cargo or on the ground.
        parent.GetInventory().CreateAttachment(className);
    }

    static void PrepareDecoration(EntityAI vehicle)
    {
        string type = vehicle.GetType();
        if (type == "Offroad_02" || type == "Offroad_02_Beige")
        {
            string suffix;
            if (type == "Offroad_02_Beige")
                suffix = "_Beige";
            for (int wheel = 0; wheel < 4; wheel++)
                AddPart(vehicle, "Offroad_02_Wheel");
            AddPart(vehicle, "Offroad_02_Door_1_1" + suffix);
            AddPart(vehicle, "Offroad_02_Door_1_2" + suffix);
            AddPart(vehicle, "Offroad_02_Door_2_1" + suffix);
            AddPart(vehicle, "Offroad_02_Door_2_2" + suffix);
            AddPart(vehicle, "Offroad_02_Hood" + suffix);
            AddPart(vehicle, "Offroad_02_Trunk" + suffix);
        }
        else if (type == "Expansion_Landrover")
        {
            for (int lrWheel = 0; lrWheel < 5; lrWheel++)
                AddPart(vehicle, "Expansion_Landrover_Wheel");
            AddPart(vehicle, "Expansion_Landrover_Hood");
            AddPart(vehicle, "Expansion_Landrover_Driverdoor");
            AddPart(vehicle, "Expansion_Landrover_Codriverdoor");
            AddPart(vehicle, "Expansion_Landrover_Left");
            AddPart(vehicle, "Expansion_Landrover_Right");
            AddPart(vehicle, "Expansion_Landrover_Trunk");
        }
        else if (type == "ExpansionVodnik")
        {
            for (int vodnikWheel = 0; vodnikWheel < 4; vodnikWheel++)
                AddPart(vehicle, "ExpansionVodnikWheel");
            AddPart(vehicle, "ExpansionVodnikDoorDriver");
            AddPart(vehicle, "ExpansionVodnikDoorCoDriver");
        }
        else if (type == "Truck_01_Covered")
        {
            AddPart(vehicle, "Truck_01_Wheel");
            AddPart(vehicle, "Truck_01_Wheel");
            for (int doubleWheel = 0; doubleWheel < 4; doubleWheel++)
                AddPart(vehicle, "Truck_01_WheelDouble");
            AddPart(vehicle, "Truck_01_Door_1_1_Orange");
            AddPart(vehicle, "Truck_01_Door_2_1_Orange");
            AddPart(vehicle, "Truck_01_Hood_Orange");
        }
        else if (g_Game.IsKindOf(type, "ExpansionUh1h") || g_Game.IsKindOf(type, "ExpansionMh6"))
        {
            string skin = vehicle.ConfigGetString("skinName");
            if (skin != string.Empty)
                skin = "_" + skin;
            string doorPrefix = "ExpansionUh1hDoor_";
            if (g_Game.IsKindOf(type, "ExpansionMh6"))
                doorPrefix = "Expansion_Mh6_Door_";
            AddPart(vehicle, doorPrefix + "1_1" + skin);
            AddPart(vehicle, doorPrefix + "1_2" + skin);
            AddPart(vehicle, doorPrefix + "2_1" + skin);
            AddPart(vehicle, doorPrefix + "2_2" + skin);
        }
        else
        {
            return;
        }

        AddPart(vehicle, "HeadlightH7");
        if (GetKind(type) == 0)
            AddPart(vehicle, "HeadlightH7");
        CarScript car = CarScript.Cast(vehicle);
        if (car)
        {
            car.Leak(CarFluid.FUEL, car.GetFluidCapacity(CarFluid.FUEL));
            car.Leak(CarFluid.COOLANT, car.GetFluidCapacity(CarFluid.COOLANT));
            car.Leak(CarFluid.OIL, car.GetFluidCapacity(CarFluid.OIL));
        }
    }

    // Scene geometry is stationary, but MUST stay registered in the collision
    // world. Deactivating the bodies entirely can prevent vehicles from landing.
    static void KeepSceneCollision(EntityAI entity)
    {
        if (!entity)
            return;
        entity.SetAllowDamage(false);
        if (dBodyIsSet(entity))
        {
            dBodyDynamic(entity, false);
            Physics physics = entity.GetPhysics();
            if (physics)
                physics.ChangeSimulationState(SimulationState.COLLISION);
        }
        GameInventory inventory = entity.GetInventory();
        if (!inventory)
            return;
        for (int i = 0; i < inventory.AttachmentCount(); i++)
            KeepSceneCollision(inventory.GetAttachmentFromIndex(i));
    }

    // Keep vehicle simulation and animation updates enabled while holding the
    // preview rigid body stationary after alignment.
    static void FreezeVehicle(EntityAI vehicle)
    {
        Transport transport = Transport.Cast(vehicle);
        if (transport)
            transport.DisableSimulation(false);
        if (dBodyIsSet(vehicle))
            dBodyEnableGravity(vehicle, false);
        KeepSceneCollision(vehicle);
        Physics physics = vehicle.GetPhysics();
        if (physics)
            physics.ChangeSimulationState(SimulationState.SIMULATION);
        if (dBodyIsSet(vehicle))
            dBodyDynamic(vehicle, false);
    }

    // Use only the authored floor near its expected elevation. RaycastRV
    // contactDir is not always a unit surface normal; do not use dir[1] as one.
    // When local collision isn't registered yet, use the authored stage plane
    // and retry later instead of leaving the vehicle at staging height.
    static bool FindPreviewFloor(EntityAI vehicle, vector anchor, int kind, out float floorY, array<Object> sceneObjects)
    {
        float expectedY = anchor[1] + GetFloorOffset(kind);
        floorY = expectedY;
        vector position = vehicle.GetPosition();
        // Helicopter scenery shares the same flat pad as the selected preview.
        // Query its known-good centre instead of falling back at panel seams.
        if (kind == 1)
        {
            position[0] = anchor[0];
            position[2] = anchor[2];
        }
        vector start = Vector(position[0], expectedY + 2.0, position[2]);
        vector end = Vector(position[0], expectedY - 2.0, position[2]);
        RaycastRVParams params = new RaycastRVParams(start, end, vehicle);
        params.type = ObjIntersectGeom;
        params.sorted = true;
        params.flags = CollisionFlags.ALLOBJECTS;
        ref array<Object> excluded = new array<Object>;
        if (sceneObjects)
        {
            foreach (Object object : sceneObjects)
            {
                if (!object)
                    continue;
                string type = object.GetType();
                if (type != "StaticObj_Panel_Concrete_1" && type != "Land_Airfield_ServiceHangar_R" && type != "Land_Airfield_ServiceHangar_L")
                    excluded.Insert(object);
            }
        }
        ref array<ref RaycastRVResult> results = new array<ref RaycastRVResult>;
        if (DayZPhysics.RaycastRVProxy(params, results, excluded))
        {
            foreach (RaycastRVResult hit : results)
            {
                if (!hit)
                    continue;
                Object hitObject = hit.obj;
                if (hit.parent)
                    hitObject = hit.parent;
                if (!hitObject || !sceneObjects || sceneObjects.Find(hitObject) < 0)
                    continue;
                // A ray starts below roofs, and excludes vehicles/containers.
                // Highest retained surface is the top of the floor geometry.
                if (hit.pos[1] >= expectedY - 2.0 && hit.pos[1] <= expectedY + 2.0)
                {
                    floorY = hit.pos[1];
                    return true;
                }
            }
        }
        if (sceneObjects)
        {
            foreach (Object floorObject : sceneObjects)
            {
                if (!floorObject || floorObject.GetType() != "StaticObj_Panel_Concrete_1")
                    continue;
                vector floorBounds[2];
                if (!floorObject.GetCollisionBox(floorBounds))
                    continue;
                vector floorLocalPosition = floorObject.WorldToModel(position);
                if (floorLocalPosition[0] < floorBounds[0][0] || floorLocalPosition[0] > floorBounds[1][0] || floorLocalPosition[2] < floorBounds[0][2] || floorLocalPosition[2] > floorBounds[1][2])
                    continue;
                vector panelTop = floorObject.ModelToWorld(Vector(floorLocalPosition[0], floorBounds[1][1], floorLocalPosition[2]));
                // In the normal scene the hangar floor can be above this panel.
                if (kind == 1 || panelTop[1] > floorY)
                    floorY = panelTop[1];
            }
        }
        Print("[BDE_VG Preview] Using authored floor plane for " + vehicle.GetType() + " y=" + floorY.ToString());
        return true;
    }

    // Align support geometry to the isolated scene floor. Delayed passes use
    // the same absolute floor and local contacts, so offsets do not accumulate.
    // Boats retain the editor waterline and are frozen by the renderer.
    static bool PlaceVehicleOnSceneFloor(EntityAI vehicle, vector anchor, int kind, bool selected = false, array<Object> sceneObjects = null)
    {
        if (!vehicle)
            return false;

        vector position = vehicle.GetPosition();
        if (kind == 2)
        {
            // Decorative water has no matching ocean simulation surface.
            // Keep the existing editor waterline and hold the preview stationary.
            position[1] = anchor[1] - 0.15;
            vehicle.SetPosition(position);
            vehicle.Update();
            return true;
        }

        float floorY;
        if (!FindPreviewFloor(vehicle, anchor, kind, floorY, sceneObjects))
            return false;

        vector orientation = vehicle.GetOrientation();
        orientation[1] = 0;
        orientation[2] = 0;
        vehicle.SetOrientation(orientation);
        vehicle.Update();

        float standingOffset;
        float levelPitch;
        if (!GetStandingOffset(vehicle, standingOffset, levelPitch, kind == 0))
        {
            Print("[BDE_VG Preview] Vehicle has no usable wheel or collision contact: " + vehicle.GetType());
            return false;
        }

        orientation[1] = levelPitch;
        vehicle.SetOrientation(orientation);
        position[1] = floorY - standingOffset;
        vehicle.SetPosition(position);
        vehicle.Update();
        Print("[BDE_VG Preview] Placed " + vehicle.GetType() + " floor=" + floorY.ToString() + " contactOffset=" + standingOffset.ToString() + " pitch=" + levelPitch.ToString());
        return true;
    }

    static void Add(array<Object> objects, vector anchor, bool islocal, string type, vector offset, vector orientation, float scale)
    {
        vector position = anchor + offset;
        Object obj;
        if (type.Contains(".p3d"))
        {
            // Editor P3D rotations are radians; the native static factory uses them directly.
            obj = g_Game.CreateStaticObjectUsingP3D(type, position, orientation, scale, islocal);
        }
        else
        {
            int flags = ECE_CREATEPHYSICS | ECE_NOLIFETIME | ECE_NOPERSISTENCY_WORLD | ECE_NOPERSISTENCY_CHAR;
            if (islocal)
                flags = flags | ECE_LOCAL;
            obj = g_Game.CreateObjectEx(type, position, flags);
            if (obj)
                obj.SetOrientation(orientation);
        }
        if (!obj)
        {
            Print("[BDE_VG scenes] Failed to create " + type);
            return;
        }
        obj.SetPosition(position);
        obj.SetScale(scale);
        obj.Update();
        EntityAI entity = EntityAI.Cast(obj);
        if (entity)
        {
            entity.SetAllowDamage(false);
            PrepareDecoration(entity);
            Transport vehicle = Transport.Cast(entity);
            if (!vehicle)
                KeepSceneCollision(entity);
            // Scenery vehicles are aligned after ALL stage collision geometry
            // has spawned, not while the scene is still being constructed.
        }
        objects.Insert(obj);
    }

    // Exact scene exports imported from user-provided DayZ Editor SpawnObject files.
    // All offsets are editor world positions minus the omitted dummy anchor.
    // Never instantiate the dummy: the current selected vehicle takes its position.
    static void Build(int kind, vector anchor, bool islocal, array<Object> objects)
    {
        if (kind == 0)
        {
            Add(objects, anchor, islocal, "Land_Airfield_ServiceHangar_R", "-17.868164 0.243546 24.669922", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Airfield_ServiceHangar_L", "-17.868164 0.243958 -25.263671", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "16.697265 -1.074875 -1.808593", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "Land_Wreck_offroad02_aban1", "7.293945 2.107254 -14.589843", "126.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Wreck_Offroad02_Medic_TK_DE", "10.082031 0.226593 12.763672", "-125.999992 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh", "7.038086 1.457291 -24.187500", "90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Bo", "14.132812 1.457291 -23.581054", "81.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Moh_DE", "20.510742 1.457291 -21.062500", "54.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Bo_DE", "24.941406 1.457291 -15.868164", "27.000008 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_ContainerLocked_Yellow_DE", "27.082031 1.457291 -9.621093", "9.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh_DE", "27.530273 1.457291 -3.309570", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh", "6.133789 1.457291 19.274414", "90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Mo_DE", "12.599609 1.457291 18.577149", "99.000008 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Mo", "18.933593 1.457291 16.347657", "117.000015 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh", "24.092773 1.457291 12.595704", "-45.000000 0.000000 0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Moh_DE", "27.020507 1.457291 6.666993", "-9.000000 0.000000 0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh", "30.022461 1.457291 1.854493", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh_DE", "30.166992 1.457291 -7.263671", "9.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Mo", "28.693359 1.457291 -14.365234", "27.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Bo", "25.092773 1.457291 -21.154296", "45.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh_DE", "18.675781 1.457291 -25.671875", "72.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh", "10.785156 1.457291 -26.974609", "81.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_ContainerLocked_Blue_DE", "28.772461 1.457291 11.061524", "-27.000000 0.000000 0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Bo", "23.630859 1.457291 16.907227", "-54.000000 0.000000 0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Aoh", "16.679687 1.457291 20.518555", "-72.000000 0.000000 0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Container_1Bo", "9.151367 1.457291 22.294922", "99.000008 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_lamppost1_easy_dn", "-0.214844 -3.964752 -6.603515", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_lamppost1_easy_dn", "-0.715821 -3.964752 6.714844", "-179.999985 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_projector_dn", "25.474609 3.648056 -0.017578", "90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Power_Pole_Wood1_Trail_B", "25.739257 0.244308 -0.119140", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_projector_dn", "-20.037110 3.648056 -0.017578", "-89.999969 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Power_Pole_Wood1_Trail_B", "-20.322266 0.244308 -0.125000", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Truck_01_Covered", "17.444336 0.171112 -11.093750", "32.589619 -0.721126 -0.212233", 1.000000);
            Add(objects, anchor, islocal, "ExpansionVodnik", "-6.473633 0.083588 -14.035156", "63.946552 0.100964 0.007396", 1.000000);
            Add(objects, anchor, islocal, "Expansion_Landrover", "-9.078125 0.083176 10.000977", "58.843079 0.100433 0.005236", 1.000000);
        }
        else if (kind == 1)
        {
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "2.041016 -5.362030 34.120117", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "0.529297 -5.362030 34.120117", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "2.021485 -5.362030 39.753906", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "1.311524 -4.084900 40.328125", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "1.331055 -4.084900 34.694336", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "0.509766 -5.362030 39.753906", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "0.488282 -5.362030 45.336914", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "2.000000 -5.362030 45.336914", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "1.290039 -4.084900 45.911132", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.215821 -5.362030 36.036132", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.505860 -4.084900 36.003906", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.229492 -5.362030 24.759765", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.519532 -4.084900 24.727539", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.212891 -5.362030 30.425781", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.502930 -4.084900 30.393554", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.172852 -5.362030 41.628906", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.462891 -4.084900 41.596679", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.156250 -5.362030 47.294921", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.446289 -4.084900 47.262695", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "10.329102 -5.362030 52.504882", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "10.354492 -5.362030 50.850586", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "15.932617 -5.362030 52.492187", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "15.958008 -5.362030 50.837890", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "21.583008 -5.362030 52.468750", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "21.608399 -5.362030 50.814453", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "21.626953 -4.084900 51.623046", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-6.581054 -5.362030 52.574218", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-6.555664 -5.362030 50.919921", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-6.537109 -4.084900 51.728515", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-0.977539 -5.362030 52.561523", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-0.952148 -5.362030 50.907226", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-0.933593 -4.084900 51.715820", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "4.672852 -5.362030 52.538086", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "4.698242 -5.362030 50.883789", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "4.716797 -4.084900 51.692382", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-23.492187 -5.362030 50.969726", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-23.473633 -4.084900 51.778320", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-17.888672 -5.362030 50.957031", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-17.870117 -4.084900 51.765625", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-12.238281 -5.362030 50.933593", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-12.219726 -4.084900 51.742187", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.717774 -5.362030 24.759765", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.701172 -5.362030 30.425781", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.116211 -5.362030 25.099609", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.604492 -5.362030 25.099609", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.314453 -4.084900 25.067382", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.131836 -5.362030 30.717773", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.620117 -5.362030 30.717773", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.330078 -4.084900 30.685546", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.148437 -5.362030 36.383789", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.636718 -5.362030 36.383789", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.346679 -4.084900 36.351562", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.144531 -5.362030 41.871093", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.632812 -5.362030 41.871093", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.342773 -4.084900 41.838867", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.160156 -5.362030 47.489257", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.648437 -5.362030 47.489257", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.358398 -4.084900 47.457031", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.037109 -5.362030 52.608398", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.011718 -5.362030 50.954101", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-28.993164 -4.084900 51.762695", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Airfield_Hangar_Desert", "3.168946 0.581879 -58.048829", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.661133 -5.362030 41.628906", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "29.547852 -6.800049 -42.882813", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "-29.456054 -6.800049 -44.150391", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "58.163086 -6.800049 -42.787110", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "58.165039 -6.781341 -7.375000", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "58.239258 -6.781341 51.470703", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "-58.607422 -6.800049 -44.244141", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "-58.747070 -6.781341 -7.664063", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "-58.375976 -6.781341 51.731445", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "Land_psilight_projector_dn", "21.385742 -0.968658 -16.620118", "135.000015 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Power_Pole_Wood1_Trail_B", "21.603516 -4.372406 -16.953125", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_projector_dn", "-20.211914 -0.968658 -16.475586", "-135.000015 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Power_Pole_Wood1_Trail_B", "-20.413086 -4.372406 -16.791016", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_projector_dn", "20.832032 -0.968658 18.453125", "45.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Power_Pole_Wood1_Trail_B", "21.062500 -4.372406 18.553711", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "10.373047 -4.084900 51.659179", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "15.976563 -4.084900 51.646484", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.644532 -5.362030 47.294921", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.704102 -5.362030 36.036132", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-23.517578 -5.362030 52.624023", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-17.914062 -5.362030 52.611328", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-12.263672 -5.362030 52.587890", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "0.234375 -6.781341 -7.526368", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "0.308594 -6.781341 51.319336", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "-29.391601 -6.781341 -7.538086", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "-29.317383 -6.781341 51.307617", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "29.549805 -6.781341 -7.470704", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "29.624024 -6.781341 51.375000", "0.000000 0.000000 -0.000000", 10.000000);
            Add(objects, anchor, islocal, "ExpansionUh1h_Uber", "-13.949218 -5.268249 36.281250", "62.308338 -1.428631 0.000383", 1.000000);
            Add(objects, anchor, islocal, "ExpansionMh6", "13.910157 -5.303421 36.519531", "-145.246658 -0.478966 0.000246", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "4.788086 -5.362030 -24.676758", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "4.813477 -5.362030 -26.331055", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "4.832032 -4.084900 -25.522461", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "10.438477 -5.362030 -24.700196", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "10.463867 -5.362030 -26.354493", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "10.482422 -4.084900 -25.545899", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "16.041992 -5.362030 -24.712891", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "16.067383 -5.362030 -26.367188", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "16.085938 -4.084900 -25.558594", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "21.692383 -5.362030 -24.736329", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "21.717774 -5.362030 -26.390625", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "21.736328 -4.084900 -25.582032", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_projector_dn", "-25.155273 -0.968658 23.366211", "-135.000015 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Power_Pole_Wood1_Trail_B", "-25.356445 -4.372406 23.050781", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.723633 -5.362030 -25.737305", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.235352 -5.362030 -25.737305", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.525391 -4.084900 -25.769532", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.707032 -5.362030 -20.071289", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.218750 -5.362030 -20.071289", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.508789 -4.084900 -20.103516", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.709961 -5.362030 -14.460938", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.221680 -5.362030 -14.460938", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.511719 -4.084900 -14.493164", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.220703 -5.362030 2.385742", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.510742 -4.084900 2.353515", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.722657 -5.362030 -8.890625", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.234375 -5.362030 -8.890625", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.524414 -4.084900 -8.922852", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.706055 -5.362030 -3.224610", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.217774 -5.362030 -3.224610", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.507813 -4.084900 -3.256836", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.708985 -5.362030 2.385742", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.253907 -5.362030 19.208007", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.543946 -4.084900 19.175781", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.755860 -5.362030 7.931640", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.267578 -5.362030 7.931640", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.557617 -4.084900 7.899414", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.739258 -5.362030 13.597656", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "28.250977 -5.362030 13.597656", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "27.541016 -4.084900 13.565429", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "26.742188 -5.362030 19.208007", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-17.751953 -5.362030 -24.598633", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-17.726562 -5.362030 -26.252930", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-17.708008 -4.084900 -25.444336", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-12.101562 -5.362030 -24.622071", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-12.076172 -5.362030 -26.276368", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-12.057617 -4.084900 -25.467774", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-6.498047 -5.362030 -24.634766", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-6.472656 -5.362030 -26.289063", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-6.454101 -4.084900 -25.480469", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-0.847656 -5.362030 -24.658204", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-0.822265 -5.362030 -26.312500", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-0.803711 -4.084900 -25.503907", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.007812 -5.362030 -24.565430", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-28.982422 -5.362030 -26.219727", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-28.963867 -4.084900 -25.411133", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-23.357422 -5.362030 -24.588868", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-23.332031 -5.362030 -26.243164", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-23.313476 -4.084900 -25.434571", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.090820 -5.362030 -25.578125", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.579101 -5.362030 -25.578125", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.289062 -4.084900 -25.610352", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.107422 -5.362030 -19.912110", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.595703 -5.362030 -19.912110", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.305664 -4.084900 -19.944336", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_projector_dn", "25.556641 -0.968658 23.656250", "135.000015 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Power_Pole_Wood1_Trail_B", "25.774414 -4.372406 23.323242", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.113281 -5.362030 -14.319336", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.601562 -5.362030 -14.319336", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.311523 -4.084900 -14.351563", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.129883 -5.362030 -8.653321", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.618164 -5.362030 -8.653321", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.328125 -4.084900 -8.685547", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.092773 -5.362030 -3.050782", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.581054 -5.362030 -3.050782", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.291015 -4.084900 -3.083008", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.109375 -5.362030 2.615234", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.597656 -5.362030 2.615234", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.307617 -4.084900 2.583007", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.125000 -5.362030 8.233398", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.613281 -5.362030 8.233398", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.323242 -4.084900 8.201171", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.141601 -5.362030 13.899414", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.629883 -5.362030 13.899414", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.339843 -4.084900 13.867187", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-31.101562 -5.362030 19.508789", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-29.589843 -5.362030 19.508789", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-30.299804 -4.084900 19.476562", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_projector_dn", "-20.740234 -0.968658 18.616211", "-45.000000 0.000000 0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Power_Pole_Wood1_Trail_B", "-20.954101 -4.372406 18.715820", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "20.948242 -5.362030 22.929687", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "20.973633 -5.362030 21.275390", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "21.741211 -4.084900 22.079101", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-28.282226 -5.362030 22.758789", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-28.256836 -5.362030 21.104492", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "-28.889648 -4.084900 21.913086", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "0.565430 -5.362030 22.853515", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "2.077149 -5.362030 22.853515", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "1.367188 -4.084900 23.427734", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "0.545899 -5.362030 28.487304", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "2.057617 -5.362030 28.487304", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_HBarrier_5_TK", "1.347657 -4.084900 29.061523", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Panel_Concrete_1", "-0.046875 -6.800049 -43.662110", "0.000000 0.000000 -0.000000", 10.000000);
        }
        else if (kind == 2)
        {
            Add(objects, anchor, islocal, "Land_Underground_WaterReservoir_Water", "19.428711 0.022430 6.856445", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Underground_WaterReservoir_Water", "19.432617 0.022430 22.303711", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Underground_WaterReservoir_Water", "19.431641 0.022430 -23.838867", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Underground_WaterReservoir_Water", "19.435547 0.022430 -8.391602", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Underground_WaterReservoir_Water", "-22.137695 0.022430 6.853515", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Underground_WaterReservoir_Water", "-22.133789 0.022430 22.300781", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Underground_WaterReservoir_Water", "-22.134765 0.022430 -23.841797", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_Underground_WaterReservoir_Water", "-22.130859 0.022430 -8.394531", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_inv_lamppost1_dn", "10.207031 3.777878 -8.148438", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_inv_lamppost1_dn", "9.042969 3.777878 9.026367", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_inv_lamppost1_dn", "-10.387695 3.777878 -8.683594", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "Land_psilight_inv_lamppost1_dn", "-11.551758 3.777878 8.491211", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "-39.917969 -21.850830 18.566406", "-179.999985 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "-39.924804 -22.089142 -21.369141", "-179.999985 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "45.770508 -22.089142 -16.999024", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "45.779297 -22.089142 22.985351", "0.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "16.250977 -22.101868 33.844726", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "-23.701172 -22.101868 33.849609", "-90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "-12.213867 -22.101868 -37.670899", "90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "27.757813 -22.101868 -37.674805", "90.000000 0.000000 -0.000000", 1.000000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "32.004883 -41.053421 -3.938477", "0.000000 0.000000 -0.000000", 1.500000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "5.820313 -41.053421 -3.914063", "0.000000 0.000000 -0.000000", 1.500000);
            Add(objects, anchor, islocal, "StaticObj_Pier_Concrete1", "-18.322265 -41.053421 -3.909180", "0.000000 0.000000 -0.000000", 1.500000);
        }
    }
}
