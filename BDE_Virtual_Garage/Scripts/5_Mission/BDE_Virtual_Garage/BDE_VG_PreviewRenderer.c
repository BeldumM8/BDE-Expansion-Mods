// Client-local preview scene. No preview vehicle is ever restored or spawned on
// the server; it is recreated from the authorized garage listing snapshot.
class BDE_VG_PreviewRenderer
{
    protected Widget m_PreviewViewport;
    protected ItemPreviewWidget m_FallbackPreviewWidget;
    protected PlayerBase m_Player;
    protected EntityAI m_RootEntity;
    protected ScriptedLightBase m_KeyLight;
    protected ScriptedLightBase m_FillLight;
    protected ref array<Object> m_SceneObjects;
    protected int m_SceneKind = -1;
    protected bool m_SceneCreated;
    protected int m_GroundingPasses;
    protected float m_GroundingDelay;
    protected bool m_EmptyHangarMode;
    protected vector m_SceneAnchor;
    protected bool m_HasSceneAnchor;
    protected float m_EmptyHangarLookHeightAboveFloor = 1.25;
    protected float m_EmptyHangarVirtualRadius = 2.50;
    protected float m_PreviewYawDegrees = 42.0;
    protected float m_PreviewElevationDegrees = 6.0;
    protected float m_FieldOfView = 0.72;
    // Preserve the v2.6.3 camera framing/rail behaviour.
    protected float m_CameraDistanceMultiplier = 0.46;
    protected float m_LargeVehicleDistanceBoostPerRadiusMetre = 0.045;
    protected float m_LargeVehicleMaximumMultiplierBoost = 0.35;
    protected float m_CameraMaximumCalculatedDistance = 30.0;
    protected float m_CameraRailMinimumHalfWidth = 3.25;
    protected float m_CameraRailMaximumHalfWidth = 13.50;
    protected float m_CameraRailMinimumHalfDepth = 4.50;
    protected float m_CameraRailMaximumHalfDepth = 16.00;
    protected float m_ViewportCenterCorrection = 1.55;
    protected bool m_SpectatorSelected;
    protected bool m_HasBounds;
    protected vector m_BoundsMinimum;
    protected vector m_BoundsMaximum;
    protected vector m_ModelCenter;
    protected float m_ModelRadius = 2.0;

    void BDE_VG_PreviewRenderer(Widget previewViewport, ItemPreviewWidget fallbackPreviewWidget)
    {
        m_PreviewViewport = previewViewport;
        m_FallbackPreviewWidget = fallbackPreviewWidget;
        m_SceneObjects = new array<Object>;
        if (m_FallbackPreviewWidget)
            m_FallbackPreviewWidget.Show(false);
    }

    void ~BDE_VG_PreviewRenderer()
    {
        Clear();
        m_PreviewViewport = null;
        m_FallbackPreviewWidget = null;
    }

    bool OwnsWidget(Widget widget)
    {
        Widget currentWidget = widget;
        while (currentWidget)
        {
            if (currentWidget == m_PreviewViewport || currentWidget == m_FallbackPreviewWidget)
                return true;
            currentWidget = currentWidget.GetParent();
        }
        return false;
    }

    // Compute one fixed local stage per menu session, roughly 400m sideways
    // from the player. The elevation isolates it from terrain and existing
    // buildings; clamping keeps it inside the world at coastal map edges.
    protected bool EnsureLocalAnchor()
    {
        if (m_HasSceneAnchor)
            return true;
        if (!m_Player)
            m_Player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!m_Player)
            return false;

        vector playerPosition = m_Player.GetPosition();
        float heading = m_Player.GetOrientation()[0] * Math.DEG2RAD;
        vector offset = Vector(Math.Cos(heading) * 375.0, 0.0, -Math.Sin(heading) * 375.0);
        vector anchor = playerPosition + offset;
        float mapSize = g_Game.GetWorld().GetWorldSize();
        if (anchor[0] < 90.0 || anchor[2] < 90.0 || anchor[0] > mapSize - 90.0 || anchor[2] > mapSize - 90.0)
            anchor = playerPosition - offset;
        anchor[0] = Math.Clamp(anchor[0], 90.0, mapSize - 90.0);
        anchor[2] = Math.Clamp(anchor[2], 90.0, mapSize - 90.0);

        float highGround = playerPosition[1];
        float ground = g_Game.SurfaceY(anchor[0], anchor[2]);
        if (ground > highGround) highGround = ground;
        ground = g_Game.SurfaceY(anchor[0] - 40.0, anchor[2] - 40.0);
        if (ground > highGround) highGround = ground;
        ground = g_Game.SurfaceY(anchor[0] + 40.0, anchor[2] - 40.0);
        if (ground > highGround) highGround = ground;
        ground = g_Game.SurfaceY(anchor[0] - 40.0, anchor[2] + 40.0);
        if (ground > highGround) highGround = ground;
        ground = g_Game.SurfaceY(anchor[0] + 40.0, anchor[2] + 40.0);
        if (ground > highGround) highGround = ground;
        anchor[1] = highGround + 90.0;

        m_SceneAnchor = anchor;
        m_HasSceneAnchor = true;
        Print("[BDE_VG Preview] Client-local stage anchor=" + anchor.ToString() + " player=" + playerPosition.ToString());
        return true;
    }

    protected bool EnsureLocalScene(int kind)
    {
        if (m_SceneCreated && m_SceneKind == kind)
            return true;
        if (!EnsureLocalAnchor())
            return false;

        DestroyLocalScene();
        m_SceneKind = kind;
        m_CameraMaximumCalculatedDistance = 30.0;
        if (kind == 1) m_CameraMaximumCalculatedDistance = 60.0;
        if (kind == 2) m_CameraMaximumCalculatedDistance = 40.0;
        if (BDE_VG_SceneModule.Ready && BDE_VG_SceneModule.Settings)
            m_CameraMaximumCalculatedDistance = BDE_VG_SceneModule.Settings.GetMaximumDistance(kind);
        m_CameraRailMaximumHalfWidth = m_CameraMaximumCalculatedDistance;
        m_CameraRailMaximumHalfDepth = m_CameraMaximumCalculatedDistance;

        // islocal=true ensures every scene prop is ECE_LOCAL (or P3D local).
        BDE_VG_SceneBuilder.Build(kind, m_SceneAnchor, true, m_SceneObjects);
        // All authored floor collision must exist before positioning the
        // scenery vehicles. This is client-only and runs once per stage build.
        for (int sceneIndex = 0; sceneIndex < m_SceneObjects.Count(); sceneIndex++)
        {
            Transport sceneryVehicle = Transport.Cast(m_SceneObjects[sceneIndex]);
            if (!sceneryVehicle)
                continue;
            EntityAI sceneryEntity = EntityAI.Cast(sceneryVehicle);
            BDE_VG_SceneBuilder.PlaceVehicleOnSceneFloor(sceneryEntity, m_SceneAnchor, kind, false, m_SceneObjects);
            BDE_VG_SceneBuilder.FreezeVehicle(sceneryEntity);
        }
        m_SceneCreated = m_SceneObjects.Count() > 0;
        ScheduleGrounding();
        if (m_SceneCreated)
        {
            // These are DayZ client-only point lights. Two shadowless sources
            // improve the readability of both the vehicle and its scene.
            m_KeyLight = ScriptedLightBase.CreateLight(BDE_VG_PreviewLight, m_SceneAnchor + Vector(7.0,9.0,6.0));
            m_FillLight = ScriptedLightBase.CreateLight(BDE_VG_PreviewLight, m_SceneAnchor + Vector(-7.0,7.0,-5.0));
        }
        Print("[BDE_VG Preview] Local scene kind=" + kind.ToString() + " objects=" + m_SceneObjects.Count().ToString());
        return m_SceneCreated;
    }

    protected void DestroyLocalScene()
    {
        if (m_KeyLight)
            m_KeyLight.Destroy();
        if (m_FillLight)
            m_FillLight.Destroy();
        m_KeyLight = null;
        m_FillLight = null;
        for (int i = m_SceneObjects.Count() - 1; i >= 0; i--)
        {
            if (m_SceneObjects[i])
                g_Game.ObjectDelete(m_SceneObjects[i]);
        }
        m_SceneObjects.Clear();
        m_SceneCreated = false;
        m_SceneKind = -1;
        m_GroundingPasses = 0;
    }

    protected void ApplyEmptyHangarGeometry()
    {
        m_ModelCenter = m_SceneAnchor;
        m_ModelCenter[1] = m_ModelCenter[1] + BDE_VG_SceneBuilder.GetFloorOffset(m_SceneKind) + m_EmptyHangarLookHeightAboveFloor;
        m_ModelRadius = m_EmptyHangarVirtualRadius;
        m_HasBounds = false;
    }

    void ShowEmptyHangar()
    {
        ClearVehicleLocal();
        m_EmptyHangarMode = true;
        if (!EnsureLocalScene(0))
            return;
        ApplyEmptyHangarGeometry();
        if (UpdateCameraGeometry())
            ActivateSpectatorCamera();
    }

    void ShowVehicle(BDE_VG_VehicleData data)
    {
        ClearVehicleLocal();
        if (!data || data.m_ClassName == string.Empty)
        {
            ShowEmptyHangar();
            return;
        }
        int kind = BDE_VG_SceneBuilder.GetKind(data.m_ClassName);
        if (!EnsureLocalScene(kind))
        {
            Print("[BDE_VG Preview] Failed to build local preview scene for " + data.m_ClassName);
            return;
        }

        vector stagingPosition = m_SceneAnchor;
        stagingPosition[1] = stagingPosition[1] + 10.0;
        // Like the vanilla script-console preview, this entity is client-local
        // and preview-only. It never appears in persistence or network traffic.
        // DayZGame.m_IsPreviewSpawn only exists in developer builds.
        // Release builds use the same client-local ECE_LOCAL spawn without it.
        #ifdef DEVELOPER
        DayZGame.m_IsPreviewSpawn = true;
        #endif
        m_RootEntity = EntityAI.Cast(g_Game.CreateObjectEx(data.m_ClassName, stagingPosition, ECE_LOCAL | ECE_NOLIFETIME | ECE_CREATEPHYSICS | ECE_NOPERSISTENCY_WORLD | ECE_NOPERSISTENCY_CHAR));
        #ifdef DEVELOPER
        DayZGame.m_IsPreviewSpawn = false;
        #endif
        if (!m_RootEntity)
        {
            Print("[BDE_VG Preview] Could not locally create " + data.m_ClassName);
            return;
        }
        m_RootEntity.SetAllowDamage(false);
        m_RootEntity.SetOrientation(BDE_VG_SceneBuilder.GetOrientation(kind));
        if (m_RootEntity.HasSelection("antiwater"))
            m_RootEntity.HideSelection("antiwater");

        RecreateAttachments(data);
        Construction.ExpansionBuildFullIfSupported(m_RootEntity);
        ExpansionVehicle vehicle;
        if (data.m_SkinIndex > -1 && ExpansionVehicle.Get(vehicle,m_RootEntity) && vehicle.HasSkin(data.m_SkinIndex))
            vehicle.SetSkin(data.m_SkinIndex);

        // Position against the local stage's physical floor immediately;
        // client-only vehicle simulation is not reliable for gravity settling.
        PlaceLocalVehicleOnFloor();
        ScheduleGrounding();
        m_EmptyHangarMode = false;
        CalculateHierarchyBounds();
        if (UpdateCameraGeometry())
            ActivateSpectatorCamera();
        Print("[BDE_VG Preview] Client-only preview ready: " + data.m_ClassName);
    }

    protected void RecreateAttachments(BDE_VG_VehicleData data)
    {
        if (!data.m_PreviewAttachments || data.m_PreviewAttachments.Count() == 0)
        {
            // Legacy records may predate the saved attachment snapshot.
            BDE_VG_SceneBuilder.PrepareDecoration(m_RootEntity);
            return;
        }
        ref array<EntityAI> created = new array<EntityAI>;
        foreach (BDE_VG_AttachmentPreviewData attachment : data.m_PreviewAttachments)
        {
            EntityAI parent = m_RootEntity;
            if (attachment && attachment.m_ParentIndex >= 0)
            {
                if (attachment.m_ParentIndex < created.Count())
                    parent = created[attachment.m_ParentIndex];
                else
                    parent = null;
            }
            EntityAI part;
            if (attachment && parent && parent.GetInventory() && attachment.m_ClassName != string.Empty)
            {
                if (attachment.m_SlotID >= 0)
                    part = parent.GetInventory().CreateAttachmentEx(attachment.m_ClassName,attachment.m_SlotID);
                if (!part)
                    part = parent.GetInventory().CreateAttachment(attachment.m_ClassName);
                if (part)
                    part.SetAllowDamage(false);
            }
            created.Insert(part); // Keep snapshot indexes stable even on failure.
        }
    }

    // Deterministic placement, with two delayed passes for attachment/suspension
    // transforms. No gravity settling in the elevated client-only scene.
    protected void PlaceLocalVehicleOnFloor()
    {
        if (!m_RootEntity)
            return;
        BDE_VG_SceneBuilder.PlaceVehicleOnSceneFloor(m_RootEntity, m_SceneAnchor, m_SceneKind, true, m_SceneObjects);
        BDE_VG_SceneBuilder.FreezeVehicle(m_RootEntity);
    }

    protected bool ActivateSpectatorCamera()
    {
        if (!m_Player)
            m_Player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!m_Player || !m_Player.GetIdentity())
            return false;
        vector cameraPosition = BDE_VG_PreviewCameraState.GetCameraPosition();
        BDE_VG_PreviewCameraState.Activate(cameraPosition,BDE_VG_PreviewCameraState.GetLookAtPosition(),m_FieldOfView);
        if (m_SpectatorSelected)
            return true;
        g_Game.SelectPlayer(m_Player.GetIdentity(),m_Player);
        g_Game.SelectSpectator(m_Player.GetIdentity(),"BDE_VG_SpectatorCamera",cameraPosition);
        m_SpectatorSelected = true;
        return true;
    }

    protected void ReturnToPlayerCamera()
    {
        BDE_VG_PreviewCameraState.Deactivate();
        if (!m_SpectatorSelected)
            return;
        if (!m_Player)
            m_Player = PlayerBase.Cast(g_Game.GetPlayer());
        if (m_Player && m_Player.GetIdentity())
            g_Game.SelectPlayer(m_Player.GetIdentity(),m_Player);
        m_SpectatorSelected = false;
    }

    protected void ScheduleGrounding()
    {
        m_GroundingPasses = 0;
        m_GroundingDelay = 0;
        if (m_SceneKind != 2)
        {
            m_GroundingPasses = 2;
            m_GroundingDelay = 0.25;
        }
    }

    protected void UpdateGrounding(float timeslice)
    {
        if (m_GroundingPasses <= 0 || m_SceneKind == 2)
            return;
        m_GroundingDelay -= timeslice;
        if (m_GroundingDelay > 0)
            return;
        m_GroundingPasses--;
        m_GroundingDelay = 0.75;
        for (int i = 0; i < m_SceneObjects.Count(); i++)
        {
            EntityAI decoration = EntityAI.Cast(m_SceneObjects[i]);
            if (!Transport.Cast(decoration))
                continue;
            BDE_VG_SceneBuilder.PlaceVehicleOnSceneFloor(decoration, m_SceneAnchor, m_SceneKind, false, m_SceneObjects);
            BDE_VG_SceneBuilder.FreezeVehicle(decoration);
        }
        if (m_RootEntity)
            PlaceLocalVehicleOnFloor();
    }

    void Update(float timeslice)
    {
        if (!m_HasSceneAnchor)
            return;
        UpdateGrounding(timeslice);
        if (m_RootEntity)
            CalculateHierarchyBounds();
        else if (m_EmptyHangarMode)
            ApplyEmptyHangarGeometry();
        else
            return;
        // Some mission/mod camera controllers can replace a spectator after
        // the garage has opened. Detect that and reclaim the garage preview
        // instead of letting mouse-wheel orbit silently stop working.
        if (m_SpectatorSelected && !BDE_VG_SpectatorCamera.Cast(Camera.GetCurrentCamera()))
        {
            m_SpectatorSelected = false;
            Print("[BDE_VG Preview] Preview camera was replaced; reactivating client-local spectator");
        }
        if (UpdateCameraGeometry() && !m_SpectatorSelected)
            ActivateSpectatorCamera();
    }

    protected bool UpdateCameraGeometry()
    {
        if ((!m_RootEntity && !m_EmptyHangarMode) || !m_PreviewViewport)
            return false;

        float viewportX;
        float viewportY;
        float viewportWidth;
        float viewportHeight;
        m_PreviewViewport.GetScreenPos(viewportX,viewportY);
        m_PreviewViewport.GetScreenSize(viewportWidth,viewportHeight);

        int screenWidth;
        int screenHeight;
        GetScreenSize(screenWidth,screenHeight);
        if (screenWidth <= 0 || screenHeight <= 0 || viewportWidth <= 1.0 || viewportHeight <= 1.0)
            return false;

        float screenAspect = (screenWidth * 1.0) / screenHeight;
        float horizontalFieldOfView = 2.0 * Math.Atan(Math.Tan(m_FieldOfView * 0.5) * screenAspect);
        float viewportCenterX = viewportX + viewportWidth * 0.5;
        float viewportCenterY = viewportY + viewportHeight * 0.5;
        float normalizedCenterX = ((viewportCenterX / screenWidth) - 0.5) * 2.0;
        float normalizedCenterY = ((viewportCenterY / screenHeight) - 0.5) * 2.0;

        float horizontalHalfRatio = (viewportWidth / screenWidth) * 0.82;
        float verticalHalfRatio = (viewportHeight / screenHeight) * 0.82;
        float horizontalHalfAngle = Math.Atan(horizontalHalfRatio * Math.Tan(horizontalFieldOfView * 0.5));
        float verticalHalfAngle = Math.Atan(verticalHalfRatio * Math.Tan(m_FieldOfView * 0.5));
        float limitingHalfAngle = horizontalHalfAngle;
        if (verticalHalfAngle < limitingHalfAngle)
            limitingHalfAngle = verticalHalfAngle;
        if (limitingHalfAngle < 0.04)
            limitingHalfAngle = 0.04;

        float adaptiveDistanceMultiplier = m_CameraDistanceMultiplier;
        if (m_ModelRadius > 2.0)
        {
            float largeVehicleBoost = (m_ModelRadius - 2.0) * m_LargeVehicleDistanceBoostPerRadiusMetre;
            largeVehicleBoost = Math.Clamp(largeVehicleBoost,0.0,m_LargeVehicleMaximumMultiplierBoost);
            adaptiveDistanceMultiplier = adaptiveDistanceMultiplier + largeVehicleBoost;
        }

        float desiredCameraDistance = m_ModelRadius / Math.Tan(limitingHalfAngle);
        desiredCameraDistance = Math.Clamp(desiredCameraDistance * adaptiveDistanceMultiplier,2.0,m_CameraMaximumCalculatedDistance);
        float railHalfWidth = Math.Clamp(desiredCameraDistance * 0.85,m_CameraRailMinimumHalfWidth,m_CameraRailMaximumHalfWidth);
        float railHalfDepth = Math.Clamp(desiredCameraDistance * 1.10,m_CameraRailMinimumHalfDepth,m_CameraRailMaximumHalfDepth);
        float yawRadians = m_PreviewYawDegrees * Math.DEG2RAD;
        float directionX = Math.Sin(yawRadians);
        float directionZ = Math.Cos(yawRadians);
        float absoluteDirectionX = Math.AbsFloat(directionX);
        float absoluteDirectionZ = Math.AbsFloat(directionZ);
        float scaleToWidth = 100000.0;
        float scaleToDepth = 100000.0;
        if (absoluteDirectionX > 0.0001)
            scaleToWidth = railHalfWidth / absoluteDirectionX;
        if (absoluteDirectionZ > 0.0001)
            scaleToDepth = railHalfDepth / absoluteDirectionZ;
        float railScale = scaleToWidth;
        if (scaleToDepth < railScale)
            railScale = scaleToDepth;

        vector cameraPosition = m_ModelCenter;
        cameraPosition[0] = cameraPosition[0] + directionX * railScale;
        cameraPosition[2] = cameraPosition[2] + directionZ * railScale;
        float elevationRadians = m_PreviewElevationDegrees * Math.DEG2RAD;
        cameraPosition[1] = cameraPosition[1] + Math.Tan(elevationRadians) * railScale;
        float cameraDistance = (cameraPosition - m_ModelCenter).Length();
        if (cameraDistance > m_CameraMaximumCalculatedDistance)
        {
            cameraPosition = m_ModelCenter + (cameraPosition - m_ModelCenter) * (m_CameraMaximumCalculatedDistance / cameraDistance);
            cameraDistance = m_CameraMaximumCalculatedDistance;
        }

        vector baseDirection = m_ModelCenter - cameraPosition;
        float baseLength = baseDirection.Length();
        if (baseLength < 0.001)
            return false;
        baseDirection = baseDirection * (1.0 / baseLength);
        vector cameraRight = Vector(baseDirection[2],0.0,-baseDirection[0]);
        float rightLength = cameraRight.Length();
        if (rightLength < 0.001)
            cameraRight = "1 0 0";
        else
            cameraRight = cameraRight * (1.0 / rightLength);

        float horizontalAimShift = normalizedCenterX * Math.Tan(horizontalFieldOfView * 0.5) * cameraDistance * m_ViewportCenterCorrection;
        float verticalAimShift = normalizedCenterY * Math.Tan(m_FieldOfView * 0.5) * cameraDistance * m_ViewportCenterCorrection;
        vector lookAtPosition = m_ModelCenter - cameraRight * horizontalAimShift;
        lookAtPosition[1] = lookAtPosition[1] + verticalAimShift;
        BDE_VG_PreviewCameraState.UpdateView(cameraPosition,lookAtPosition,m_FieldOfView);
        return true;
    }

    protected void CalculateHierarchyBounds()
    {
        m_HasBounds = false;
        m_BoundsMinimum = vector.Zero;
        m_BoundsMaximum = vector.Zero;
        AccumulateHierarchyBounds(m_RootEntity);
        if (!m_HasBounds)
        {
            m_ModelCenter = m_RootEntity.GetPosition();
            m_ModelRadius = 2.0;
            return;
        }
        m_ModelCenter = (m_BoundsMinimum + m_BoundsMaximum) * 0.5;
        vector dimensions = m_BoundsMaximum - m_BoundsMinimum;
        m_ModelRadius = dimensions.Length() * 0.5;
        if (m_ModelRadius < 0.75)
            m_ModelRadius = 0.75;
    }

    protected void AccumulateHierarchyBounds(EntityAI entity)
    {
        if (!entity)
            return;
        AccumulateEntityBounds(entity);
        GameInventory inventory = entity.GetInventory();
        if (!inventory)
            return;
        for (int i = 0; i < inventory.AttachmentCount(); i++)
            AccumulateHierarchyBounds(inventory.GetAttachmentFromIndex(i));
    }

    protected void AccumulateEntityBounds(EntityAI entity)
    {
        vector minMax[2];
        if (!entity.GetCollisionBox(minMax))
            return;
        for (int xIndex = 0; xIndex < 2; xIndex++)
        {
            for (int yIndex = 0; yIndex < 2; yIndex++)
            {
                for (int zIndex = 0; zIndex < 2; zIndex++)
                {
                    vector localCorner = Vector(minMax[xIndex][0],minMax[yIndex][1],minMax[zIndex][2]);
                    AddBoundsPoint(entity.ModelToWorld(localCorner));
                }
            }
        }
    }

    protected void AddBoundsPoint(vector point)
    {
        if (!m_HasBounds)
        {
            m_BoundsMinimum = point;
            m_BoundsMaximum = point;
            m_HasBounds = true;
            return;
        }
        if (point[0] < m_BoundsMinimum[0]) m_BoundsMinimum[0] = point[0];
        if (point[1] < m_BoundsMinimum[1]) m_BoundsMinimum[1] = point[1];
        if (point[2] < m_BoundsMinimum[2]) m_BoundsMinimum[2] = point[2];
        if (point[0] > m_BoundsMaximum[0]) m_BoundsMaximum[0] = point[0];
        if (point[1] > m_BoundsMaximum[1]) m_BoundsMaximum[1] = point[1];
        if (point[2] > m_BoundsMaximum[2]) m_BoundsMaximum[2] = point[2];
    }

    void Rotate(float degrees)
    {
        m_PreviewYawDegrees = m_PreviewYawDegrees + degrees;
        if (m_PreviewYawDegrees > 360.0) m_PreviewYawDegrees = m_PreviewYawDegrees - 360.0;
        if (m_PreviewYawDegrees < -360.0) m_PreviewYawDegrees = m_PreviewYawDegrees + 360.0;
        if (m_SpectatorSelected) UpdateCameraGeometry();
    }

    protected void ClearVehicleLocal()
    {
        if (m_FallbackPreviewWidget)
        {
            m_FallbackPreviewWidget.SetItem(null);
            m_FallbackPreviewWidget.Show(false);
        }
        if (m_RootEntity)
            g_Game.ObjectDelete(m_RootEntity);
        m_RootEntity = null;
        m_EmptyHangarMode = false;
        m_HasBounds = false;
    }

    void Clear()
    {
        ReturnToPlayerCamera();
        ClearVehicleLocal();
        DestroyLocalScene();
        m_SceneAnchor = vector.Zero;
        m_HasSceneAnchor = false;
        m_Player = null;
    }
};
