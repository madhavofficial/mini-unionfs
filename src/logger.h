#ifndef LOGGER_H
#define LOGGER_H

void log_init(void);
void log_msg(const char *format, ...);
void log_close(void);

#endif // LOGGER_H