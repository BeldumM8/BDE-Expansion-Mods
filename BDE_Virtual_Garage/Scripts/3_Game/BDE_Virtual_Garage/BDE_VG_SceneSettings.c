class BDE_VG_SceneOptions
{
    float MaxCameraDistance = 30.0;
}

class BDE_VG_SceneSettings
{
    int Version = 1;
    vector Location = "60 900 60";
    float GarageSessionVerticalSpacing = 200.0;
    ref BDE_VG_SceneOptions Normal = new BDE_VG_SceneOptions;
    ref BDE_VG_SceneOptions CurHelicopter = new BDE_VG_SceneOptions;
    ref BDE_VG_SceneOptions Boats = new BDE_VG_SceneOptions;

    void BDE_VG_SceneSettings()
    {
        CurHelicopter.MaxCameraDistance = 60.0;
        Boats.MaxCameraDistance = 40.0;
    }

    void Normalize()
    {
        if (!Normal)
            Normal = new BDE_VG_SceneOptions;
        if (!CurHelicopter)
            CurHelicopter = new BDE_VG_SceneOptions;
        if (!Boats)
            Boats = new BDE_VG_SceneOptions;
        Normal.MaxCameraDistance = Math.Max(2.0, Normal.MaxCameraDistance);
        CurHelicopter.MaxCameraDistance = Math.Max(2.0, CurHelicopter.MaxCameraDistance);
        Boats.MaxCameraDistance = Math.Max(2.0, Boats.MaxCameraDistance);
        GarageSessionVerticalSpacing = Math.Max(100.0, GarageSessionVerticalSpacing);
    }

    float GetMaximumDistance(int kind)
    {
        if (kind == 1)
            return CurHelicopter.MaxCameraDistance;
        if (kind == 2)
            return Boats.MaxCameraDistance;
        return Normal.MaxCameraDistance;
    }
}
