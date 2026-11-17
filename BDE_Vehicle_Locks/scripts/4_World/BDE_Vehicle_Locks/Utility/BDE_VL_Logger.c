class BDE_VL_Logger
{
    static void Log(string message)
    {
        Print("[BDE Vehicle Locks] " + message);
    }

    static void Debug(string message)
    {
        BDE_VL_Settings settings = BDE_VL_Settings.Get();
        if (settings && settings.DebugLogging)
            Log(message);
    }
};
