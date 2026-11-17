class BDE_VG_ExclusiveList
{
    int Version = 1;
    bool MatchInheritedClasses = true;
    ref array<string> ClassNames = new array<string>;
};

class BDE_VG_ExclusivePromptData
{
    string Token;
    int AccessLow;
    int AccessHigh;
    int VehicleLow;
    int VehicleHigh;
    bool Repair;
    ref array<string> Rows = new array<string>;
};
