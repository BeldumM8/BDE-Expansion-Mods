class BDE_MapUI_RestartBackups
{
    protected static bool s_Ran;
    static const string DIRECTORY = "$profile:/BDE/MapUI/_backups";

    static void Run()
    {
        if (s_Ran || !g_Game.IsServer())
        {
            return;
        }
        s_Ran = true;
        MakeDirectory("$profile:/BDE");
        MakeDirectory(BDE_MAPUI_CONFIG_DIR);
        MakeDirectory(DIRECTORY);
        Backup(BDE_MAPUI_CONFIG_PATH, "BDE_MapUI");
        Backup(BDE_MAPUI_CONFIG_DIR + "/DonationProgress.json", "DonationProgress");
        Backup(BDE_MAPUI_CONFIG_DIR + "/Reports.json", "Reports");
        MakeDirectory(BDE_MapUI_POINTS_BACKUPS_PATH);
        Backup(BDE_MapUI_POINTS_CONFIG_PATH, "PointsStoreConfig", BDE_MapUI_POINTS_BACKUPS_PATH);
    }

    protected static bool IsValid(string path, string name)
    {
        FileHandle file = OpenFile(path, FileMode.READ);
        if (!file)
        {
            return false;
        }
        string text;
        ReadFile(file, text, 100000000);
        CloseFile(file);
        string error;
        // Required keys prevent an empty object being accepted through constructor defaults.
        if (name == "BDE_MapUI")
        {
            BDE_MapUI_Config config;
            if (text.IndexOf("\"Tabs\"") < 0 || text.IndexOf("\"Links\"") < 0)
            {
                return false;
            }
            return JsonFileLoader<BDE_MapUI_Config>.LoadData(text, config, error) && config && config.Tabs && config.Tabs.Count() > 0 && config.Links && config.ReportTypes;
        }
        if (name == "DonationProgress")
        {
            BDE_MapUI_DonationProgressData donation;
            if (text.IndexOf("\"CurrentAmount\"") < 0 || text.IndexOf("\"MonthlyGoal\"") < 0)
            {
                return false;
            }
            return JsonFileLoader<BDE_MapUI_DonationProgressData>.LoadData(text, donation, error) && donation && donation.ConfigVersion > 0 && donation.MonthlyGoal >= 0 && donation.CurrentAmount >= 0;
        }
        if (name == "Reports")
        {
            BDE_MapUI_ReportStore reports;
            if (text.IndexOf("\"Reports\"") < 0 || text.IndexOf("\"NextID\"") < 0)
            {
                return false;
            }
            return JsonFileLoader<BDE_MapUI_ReportStore>.LoadData(text, reports, error) && reports && reports.Reports && reports.NextID > 0;
        }
        BDE_MapUI_PointsStoreConfig points;
        if (text.IndexOf("\"Tasks\"") < 0 || text.IndexOf("\"StoreItems\"") < 0)
        {
            return false;
        }
        return JsonFileLoader<BDE_MapUI_PointsStoreConfig>.LoadData(text, points, error) && points && points.Tasks && points.StoreItems && points.VIPMultiplier > 0;
    }

    protected static void Backup(string source, string name, string backupDirectory = DIRECTORY)
    {
        string pending = backupDirectory + "/" + name + ".pending.json";
        if (FileExist(pending))
        {
            DeleteFile(pending);
        }
        if (!FileExist(source))
        {
            return;
        }
        if (!IsValid(source, name))
        {
            Print("[BDE_MapUI backups] Invalid file skipped; previous backups preserved: " + source);
            return;
        }
        if (!CopyFile(source, pending) || !IsValid(pending, name))
        {
            DeleteFile(pending);
            Print("[BDE_MapUI backups] Could not stage backup: " + source);
            return;
        }
        for (int slot = 3; slot >= 2; slot--)
        {
            string previous = backupDirectory + "/" + name + "." + (slot - 1).ToString() + ".json";
            string destination = backupDirectory + "/" + name + "." + slot.ToString() + ".json";
            if (FileExist(previous) && !CopyFile(previous, destination))
            {
                DeleteFile(pending);
                Print("[BDE_MapUI backups] Rotation failed: " + destination);
                return;
            }
        }
        string newest = backupDirectory + "/" + name + ".1.json";
        if (!CopyFile(pending, newest))
        {
            Print("[BDE_MapUI backups] Could not save newest backup: " + newest);
        }
        else
        {
            Print("[BDE_MapUI backups] Saved " + newest);
        }
        DeleteFile(pending);
    }
};
