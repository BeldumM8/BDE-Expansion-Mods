class BDE_MapUI_DisplayColors
{
    // Colors
    static const int 			WHITE				= ARGB(255, 255, 255, 255);
    static const int 			ORANGE				= ARGB(255, 255, 165, 0);
    static const int 			GREEN				= ARGB(255, 0, 206, 0);
    static const int 			RED					= ARGB(255, 255, 0, 0);
    static const int 			YELLOW				= ARGB(255, 255, 255, 0);
    static const int 			ACTIVE_BUTTON_COLOR	= ARGB(255, 153, 153, 153);
    static const int 			NORMAL_BUTTON_COLOR	= ARGB(255, 51, 51, 51);
}

class BDE_MapUI_Log
{
 static void Log(string message, string source) { Print("[BDE_MapUI] " + message); }
};
