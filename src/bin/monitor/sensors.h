#ifndef MONITOR_SENSORS_H
#define MONITOR_SENSORS_H

#include "Enigmatic.h"

Eina_Bool
monitor_sensors(Enigmatic *enigmatic, Eina_Hash **cache_hash);

void
monitor_sensors_init(void);

void
monitor_sensors_shutdown(void);

#endif
