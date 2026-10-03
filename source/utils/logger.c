/* Persistent diagnostics for the first hardware builds. */
#include "utils/logger.h"
#include "scrib.h"
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifdef SCRIB_DIAGNOSTICS
static SceKernelLwMutexWork log_lock;
static int log_fd = -1;
static unsigned log_bytes;
static int verbose_log;

int scrib_log_verbose_enabled(void) { return verbose_log; }

void scrib_log_init(void) {
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir(DATA_PATH, 0777);
    sceKernelCreateLwMutex(&log_lock, "scrib_log", 0, 0, NULL);
    log_fd = sceIoOpen(DATA_PATH "loader.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    SceIoStat st;
    verbose_log = sceIoGetstat(DATA_PATH "verbose-log.enabled", &st) >= 0;
}

void _log_print(int type, const char *fmt, ...) {
    if (type == LT_DEBUG && !verbose_log) return;
    char message[1536], line[1664];
    va_list args; va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args); va_end(args);
    snprintf(line, sizeof(line), "[%u][%d] %s\n", sceKernelGetProcessTimeLow(), type, message);
    if (verbose_log || type == LT_ERROR || type == LT_FATAL) sceClibPrintf("%s", line);
    sceKernelLockLwMutex(&log_lock, 1, NULL);
    // Bound verbose disk traffic; fatal/errors still get written after the cap.
    if (log_fd >= 0 && (log_bytes < 8 * 1024 * 1024 || type == LT_ERROR || type == LT_FATAL)) {
        int count = sceIoWrite(log_fd, line, strlen(line));
        if (count > 0) log_bytes += count;
        // A forced storage flush for every touch/render trace cost many ms.
        // Routine game traces are filtered before formatting.
        if (type == LT_ERROR || type == LT_FATAL) sceIoSyncByFd(log_fd, 0);
    }
    sceKernelUnlockLwMutex(&log_lock, 1);
}
#else
int scrib_log_verbose_enabled(void) { return 0; }
void scrib_log_init(void) {}
void _log_print(int type, const char *fmt, ...) {}
#endif
