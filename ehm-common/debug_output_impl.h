// ************************************************************************
// Implementations for debug functions
// No need to make changes here, modify statements in debug_output_control
// ************************************************************************

extern char mdMessageBuffer[255];

void md_printf(int len);


#ifdef ENABLE_DEBUG_OUTPUT_XBEE
	#define module_debug_xbee(fmt, ...)   printf("XBEE: "fmt"\n", ##__VA_ARGS__)
#elif defined(ZB_ENABLE_DEBUG_OUTPUT_XBEE)
	#define module_debug_xbee(fmt, ...)   md_printf(sprintf(mdMessageBuffer, "XBEE: "fmt"\n", ##__VA_ARGS__))
#else
	#define module_debug_xbee(fmt, ...)   
#endif

#ifdef ENABLE_DEBUG_OUTPUT_STRG
	#define module_debug_strg(fmt, ...)   printf("STRG: "fmt"\n", ##__VA_ARGS__)
#elif defined(ZB_ENABLE_DEBUG_OUTPUT_STRG)
	#define module_debug_strg(fmt, ...)   md_printf(sprintf(mdMessageBuffer, "STRG: "fmt"\n", ##__VA_ARGS__))
#else
	#define module_debug_strg(fmt, ...)   
#endif
