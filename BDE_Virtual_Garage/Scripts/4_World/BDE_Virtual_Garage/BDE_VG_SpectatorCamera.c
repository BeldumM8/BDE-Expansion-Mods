class BDE_VG_PreviewCameraState
{
    protected static bool s_Active;
    protected static vector s_CameraPosition;
    protected static vector s_LookAtPosition;
    protected static float s_FieldOfView = 0.72;

    static void Activate(vector cameraPosition, vector lookAtPosition, float fieldOfView)
    {
        s_CameraPosition = cameraPosition;
        s_LookAtPosition = lookAtPosition;
        s_FieldOfView = fieldOfView;
        s_Active = true;
    }

    static void UpdateView(vector cameraPosition, vector lookAtPosition, float fieldOfView)
    {
        s_CameraPosition = cameraPosition;
        s_LookAtPosition = lookAtPosition;
        s_FieldOfView = fieldOfView;
    }

    static void Deactivate()
    {
        s_Active = false;
        s_CameraPosition = vector.Zero;
        s_LookAtPosition = vector.Zero;
        s_FieldOfView = 0.72;
    }

    static bool IsActive()
    {
        return s_Active;
    }

    static vector GetCameraPosition()
    {
        return s_CameraPosition;
    }

    static vector GetLookAtPosition()
    {
        return s_LookAtPosition;
    }

    static float GetFieldOfView()
    {
        return s_FieldOfView;
    }
};

class BDE_VG_SpectatorCamera extends Camera
{
    protected vector m_LastCameraPosition;
    protected vector m_LastLookAtPosition;
    protected float m_LastFieldOfView;
    protected bool m_Applied;

    void BDE_VG_SpectatorCamera()
    {
        SetEventMask(EntityEvent.FRAME);
        SetNearPlane(0.03);
        m_LastCameraPosition = vector.Zero;
        m_LastLookAtPosition = vector.Zero;
        m_LastFieldOfView = 0.72;
    }

    override void EOnFrame(IEntity other, float timeSlice)
    {
        if (!BDE_VG_PreviewCameraState.IsActive())
            return;

        vector cameraPosition = BDE_VG_PreviewCameraState.GetCameraPosition();
        vector lookAtPosition = BDE_VG_PreviewCameraState.GetLookAtPosition();
        float fieldOfView = BDE_VG_PreviewCameraState.GetFieldOfView();

        bool transformChanged = false;
        if (!m_Applied || !VectorsClose(m_LastCameraPosition,cameraPosition,0.0001))
        {
            SetPosition(cameraPosition);
            m_LastCameraPosition = cameraPosition;
            transformChanged = true;
        }

        if (!m_Applied || !VectorsClose(m_LastLookAtPosition,lookAtPosition,0.0001))
        {
            m_LastLookAtPosition = lookAtPosition;
            transformChanged = true;
        }

        if (transformChanged)
            LookAt(lookAtPosition);

        if (!m_Applied || Math.AbsFloat(m_LastFieldOfView - fieldOfView) > 0.0001)
        {
            SetFOV(fieldOfView);
            m_LastFieldOfView = fieldOfView;
        }

        m_Applied = true;


    }

    protected bool VectorsClose(vector first, vector second, float epsilon)
    {
        if (Math.AbsFloat(first[0] - second[0]) > epsilon)
            return false;
        if (Math.AbsFloat(first[1] - second[1]) > epsilon)
            return false;
        if (Math.AbsFloat(first[2] - second[2]) > epsilon)
            return false;
        return true;
    }
};
