class BDE_VG_Logger
{
    static void Log(string text, bool force = false)
    {
        BDE_VG_Settings settings = BDE_VG_Settings.Get();
        if (!force && (!settings || !settings.DebugLogging))
            return;
        string line = "[BDE_VG] " + text;
        Print(line);
        FileHandle file = OpenFile(BDE_VG_LOG_FILE,FileMode.APPEND);
        if (file)
        {
            FPrintln(file,line);
            CloseFile(file);
        }
    }
};
