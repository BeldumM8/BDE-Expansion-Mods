class BDE_EMU_VehiclePreviewRenderer
{
    protected Widget m_Viewport;
    protected PlayerBase m_Player;
    protected EntityAI m_RootEntity;
    protected ref array<Object> m_SceneObjects;
    protected vector m_SceneAnchor;
    protected bool m_SceneCreated;
    protected bool m_SpectatorSelected;
    protected bool m_HasBounds;
    protected vector m_BoundsMinimum;
    protected vector m_BoundsMaximum;
    protected vector m_ModelCenter;
    protected float m_ModelRadius = 2.0;

    // Scene and camera tuning.
    protected int m_SceneKind = -1;
    protected float m_FloorOffsetFromHEMTT = -0.273773;
    protected float m_FloorClearance = 0.03;
    protected float m_PreviewYawDegrees = 42.0;
    protected float m_PreviewElevationDegrees = 6.0;
    protected float m_FieldOfView = 0.72;
    protected float m_CameraDistanceMultiplier = 0.46;
    protected float m_LargeVehicleDistanceBoostPerRadiusMetre = 0.045;
    protected float m_LargeVehicleMaximumMultiplierBoost = 0.35;
    protected float m_CameraMaximumCalculatedDistance = 30.0;
    protected float m_CameraRailMinimumHalfWidth = 3.25;
    protected float m_CameraRailMaximumHalfWidth = 8.25;
    protected float m_CameraRailMinimumHalfDepth = 4.50;
    protected float m_CameraRailMaximumHalfDepth = 12.00;
    protected float m_ViewportCenterCorrection = 1.42;

    void BDE_EMU_VehiclePreviewRenderer(Widget viewport)
    {
        m_Viewport = viewport;
        m_SceneObjects = new array<Object>;
    }

    void ~BDE_EMU_VehiclePreviewRenderer()
    {
        Clear();
        m_Viewport = null;
    }

    bool OwnsWidget(Widget widget)
    {
        Widget current = widget;
        while (current)
        {
            if (current == m_Viewport)
                return true;
            current = current.GetParent();
        }
        return false;
    }

    bool IsActive()
    {
        return m_SpectatorSelected;
    }

    bool IsVehiclePreviewActive()
    {
        return m_RootEntity != null && m_SpectatorSelected;
    }

    EntityAI GetPreviewEntity()
    {
        return m_RootEntity;
    }

    void Build(ExpansionMarketItem marketItem, string previewClassName, int skinIndex)
    {
        ClearVehicle(false);
        if (!marketItem)
            return;
        if (previewClassName == string.Empty)
            previewClassName = marketItem.ClassName;
        if (previewClassName == string.Empty)
            return;

        if (!EnsureScene(previewClassName))
            return;

        vector spawnPosition = m_SceneAnchor;
        spawnPosition[1] = spawnPosition[1] + 5.0;
        m_RootEntity = EntityAI.Cast(g_Game.CreateObjectEx(previewClassName, spawnPosition, ECE_LOCAL | ECE_NOLIFETIME | ECE_CREATEPHYSICS));
        if (!m_RootEntity)
        {
            Print("[BDE Market UI] Failed to create vehicle preview " + previewClassName);
            return;
        }

        m_RootEntity.SetAllowDamage(false);
        m_RootEntity.SetOrientation(BDE_EMU_SceneBuilder.GetOrientation(m_SceneKind));
        if (m_RootEntity.HasSelection("antiwater"))
            m_RootEntity.HideSelection("antiwater");

        SpawnAttachments(marketItem, m_RootEntity, skinIndex, 0);
        Construction.ExpansionBuildFullIfSupported(m_RootEntity);

        ExpansionVehicle expansionVehicle;
        if (skinIndex > -1 && ExpansionVehicle.Get(expansionVehicle, m_RootEntity) && expansionVehicle.HasSkin(skinIndex))
            expansionVehicle.SetSkin(skinIndex);

        PlaceVehicleOnHangarFloor();
        FreezePreviewHierarchy(m_RootEntity);
        CalculateHierarchyBounds();

        if (!UpdateCameraGeometry() || !ActivateSpectatorCamera())
            Print("[BDE Market UI] Failed to activate vehicle spectator preview for " + previewClassName);
    }

    protected void SpawnAttachments(ExpansionMarketItem marketItem, EntityAI parent, int skinIndex, int level)
    {
        if (!marketItem || !parent || level > 3)
            return;

        foreach (string attachmentName : marketItem.SpawnAttachments)
        {
            EntityAI attachmentEntity = ExpansionItemSpawnHelper.SpawnAttachment(attachmentName, parent, skinIndex);
            if (!attachmentEntity)
                continue;

            attachmentEntity.SetAllowDamage(false);
            ExpansionMarketItem attachmentItem = ExpansionMarketCategory.GetGlobalItem(attachmentName, false);
            if (attachmentItem)
                SpawnAttachments(attachmentItem, attachmentEntity, skinIndex, level + 1);
        }
    }

    protected void PlaceVehicleOnHangarFloor()
    {
        if (m_SceneKind == 2)
        {
            m_RootEntity.SetPosition(m_SceneAnchor);
            return;
        }
        CalculateHierarchyBounds();
        if (!m_HasBounds)
            return;

        float floorHeight = m_SceneAnchor[1] + m_FloorOffsetFromHEMTT;
        vector rayStart = m_SceneAnchor + Vector(0, 2, 0);
        vector rayEnd = m_SceneAnchor - Vector(0, 15, 0);
        vector hitPosition;
        vector hitNormal;
        int hitComponent;
        set<Object> hitObjects = new set<Object>;
        if (DayZPhysics.RaycastRV(rayStart, rayEnd, hitPosition, hitNormal, hitComponent, hitObjects, null, m_RootEntity))
        {
            foreach (Object hit : hitObjects)
            {
                if (m_SceneObjects.Find(hit) >= 0 && !Transport.Cast(hit) && hitNormal[1] > 0.5)
                {
                    floorHeight = hitPosition[1];
                    break;
                }
            }
        }
        float contactY;
        if (!BDE_EMU_SceneBuilder.GetGroundContactY(m_RootEntity, contactY))
            return;
        float shift = floorHeight + m_FloorClearance - contactY;
        vector position = m_RootEntity.GetPosition();
        position[1] = position[1] + shift;
        m_RootEntity.SetPosition(position);
        CalculateHierarchyBounds();
    }

    protected void FreezePreviewHierarchy(EntityAI entity)
    {
        if (!entity)
            return;

        entity.SetAllowDamage(false);
        Transport transport;
        if (Class.CastTo(transport, entity))
            transport.DisableSimulation(true);
        if (dBodyIsSet(entity))
        {
            dBodyActive(entity, ActiveState.INACTIVE);
            dBodyDynamic(entity, false);
        }

        GameInventory inventory = entity.GetInventory();
        if (!inventory)
            return;
        for (int i = 0; i < inventory.AttachmentCount(); i++)
            FreezePreviewHierarchy(inventory.GetAttachmentFromIndex(i));
    }

    protected bool EnsureScene(string className)
    {
        if (!BDE_EMU_SceneModule.Ready || !BDE_EMU_SceneModule.Settings)
            return false;
        int kind = BDE_EMU_SceneBuilder.GetKind(className);
        if (m_SceneCreated && m_SceneKind == kind)
            return true;
        DestroyScene();
        m_SceneKind = kind;
        m_Player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!m_Player)
            return false;
        m_SceneAnchor = BDE_EMU_SceneModule.Settings.Location;
        m_CameraMaximumCalculatedDistance = BDE_EMU_SceneModule.Settings.GetMaximumDistance(kind);
        m_CameraRailMaximumHalfWidth = m_CameraMaximumCalculatedDistance;
        m_CameraRailMaximumHalfDepth = m_CameraMaximumCalculatedDistance;
        m_FloorOffsetFromHEMTT = BDE_EMU_SceneBuilder.GetFloorOffset(kind);
        SpawnPreviewScene();
        m_SceneCreated = m_SceneObjects.Count() > 0;
        return m_SceneCreated;
    }

    protected void SpawnPreviewScene()
    {
        BDE_EMU_SceneBuilder.Build(m_SceneKind, m_SceneAnchor, true, m_SceneObjects);
    }



    protected bool ActivateSpectatorCamera()
    {
        if (!m_Player)
            m_Player = PlayerBase.Cast(g_Game.GetPlayer());
        if (!m_Player || !m_Player.GetIdentity())
            return false;

        vector cameraPosition = BDE_EMU_PreviewCameraState.GetCameraPosition();
        BDE_EMU_PreviewCameraState.Activate(cameraPosition, BDE_EMU_PreviewCameraState.GetLookAtPosition(), m_FieldOfView);
        if (m_SpectatorSelected)
            return true;

        g_Game.SelectPlayer(m_Player.GetIdentity(), m_Player);
        g_Game.SelectSpectator(m_Player.GetIdentity(), "BDE_EMU_SpectatorCamera", cameraPosition);
        m_SpectatorSelected = true;
        return true;
    }

    protected void ReturnToPlayerCamera()
    {
        BDE_EMU_PreviewCameraState.Deactivate();
        if (!m_SpectatorSelected)
            return;

        if (!m_Player)
            m_Player = PlayerBase.Cast(g_Game.GetPlayer());
        if (m_Player && m_Player.GetIdentity())
            g_Game.SelectPlayer(m_Player.GetIdentity(), m_Player);
        m_SpectatorSelected = false;
    }

    protected bool UpdateCameraGeometry()
    {
        if (!m_RootEntity || !m_Viewport)
            return false;

        float viewportX;
        float viewportY;
        float viewportWidth;
        float viewportHeight;
        m_Viewport.GetScreenPos(viewportX, viewportY);
        m_Viewport.GetScreenSize(viewportWidth, viewportHeight);

        int screenWidth;
        int screenHeight;
        GetScreenSize(screenWidth, screenHeight);
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
            largeVehicleBoost = Math.Clamp(largeVehicleBoost, 0.0, m_LargeVehicleMaximumMultiplierBoost);
            adaptiveDistanceMultiplier = adaptiveDistanceMultiplier + largeVehicleBoost;
        }

        float desiredCameraDistance = m_ModelRadius / Math.Tan(limitingHalfAngle);
        desiredCameraDistance = Math.Clamp(desiredCameraDistance * adaptiveDistanceMultiplier, 2.0, m_CameraMaximumCalculatedDistance);
        float railHalfWidth = Math.Clamp(desiredCameraDistance * 0.85, m_CameraRailMinimumHalfWidth, m_CameraRailMaximumHalfWidth);
        float railHalfDepth = Math.Clamp(desiredCameraDistance * 1.10, m_CameraRailMinimumHalfDepth, m_CameraRailMaximumHalfDepth);

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
        vector cameraRight = Vector(baseDirection[2], 0.0, -baseDirection[0]);
        float rightLength = cameraRight.Length();
        if (rightLength < 0.001)
            cameraRight = "1 0 0";
        else
            cameraRight = cameraRight * (1.0 / rightLength);

        float horizontalAimShift = normalizedCenterX * Math.Tan(horizontalFieldOfView * 0.5) * cameraDistance * m_ViewportCenterCorrection;
        float verticalAimShift = normalizedCenterY * Math.Tan(m_FieldOfView * 0.5) * cameraDistance * m_ViewportCenterCorrection;
        vector lookAtPosition = m_ModelCenter - cameraRight * horizontalAimShift;
        lookAtPosition[1] = lookAtPosition[1] + verticalAimShift;
        BDE_EMU_PreviewCameraState.UpdateView(cameraPosition, lookAtPosition, m_FieldOfView);
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
                    vector localCorner = Vector(minMax[xIndex][0], minMax[yIndex][1], minMax[zIndex][2]);
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
        if (m_PreviewYawDegrees > 360.0)
            m_PreviewYawDegrees = m_PreviewYawDegrees - 360.0;
        if (m_PreviewYawDegrees < -360.0)
            m_PreviewYawDegrees = m_PreviewYawDegrees + 360.0;
        if (m_SpectatorSelected)
            UpdateCameraGeometry();
    }

    void ClearVehicle(bool returnPlayerCamera = true)
    {
        if (returnPlayerCamera)
            ReturnToPlayerCamera();
        if (m_RootEntity)
            g_Game.ObjectDelete(m_RootEntity);
        m_RootEntity = null;
        m_HasBounds = false;
    }

    protected void DestroyScene()
    {
        for (int i = m_SceneObjects.Count() - 1; i >= 0; i--)
        {
            if (m_SceneObjects[i])
                g_Game.ObjectDelete(m_SceneObjects[i]);
        }
        m_SceneObjects.Clear();
        m_SceneCreated = false;
        m_SceneAnchor = vector.Zero;
    }

    void Clear()
    {
        ClearVehicle(true);
        DestroyScene();
    }
};
