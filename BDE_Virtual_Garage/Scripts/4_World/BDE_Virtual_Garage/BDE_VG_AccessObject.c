// Register on the player, matching Expansion's garage interaction path.
// Access is checked against the server-supplied whitelist, not per-object net variables.
modded class PlayerBase
{
    override void SetActions(out TInputActionMap InputActionMap)
    {
        super.SetActions(InputActionMap);
        AddAction(BDE_VG_ActionOpenGarage, InputActionMap);
    }
}
