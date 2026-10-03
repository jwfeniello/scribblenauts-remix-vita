/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/log.h"
#include "utils/logger.h"
#include <psp2/kernel/clib.h>
#include <stdlib.h>

#ifdef SCRIB_DIAGNOSTICS
#define print_common \
    switch (prio) { \
        case ANDROID_LOG_INFO: \
            l_info("[ALOG][%s] %s", tag, text); \
            break; \
        case ANDROID_LOG_WARN: \
            l_warn("[ALOG][%s] %s", tag, text); \
            break; \
        case ANDROID_LOG_ERROR: \
        case ANDROID_LOG_FATAL: \
            l_error("[ALOG][%s] %s", tag, text); \
            break; \
        case ANDROID_LOG_UNKNOWN: \
        case ANDROID_LOG_DEFAULT: \
        case ANDROID_LOG_VERBOSE: \
        case ANDROID_LOG_DEBUG: \
        case ANDROID_LOG_SILENT: \
        default: \
            l_debug("[ALOG][%s] %s", tag, text); \
            break; \
    }

int __android_log_write(int prio, const char* tag, const char* text) {
    if (prio < ANDROID_LOG_WARN && !scrib_log_verbose_enabled()) return 0;
    print_common
    return 0;
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    if (prio < ANDROID_LOG_WARN && !scrib_log_verbose_enabled()) return 0;
    va_list list;
    char text[1024];

    va_start(list, fmt);
    sceClibVsnprintf(text, sizeof(text), fmt, list);
    va_end(list);

    print_common

    return 0;
}

int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap) {
    if (prio < ANDROID_LOG_WARN && !scrib_log_verbose_enabled()) return 0;
    char text[1024];

    sceClibVsnprintf(text, sizeof(text), fmt, ap);

    print_common

    return 0;
}

void __android_log_assert(const char* cond, const char* tag, const char* fmt, ...) {
    if (fmt) {
        va_list list;
        char text[1024];

        va_start(list, fmt);
        sceClibVsnprintf(text, sizeof(text), fmt, list);
        va_end(list);

        l_fatal("[ALOG][ASSERT] %s", text);
    } else {
        if (cond) {
            l_fatal("[ALOG][ASSERT] Assertion failed: %s", cond);
        } else {
            l_fatal("[ALOG][ASSERT] Unspecified assertion failed");
        }
    }

    abort();
}
#else
int __android_log_write(int prio, const char *tag, const char *text) { return 0; }
int __android_log_print(int prio, const char *tag, const char *fmt, ...) { return 0; }
int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) { return 0; }
void __android_log_assert(const char *cond, const char *tag, const char *fmt, ...) { abort(); }
#endif

int scrib_quiet_printf(const char *fmt, ...) { return 0; }
int scrib_quiet_vprintf(const char *fmt, va_list args) { return 0; }
