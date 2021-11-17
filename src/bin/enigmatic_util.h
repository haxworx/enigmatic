#ifndef ENIGMATIC_UTIL_H
#define ENIGMATIC_UTIL_H

#include "Enigmatic.h"

Eina_Bool
enigmatic_pidfile_create(Enigmatic *enigmatic);

const char *
enigmatic_cache_dir_get(void);

void
enigmatic_pidfile_delete(Enigmatic *enigmatic);

uint32_t
enigmatic_pidfile_pid_get(const char *path);

void
enigmatic_terminate(void);

Eina_Bool
enigmatic_launch(void);

Eina_Bool
enigmatic_running(void);

char *
enigmatic_log_path(void);

char *
enigmatic_log_directory(void);

void
enigmatic_nuke(void);

uint8_t
enigmatic_count(void);

#endif
