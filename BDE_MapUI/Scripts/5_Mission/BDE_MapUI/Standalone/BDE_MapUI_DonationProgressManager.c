// BDE_MapUI_DonationProgressManager.c
// Server-authoritative storage and live synchronisation for the monthly donation goal.

class BDE_MapUI_DonationProgressManager
{
    protected static ref BDE_MapUI_DonationProgressData s_Data;

    static const string FILE_NAME = "DonationProgress.json";
    static const string FILE_PATH = "$profile:/BDE/MapUI/DonationProgress.json";

    protected static void EnsureDirectories()
    {
        if (!FileExist("$profile:/BDE"))
        {
            MakeDirectory("$profile:/BDE");
        }

        if (!FileExist("$profile:/BDE/MapUI"))
        {
            MakeDirectory("$profile:/BDE/MapUI");
        }
    }

    protected static void Validate(BDE_MapUI_DonationProgressData data)
    {
        if (!data)
        {
            return;
        }

        if (data.ConfigVersion < 1)
        {
            data.ConfigVersion = 1;
        }

        if (data.Enabled != 0)
        {
            data.Enabled = 1;
        }

        // Migrate the previous built-in wording while preserving genuinely custom text.
        if (data.Message == "" || data.Message == "Donations go directly towards covering the monthly server hosting costs.")
        {
            data.Message = BDE_MapUI_DonationProgressDefaults.MESSAGE;
        }

        if (data.CurrentAmount < 0)
        {
            data.CurrentAmount = 0;
        }

        if (data.MonthlyGoal < 0)
        {
            data.MonthlyGoal = 0;
        }

        if (data.CurrencySymbol == "")
        {
            data.CurrencySymbol = "$";
        }
    }

    static BDE_MapUI_DonationProgressData GetData(bool forceReload = false)
    {
        if (!GetGame() || !GetGame().IsServer())
        {
            return null;
        }

        if (s_Data && !forceReload)
        {
            return s_Data;
        }

        EnsureDirectories();

        s_Data = new BDE_MapUI_DonationProgressData();

        if (FileExist(FILE_PATH))
        {
            JsonFileLoader<BDE_MapUI_DonationProgressData>.JsonLoadFile(FILE_PATH, s_Data);

            if (!s_Data)
            {
                s_Data = new BDE_MapUI_DonationProgressData();
            }
        }

        Validate(s_Data);
        Save();

        return s_Data;
    }

    static void Save()
    {
        if (!GetGame() || !GetGame().IsServer())
        {
            return;
        }

        if (!s_Data)
        {
            s_Data = new BDE_MapUI_DonationProgressData();
        }

        Validate(s_Data);
        EnsureDirectories();
        JsonFileLoader<BDE_MapUI_DonationProgressData>.JsonSaveFile(FILE_PATH, s_Data);
    }

    static bool SetAmounts(int currentAmount, int monthlyGoal)
    {
        BDE_MapUI_DonationProgressData data = GetData();

        if (!data)
        {
            return false;
        }

        if (currentAmount < 0 || monthlyGoal < 0)
        {
            return false;
        }

        data.CurrentAmount = currentAmount;
        data.MonthlyGoal = monthlyGoal;
        Save();
        Broadcast();
        return true;
    }

    static bool AddDonation(int amount)
    {
        if (amount <= 0)
        {
            return false;
        }

        BDE_MapUI_DonationProgressData data = GetData();

        if (!data)
        {
            return false;
        }

        data.CurrentAmount += amount;
        Save();
        Broadcast();
        return true;
    }

    static bool ResetCurrentAmount()
    {
        BDE_MapUI_DonationProgressData data = GetData();

        if (!data)
        {
            return false;
        }

        data.CurrentAmount = 0;
        Save();
        Broadcast();
        return true;
    }

    static void SendTo(PlayerIdentity targetIdentity)
    {
        if (!targetIdentity)
        {
            return;
        }

        BDE_MapUI_DonationProgressData data = GetData();

        if (!data)
        {
            return;
        }

        // Primitive-only payload avoids passing a live server-owned object reference
        // through CF RPC and exactly matches ServerPanelBase.ReceiveDonationProgress.
        Param6<int, int, string, int, int, string> rpcData = new Param6<int, int, string, int, int, string>(data.ConfigVersion, data.Enabled, data.Message, data.CurrentAmount, data.MonthlyGoal, data.CurrencySymbol);
        GetRPCManager().SendRPC(BDE_MAPUI_RPC, "RPC_ReceiveDonation", rpcData, true, targetIdentity);
    }

    static void Broadcast()
    {
        if (!GetGame() || !GetGame().IsServer())
        {
            return;
        }

        array<Man> players = new array<Man>;
        GetGame().GetPlayers(players);

        foreach (Man man : players)
        {
            PlayerBase player = PlayerBase.Cast(man);

            if (!player || !player.GetIdentity())
            {
                continue;
            }

            SendTo(player.GetIdentity());
        }
    }
}
