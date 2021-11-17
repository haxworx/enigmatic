#include "monitor.h"

void
monitor_init(Monitor *mon)
{
   mon->cores = &monitor_cores;
   mon->memory = &monitor_memory;
   mon->batteries = &monitor_batteries;
   mon->power = &monitor_power;
   mon->sensors = &monitor_sensors;
   mon->network = &monitor_network_interfaces;
   mon->file_systems = &monitor_file_systems;
   mon->processes = &monitor_processes;
}

