static const string BDE_MAPUI_RPC = "BDE_MapUI_RPC";
static const string BDE_MAPUI_CONFIG_DIR = "$profile:/BDE/MapUI";
static const string BDE_MAPUI_CONFIG_PATH = "$profile:/BDE/MapUI/BDE_MapUI.json";
static const int BDE_MAPUI_CONFIG_VERSION = 5;
static const string BDE_MAPUI_DEFAULT_CONFIG_PATH = "BDE_MapUI/Defaults/BDE_MapUI.json";

static const string BDE_MAPUI_TAB_STATIC = "STATIC";
static const string BDE_MAPUI_TAB_BOUNTIES = "BOUNTIES";
static const string BDE_MAPUI_TAB_LEADERBOARD = "LEADERBOARD";
static const string BDE_MAPUI_TAB_POINTS = "POINTS_STORE";
static const string BDE_MAPUI_TAB_CRAFTING = "CRAFTING";
static const string BDE_MAPUI_TAB_PLAYER = "PLAYER";
static const string BDE_MAPUI_TAB_VIP_ADMIN = "VIP_ADMIN";

class BDE_MapUI_Theme
{
    static const int TEXT = ARGB(255, 220, 220, 220);
    static const int MUTED = ARGB(255, 170, 180, 190);
    static const int ACCENT = ARGB(255, 74, 145, 194);
    static const int ACTIVE = ARGB(255, 58, 111, 148);
    static const int BUTTON = ARGB(255, 20, 38, 54);
    static const int GREEN = ARGB(255, 80, 210, 120);
    static const int RED = ARGB(255, 220, 80, 80);
    static const int YELLOW = ARGB(255, 230, 190, 70);
};

class BDE_MapUI_TabConfig
{
    bool Enabled;
    string Type;
    string Label;
    string Heading;
    ref array<string> Content;

    void BDE_MapUI_TabConfig()
    {
        Enabled = true;
        Type = BDE_MAPUI_TAB_STATIC;
        Label = "Tab";
        Heading = "Information";
        Content = new array<string>();
    }
};

class BDE_MapUI_LinkConfig
{
    bool Enabled;
    string Label;
    string URL;

    void BDE_MapUI_LinkConfig()
    {
        Enabled = false;
        Label = "Link";
        URL = "";
    }
};

class BDE_MapUI_Config
{
    int ConfigVersion;

    bool ShowMapTools;

    bool ShowCurrency;
    string BankLabel;
    string InventoryLabel;
    int SidebarRefreshSeconds;
    int PassiveIncomeAmount;
    int PassiveRPAmount;
    int MinBountyAmount;

    bool ShowDonationGoal;
    bool ShowDonationMessage;

    bool OpenInfoOnMapOpen;
    string VIPURL;

    ref array<string> ReportAdminSteamIDs;
    int ReportCooldownSeconds;
    int MaxOpenReportsPerPlayer;
    ref array<ref BDE_MapUI_TabConfig> Tabs;
    ref array<ref BDE_MapUI_LinkConfig> Links;
    ref array<string> ReportTypes;

    void BDE_MapUI_Config()
    {
        ConfigVersion = BDE_MAPUI_CONFIG_VERSION;

        ShowMapTools = true;

        ShowCurrency = true;
        BankLabel = "Bank";
        InventoryLabel = "Cash";
        SidebarRefreshSeconds = 30;
        PassiveIncomeAmount = 4000;
        PassiveRPAmount = 40;
        MinBountyAmount = 50000;

        ShowDonationGoal = true;
        ShowDonationMessage = true;

        OpenInfoOnMapOpen = false;
        VIPURL = "";

        Tabs = new array<ref BDE_MapUI_TabConfig>();
        Links = new array<ref BDE_MapUI_LinkConfig>();
        ReportAdminSteamIDs = new array<string>();
        ReportTypes = new array<string>();
        ReportCooldownSeconds = 60;
        MaxOpenReportsPerPlayer = 5;
    }
};

class BDE_MapUI_ConfigManager
{
    protected static ref BDE_MapUI_Config s_Config;

    static BDE_MapUI_Config Get()
    {
        if (!s_Config)
            s_Config = CreateDefaults();
        return s_Config;
    }

    static void Set(BDE_MapUI_Config cfg)
    {
        if (!cfg)
            return;

        Validate(cfg);
        s_Config = cfg;
    }

    static BDE_MapUI_Config LoadServer()
    {
        if (!FileExist("$profile:/BDE"))
            MakeDirectory("$profile:/BDE");
        if (!FileExist(BDE_MAPUI_CONFIG_DIR))
            MakeDirectory(BDE_MAPUI_CONFIG_DIR);

        string path = BDE_MAPUI_CONFIG_PATH;
        bool configExists = FileExist(path);
        BDE_MapUI_Config cfg = new BDE_MapUI_Config();
        if (configExists)
        {
            string loadError;
            if (!JsonFileLoader<BDE_MapUI_Config>.LoadFile(path, cfg, loadError))
            {
                Print("[BDE_MapUI config] " + loadError);
                s_Config = CreateDefaults();
                Validate(s_Config);
                return s_Config;
            }
        }
        else
        {
            cfg = CreateDefaults();
        }
        Validate(cfg);
        // Preserve the administrator's ordering, indentation and spacing.
        // Only write defaults when there is no existing config file.
        if (!configExists)
        {
            string saveError;
            if (!SaveFreshConfig(path, cfg, saveError))
            {
                Print("[BDE_MapUI config] " + saveError);
            }
        }
        s_Config = cfg;
        return s_Config;
    }

    protected static bool SaveFreshConfig(string path, BDE_MapUI_Config cfg, out string error)
    {
        // Copy the bundled template text to preserve its exact formatting.
        FileHandle templateFile = OpenFile(BDE_MAPUI_DEFAULT_CONFIG_PATH, FileMode.READ);
        if (templateFile)
        {
            string templateText;
            ReadFile(templateFile, templateText, 1000000);
            CloseFile(templateFile);
            BDE_MapUI_Config templateConfig;
            if (JsonFileLoader<BDE_MapUI_Config>.LoadData(templateText, templateConfig, error))
            {
                FileHandle templateOutput = OpenFile(path, FileMode.WRITE);
                if (!templateOutput)
                {
                    error = "Cannot create config: " + path;
                    return false;
                }
                FPrint(templateOutput, templateText);
                CloseFile(templateOutput);
                return true;
            }
        }
        // Fallback if the template was omitted while packaging.
        string serialized;
        if (!JsonFileLoader<BDE_MapUI_Config>.MakeData(cfg, serialized, error, false))
        {
            return false;
        }
        string formatted = FormatConfigJSON(serialized);
        BDE_MapUI_Config checkedConfig;
        if (!JsonFileLoader<BDE_MapUI_Config>.LoadData(formatted, checkedConfig, error))
        {
            return false;
        }
        FileHandle file = OpenFile(path, FileMode.WRITE);
        if (!file)
        {
            error = "Cannot create config: " + path;
            return false;
        }
        FPrint(file, formatted);
        CloseFile(file);
        return true;
    }

    protected static string JSONIndent(int depth)
    {
        string indent;
        for (int i = 0; i < depth; i++)
        {
            indent += "    ";
        }
        return indent;
    }

    protected static string FormatConfigJSON(string source)
    {
        string output;
        string previous;
        int depth;
        bool inString;
        bool escaped;
        for (int i = 0; i < source.Length(); i++)
        {
            string character = source.Substring(i, 1);
            if (inString)
            {
                output += character;
                if (escaped)
                {
                    escaped = false;
                }
                else if (character == "\\")
                {
                    escaped = true;
                }
                else if (character == "\"")
                {
                    inString = false;
                    previous = character;
                }
                continue;
            }
            if (character == " " || character == "\t" || character == "\r" || character == "\n")
            {
                continue;
            }
            if (character == "\"")
            {
                inString = true;
                output += character;
            }
            else if (character == "{" || character == "[")
            {
                output += character;
                depth++;
                string next;
                for (int j = i + 1; j < source.Length(); j++)
                {
                    next = source.Substring(j, 1);
                    if (next != " " && next != "\t" && next != "\r" && next != "\n")
                    {
                        break;
                    }
                }
                if (next != "]" && next != "}")
                {
                    output += "\r\n" + JSONIndent(depth);
                }
            }
            else if (character == "}" || character == "]")
            {
                depth--;
                if (previous != "[" && previous != "{")
                {
                    output += "\r\n" + JSONIndent(depth);
                }
                output += character;
            }
            else if (character == ",")
            {
                output += ",\r\n" + JSONIndent(depth);
            }
            else if (character == ":")
            {
                output += ": ";
            }
            else if (character == "t" && source.Length() - i >= 4 && source.Substring(i, 4) == "true")
            {
                output += "1";
                i += 3;
            }
            else if (character == "f" && source.Length() - i >= 5 && source.Substring(i, 5) == "false")
            {
                output += "0";
                i += 4;
            }
            else
            {
                output += character;
            }
            previous = character;
        }
        return output + "\r\n";
    }

    protected static BDE_MapUI_TabConfig MakeStaticTab(string label, string heading, string line)
    {
        BDE_MapUI_TabConfig tab = new BDE_MapUI_TabConfig();
        tab.Type = BDE_MAPUI_TAB_STATIC;
        tab.Label = label;
        tab.Heading = heading;
        tab.Content.Insert(line);
        return tab;
    }

    protected static BDE_MapUI_TabConfig MakeFeatureTab(string type, string label, string heading)
    {
        BDE_MapUI_TabConfig tab = new BDE_MapUI_TabConfig();
        tab.Type = type;
        tab.Label = label;
        tab.Heading = heading;
        return tab;
    }

    protected static BDE_MapUI_LinkConfig MakeLink(string label, string url)
    {
        BDE_MapUI_LinkConfig link = new BDE_MapUI_LinkConfig();
        link.Enabled = (url != "");
        link.Label = label;
        link.URL = url;
        return link;
    }

    static BDE_MapUI_Config CreateDefaults()
    {
        BDE_MapUI_Config cfg = new BDE_MapUI_Config();
        string templateError;
        if (JsonFileLoader<BDE_MapUI_Config>.LoadFile(BDE_MAPUI_DEFAULT_CONFIG_PATH, cfg, templateError) && cfg && cfg.Tabs && cfg.Links)
        {
            return cfg;
        }
        cfg = new BDE_MapUI_Config();

        cfg.Tabs.Insert(MakeStaticTab("Changelog", "Changelog", "<h1>Changelog</h1><p>Add your server updates here.</p>"));
        cfg.Tabs.Insert(MakeFeatureTab(BDE_MAPUI_TAB_BOUNTIES, "Bounties", "Bounty Board"));
        cfg.Tabs.Insert(MakeFeatureTab(BDE_MAPUI_TAB_LEADERBOARD, "Leaderboard", "Player Leaderboard"));
        cfg.Tabs.Insert(MakeFeatureTab(BDE_MAPUI_TAB_POINTS, "Point Store", "Task Point Store"));
        cfg.Tabs.Insert(MakeFeatureTab(BDE_MAPUI_TAB_CRAFTING, "Crafting", "Crafting Guide"));
        cfg.Tabs.Insert(MakeFeatureTab(BDE_MAPUI_TAB_PLAYER, "Player", "Player Information"));
        cfg.Tabs.Insert(MakeFeatureTab(BDE_MAPUI_TAB_VIP_ADMIN, "VIP", "VIP Manager"));
        cfg.Tabs.Insert(MakeStaticTab("Rules & Info", "Server Rules & Information", "<h1>Welcome</h1><p>Add your server information and rules here.</p>"));
        cfg.Tabs.Insert(MakeStaticTab("Bases", "Bases and Raiding", "<h1>Bases and Raiding</h1><p>Add your base rules here.</p>"));
        cfg.Tabs.Insert(MakeStaticTab("AI Companions", "AI Companions", "<h1>AI Companions</h1><p>Add your companion information here.</p>"));
        AddMissingInfoTabs(cfg);

        cfg.Links.Insert(MakeLink("Discord", ""));
        cfg.Links.Insert(MakeLink("BuyMeACoffee", ""));
        cfg.Links.Insert(MakeLink("PayPal", ""));

        return cfg;
    }

    protected static void AddMissingTabs(BDE_MapUI_Config cfg)
    {
        bool playerFound;
        bool vipFound;
        bool aiFound;
        bool changesFound;
        foreach (BDE_MapUI_TabConfig existing : cfg.Tabs)
        {
            if (!existing) continue;
            if (existing.Type == BDE_MAPUI_TAB_PLAYER) playerFound = true;
            if (existing.Type == BDE_MAPUI_TAB_VIP_ADMIN) vipFound = true;
            if (existing.Label == "AI Companions") aiFound = true;
            if (existing.Label == "Changelog" || existing.Label == "Change Log") changesFound = true;
        }
        if (!playerFound) cfg.Tabs.Insert(MakeFeatureTab(BDE_MAPUI_TAB_PLAYER, "Player", "Player Information"));
        if (!vipFound) cfg.Tabs.Insert(MakeFeatureTab(BDE_MAPUI_TAB_VIP_ADMIN, "VIP", "VIP Manager"));
        if (!aiFound) cfg.Tabs.Insert(MakeStaticTab("AI Companions", "AI Companions", "<h1>AI Companions</h1><p>Add your companion information here.</p>"));
        if (!changesFound) cfg.Tabs.Insert(MakeStaticTab("Changelog", "Changelog", "<h1>Changelog</h1><p>Add your server updates here.</p>"));
    }

    protected static void AddMissingInfoTabs(BDE_MapUI_Config cfg)
    {
        int count;
        foreach (BDE_MapUI_TabConfig tab : cfg.Tabs)
        {
            if (!tab || tab.Type != BDE_MAPUI_TAB_STATIC)
            {
                continue;
            }
            string label = tab.Label;
            label.ToLower();
            if (label != "changelog" && label != "change log")
            {
                count++;
            }
        }
        int placeholder = 1;
        while (count < 5)
        {
            string placeholderLabel = "Coming Soon " + placeholder.ToString();
            placeholder++;
            bool exists = false;
            foreach (BDE_MapUI_TabConfig existing : cfg.Tabs)
            {
                if (existing && existing.Label == placeholderLabel)
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
            {
                continue;
            }
            cfg.Tabs.Insert(MakeStaticTab(placeholderLabel, placeholderLabel, "<p>More information coming soon.</p>"));
            count++;
        }
    }

    static void Validate(BDE_MapUI_Config cfg)
    {
        if (!cfg)
            return;

        if (cfg.PassiveIncomeAmount < 0)
            cfg.PassiveIncomeAmount = 0;
        if (cfg.PassiveRPAmount < 0)
            cfg.PassiveRPAmount = 0;
        if (cfg.MinBountyAmount < 1)
            cfg.MinBountyAmount = 1;

        int previousVersion = cfg.ConfigVersion;
        cfg.ConfigVersion = BDE_MAPUI_CONFIG_VERSION;

        if (cfg.BankLabel == "")
            cfg.BankLabel = "Bank";
        if (cfg.InventoryLabel == "")
            cfg.InventoryLabel = "Cash";
        if (cfg.SidebarRefreshSeconds < 5)
            cfg.SidebarRefreshSeconds = 5;
        if (cfg.SidebarRefreshSeconds > 300)
            cfg.SidebarRefreshSeconds = 300;

        if (!cfg.Tabs)
            cfg.Tabs = new array<ref BDE_MapUI_TabConfig>();
        if (!cfg.Links)
            cfg.Links = new array<ref BDE_MapUI_LinkConfig>();

        // v1 prototype migration: its old TopTabs field is intentionally ignored.
        // If the new Tabs array is absent/empty, install the current default feature set.
        if (cfg.Tabs.Count() == 0)
        {
            BDE_MapUI_Config defaults = CreateDefaults();
            cfg.Tabs = defaults.Tabs;
        }

        if (previousVersion < 5)
            AddMissingTabs(cfg);

        for (int i = 0; i < cfg.Tabs.Count(); i++)
        {
            BDE_MapUI_TabConfig tab = cfg.Tabs.Get(i);
            if (!tab)
            {
                tab = new BDE_MapUI_TabConfig();
                tab.Enabled = false;
                cfg.Tabs.Set(i, tab);
            }

            if (!tab.Content)
                tab.Content = new array<string>();
            if (tab.Type == "")
                tab.Type = BDE_MAPUI_TAB_STATIC;
        }

        AddMissingInfoTabs(cfg);
        if (!cfg.ReportAdminSteamIDs)
        {
            cfg.ReportAdminSteamIDs = new array<string>();
        }
        if (!cfg.ReportTypes)
        {
            cfg.ReportTypes = new array<string>();
        }
        if (cfg.ReportTypes.Count() == 0)
        {
            cfg.ReportTypes.Insert("Report Player");
            cfg.ReportTypes.Insert("Report Glitch");
            cfg.ReportTypes.Insert("Lost Item");
            cfg.ReportTypes.Insert("Other");
        }
        cfg.ReportCooldownSeconds = Math.Max(10, cfg.ReportCooldownSeconds);
        cfg.MaxOpenReportsPerPlayer = Math.Max(1, cfg.MaxOpenReportsPerPlayer);

        while (cfg.Links.Count() < 3)
        {
            BDE_MapUI_LinkConfig link = new BDE_MapUI_LinkConfig();
            cfg.Links.Insert(link);
        }

        while (cfg.Links.Count() > 3)
            cfg.Links.Remove(cfg.Links.Count() - 1);
    }
};
