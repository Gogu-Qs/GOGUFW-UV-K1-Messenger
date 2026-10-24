
#ifdef VERSION_STRING
    #define VER     " "VERSION_STRING
#else
    #define VER     ""
#endif

#ifdef ENABLE_FEAT_F4HWN
    const char Version[]      = AUTHOR_STRING_2 " " VERSION_STRING_2;
    /* The CHIRP/UART handshake reserves 16 bytes including the terminator. */
    _Static_assert(sizeof(Version) <= 16u, "firmware identity exceeds UART version field");
    /* UV Studio 2.1 uses the compact v7 service generation to probe Apps and
       raw-flash support independently. Keep the dotted catalog identity at
       v6.3.0 so it selects the matching official API-3 application catalog. */
    const char HostVersion[]  = "v7 GGFW v6.3.0";
    _Static_assert(sizeof(HostVersion) <= 16u, "host identity exceeds UART version field");
    const char Edition[]      = EDITION_STRING;
    const char BuildDate[]    = __DATE__;
    const char BuildTime[]    = __TIME__;
    const char BuildCommit[]  = BUILD_COMMIT;
#else
    const char Version[]      = AUTHOR_STRING VER;
    const char HostVersion[]  = AUTHOR_STRING VER;
#endif

const char UART_Version[] = "UV-K5 Firmware, " AUTHOR_STRING VER "\r\n";
