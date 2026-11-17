// BDE_MapUI_DonationProgressData.c
// Shared JSON data and persistent client cache for the Server Panel donation goal.

class BDE_MapUI_DonationProgressDefaults
{
    static const string MESSAGE = "Donations aren't mandatory, but are muchly appreciated to help cover operating costs.";
}

class BDE_MapUI_DonationProgressData
{
    int ConfigVersion;
    int Enabled;
    string Message;
    int CurrentAmount;
    int MonthlyGoal;
    string CurrencySymbol;

    void BDE_MapUI_DonationProgressData()
    {
        ConfigVersion = 1;
        Enabled = 1;
        Message = BDE_MapUI_DonationProgressDefaults.MESSAGE;
        CurrentAmount = 0;
        MonthlyGoal = 0;
        CurrencySymbol = "$";
    }
}

// The CF RPC receiver is registered on persistent ServerPanelBase rather than
// a temporary ServerPanelMenu. This cache lets newly opened panels immediately
// display the latest received values without registering RPCs on UI objects.
class BDE_MapUI_DonationProgressClientCache
{
    protected static ref BDE_MapUI_DonationProgressData s_Data;

    static void Set(int configVersion, int enabled, string message, int currentAmount, int monthlyGoal, string currencySymbol)
    {
        s_Data = new BDE_MapUI_DonationProgressData();
        s_Data.ConfigVersion = configVersion;
        s_Data.Enabled = enabled;
        s_Data.Message = message;
        s_Data.CurrentAmount = currentAmount;
        s_Data.MonthlyGoal = monthlyGoal;
        s_Data.CurrencySymbol = currencySymbol;
    }

    static BDE_MapUI_DonationProgressData Get()
    {
        return s_Data;
    }

    static void Clear()
    {
        s_Data = null;
    }
}
