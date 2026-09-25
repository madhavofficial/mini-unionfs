#include "logger.h"
#include <stdio.h>
#include <stdarg.h>

static FILE *log_fp = NULL;

void log_init(void) {
    log_fp = fopen("/tmp/mini_unionfs.log", "w");
}

void log_msg(const char *format, ...) {
    if (!log_fp) return;
    va_list args;
    va_start(args, format);
    vfprintf(log_fp, format, args);
    va_end(args);
    fflush(log_fp);
}

void log_close(void) {
    if (log_fp) fclose(log_fp);
}