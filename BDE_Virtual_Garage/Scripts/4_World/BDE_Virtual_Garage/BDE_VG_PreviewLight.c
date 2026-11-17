// Local-only, shadowless garage presentation lighting.
// ScriptedLightBase.CreateLight checks !g_Game.IsServer() before creating
// the underlying light; it is never broadcast to other players.
class BDE_VG_PreviewLight extends PointLightBase
{
    void BDE_VG_PreviewLight()
    {
        SetVisibleDuringDaylight(true);
        SetRadiusTo(55.0);
        SetBrightnessTo(3.1);
        SetCastShadow(false);
        SetAmbientColor(0.90,0.95,1.00);
        SetDiffuseColor(0.95,0.96,1.00);
    }
}
