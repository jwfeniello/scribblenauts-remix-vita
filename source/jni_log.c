#include <stdarg.h>
#include <stdio.h>
#include <falso_jni/FalsoJNI_Logger.h>
#include "utils/logger.h"

// Keep JNI diagnostics in the same persistent log as the loader.
#ifdef SCRIB_DIAGNOSTICS
#define JNI_LOG_FUNCTION(NAME, LEVEL) \
void NAME(const char *file, int line, const char *fn, const char *fmt, ...) { \
    char message[1024]; \
    va_list args; va_start(args, fmt); \
    vsnprintf(message, sizeof(message), fmt, args); va_end(args); \
    _log_print(LEVEL, "JNI %s: %s", fn, message); \
}
#else
#define JNI_LOG_FUNCTION(NAME, LEVEL) \
void NAME(const char *file, int line, const char *fn, const char *fmt, ...) {}
#endif
JNI_LOG_FUNCTION(_fjni_log_error, LT_ERROR)
JNI_LOG_FUNCTION(_fjni_log_warn, LT_WARN)
JNI_LOG_FUNCTION(_fjni_log_info, LT_DEBUG)
// Per-call JNI traces would swamp the startup log.
void _fjni_log_debug(const char *file, int line, const char *fn, const char *fmt, ...) {}
