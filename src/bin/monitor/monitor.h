#ifndef MONITOR_H
#define MONITOR_H
#include "Enigmatic.h"
#include "cores.h"
#include "memory.h"
#include "batteries.h"
#include "power.h"
#include "sensors.h"
#include "file_systems.h"
#include "network_interfaces.h"
#include "processes.h"

void
monitor_init(Monitor *);

#endif
