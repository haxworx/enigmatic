#include "Events.h"
#include "enigmatic_log.h"
#include "enigmatic_util.h"
#include "Enigmatic_Client.h"
#include <Eina.h>
#include <Ecore.h>
#include <Ecore_File.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "lz4frame.h"

static uint32_t specialfriend[4] = { HEADER_MAGIC, HEADER_MAGIC, HEADER_MAGIC, HEADER_MAGIC };

#define FLOAT_VALID(x) ((x < 0) ? 0 : (x))

static void
free_snapshot(Snapshot *s)
{
   Cpu_Core *core;
   EINA_LIST_FREE(s->cores, core)
     free(core);

   Sensor *sensor;
   EINA_LIST_FREE(s->sensors, sensor)
     free(sensor);

   Battery *battery;
   EINA_LIST_FREE(s->batteries, battery)
     free(battery);

   Network_Interface *iface;
   EINA_LIST_FREE(s->network_interfaces, iface)
     free(iface);

   File_System *fs;
   EINA_LIST_FREE(s->file_systems, fs)
     free(fs);

   Proc_Stubby *proc;
   EINA_LIST_FREE(s->processes, proc)
     proc_stubby_free(proc);
}

static void
buffer_clear(Buffer *buf)
{
   if (buf->data)
     free(buf->data);
   buf->data = NULL;
   buf->length = 0;
   buf->index = 0;
}

static void
client_reset(Enigmatic_Client *client)
{
   buffer_clear(&client->zbuf);
   buffer_clear(&client->buf);
   client->offset = 0;
   client->file_size = 0;
   client->truncated = 0;
   free_snapshot(&client->snapshot);
}

static Enigmatic_Client_Event *
event_create(Enigmatic_Client *client, void *data)
{
   Enigmatic_Client_Event *ev = malloc(sizeof(Enigmatic_Client_Event));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ev, NULL);

   ev->time = client->header.time;
   ev->data = data;

   return ev;
}

static float
change_float(Enigmatic_Client *client)
{
   float *change;

   change = (float *) &client->buf.data[client->buf.index];

   client->buf.index += sizeof(float);

   return *change;
}

static Eina_Bool
callback_fire(Enigmatic_Client *client)
{
   if (client->follow)
     return 1;

   if (!client->replay.enabled)
     return 0;

   if ((client->header.time >= client->replay.start_time) && (client->header.time <= client->replay.end_time))
     return 1;

   return 0;
}

Eina_Bool
client_event_is_snapshot(Enigmatic_Client *client)
{
   return client->header.event == EVENT_BLOCK_END;
}

static int64_t
change_find(Enigmatic_Client *client)
{
   Change change;
   int64_t value = 0;
   off_t offset = 0;
   int8_t  *d8;
   int16_t *d16;
   int32_t *d32;
   int64_t *d64;

   change = client->change;

   switch (change)
     {
        case CHANGE_I8:
           d8 = (int8_t *) &client->buf.data[client->buf.index];
           value = *d8;
           offset = sizeof(int8_t);
           break;
        case CHANGE_I16:
           d16 = (int16_t *) &client->buf.data[client->buf.index];
           value = *d16;
           offset = sizeof(int16_t);
           break;
        case CHANGE_I32:
           d32 = (int32_t *) &client->buf.data[client->buf.index];
           value = *d32;
           offset = sizeof(int32_t);
           break;
        case CHANGE_I64:
           d64 = (int64_t *) &client->buf.data[client->buf.index];
           value = *d64;
           offset = sizeof(int64_t);
           break;
        default:
           fprintf(stderr, "Not supported!\n");
           exit(1);
           break;
     }

   client->buf.index += offset;

   return value;
}

static void
message_processes(Enigmatic_Client *client)
{
   Eina_List *l, *l2;
   Proc_Stubby *proc, *p2;
   Eina_Bool update = 1;
   int64_t change;
   Snapshot *snapshot;
   Message *msg = &client->message;

   snapshot = &client->snapshot;

   switch (msg->type)
     {
        case MESG_REFRESH:
           if (!snapshot->processes) update = 0;
           for (int i = 0; i < msg->number; i++)
             {
                proc = malloc(sizeof(Proc_Stubby));
                EINA_SAFETY_ON_NULL_RETURN(proc);

                memcpy(proc, &client->buf.data[client->buf.index], sizeof(Proc_Stubby));

                client->buf.index += sizeof(Proc_Stubby);
                char *cp = (char *) &client->buf.data[client->buf.index];
                proc->command = strdup(cp);
                client->buf.index += (strlen(proc->command) + 1);
                cp = (char *) &client->buf.data[client->buf.index];
                proc->path = strdup(cp);
                client->buf.index += (strlen(proc->path) + 1);

                if (!update)
                  snapshot->processes = eina_list_append(snapshot->processes, proc);
                else
                  {
                     EINA_LIST_FOREACH(snapshot->processes, l, p2)
                       {
                          if (p2->pid == proc->pid)
                            {
                               p2->mem_size = proc->mem_size;
                            }
                       }
                     proc_stubby_free(proc);
                  }
             }
           break;
        case MESG_ADD:
           for (int i = 0; i < msg->number; i++)
             {
                proc = malloc(sizeof(Proc_Stubby));
                EINA_SAFETY_ON_NULL_RETURN(proc);

                memcpy(proc, &client->buf.data[client->buf.index], sizeof(Proc_Stubby));
                client->buf.index += sizeof(Proc_Stubby);
                char *cp = (char *) &client->buf.data[client->buf.index];
                proc->command = strdup(cp);
                client->buf.index += (strlen(proc->command) + 1);
                cp = (char *) &client->buf.data[client->buf.index];
                proc->path = strdup(cp);
                client->buf.index += (strlen(proc->path) + 1);
                snapshot->processes = eina_list_append(snapshot->processes, proc);
                if ((client->event_process_add.callback) && (callback_fire(client)))
                  {
                     Enigmatic_Client_Event *ev = event_create(client, proc);
                     if (ev)
                       {
                          client->event_process_add.callback(client, ev,
                                                             client->event_process_add.data);
                          free(ev);
                       }
                  }
             }
           break;
        case MESG_MOD:
           change = change_find(client);
           EINA_LIST_FOREACH(snapshot->processes, l, proc)
             {
                if (proc->pid == msg->number)
                  {
                     if (msg->object_type == PROCESS_MEM_SIZE)
                       proc->mem_size += (change * 4096);
                     else if (msg->object_type == PROCESS_CPU_USAGE)
                       proc->cpu_usage = change;  // XXX value not delta.
                     else if (msg->object_type == PROCESS_NUM_THREAD)
                       proc->numthreads = change; // XXX value not delta.
                     else if (msg->object_type == PROCESS_PRIORITY)
                       proc->priority = change;   // XXX value not delta.
                  }
             }
           break;
        case MESG_DEL:
           EINA_LIST_FOREACH_SAFE(snapshot->processes, l, l2, proc)
             {
                if (proc->pid == msg->number)
                  {
                     if ((client->event_process_del.callback) && (callback_fire(client)))
                       {
                          Enigmatic_Client_Event *ev = event_create(client, proc);
                          if (ev)
                            {
                               client->event_process_del.callback(client, ev,
                                                                  client->event_process_del.data);
                               free(ev);
                            }
                       }
                     proc_stubby_free(proc);
                     snapshot->processes = eina_list_remove_list(snapshot->processes, l);
                  }
             }
           break;
        default:
           fprintf(stderr, "message_processes!!!\n");
           exit(1);
     }
   client->changes |= PROCESS;
}

static void
message_file_system(Enigmatic_Client *client)
{
   Eina_List *l, *l2;
   File_System *fs, *fs2;
   Eina_Bool update = 1;
   int64_t change;
   Snapshot *snapshot;
   Message *msg = &client->message;

   snapshot = &client->snapshot;

   switch (msg->type)
     {
        case MESG_REFRESH:
           if (!snapshot->file_systems) update = 0;
           for (int i = 0; i < msg->number; i++)
             {
                File_System *fs = malloc(sizeof(File_System));
                EINA_SAFETY_ON_NULL_RETURN(fs);

                memcpy(fs, &client->buf.data[client->buf.index], sizeof(File_System));
                client->buf.index += sizeof(File_System);

                if (!update)
                  snapshot->file_systems = eina_list_append(snapshot->file_systems, fs);
                else
                  {
                     EINA_LIST_FOREACH(snapshot->file_systems, l, fs2)
                       {
                          if (fs2->unique_id == fs->unique_id)
                            {
                               fs2->usage.total = fs->usage.total;
                               fs2->usage.used = fs->usage.used;
                            }
                       }
                     free(fs);
                  }
             }
           break;
        case MESG_ADD:
           for (int i = 0; i < msg->number; i++)
             {
                File_System *fs = malloc(sizeof(File_System));
                EINA_SAFETY_ON_NULL_RETURN(fs);

                memcpy(fs, &client->buf.data[client->buf.index], sizeof(File_System));
                client->buf.index += sizeof(File_System);
                snapshot->file_systems = eina_list_append(snapshot->file_systems, fs);
                if ((client->event_file_system_add.callback) && (callback_fire(client)))
                  {
                     Enigmatic_Client_Event *ev = event_create(client, fs);
                     if (ev)
                       {
                          client->event_file_system_add.callback(client, ev,
                                                                 client->event_file_system_add.data);
                          free(ev);
                       }
                  }
             }
           break;
        case MESG_MOD:
           change = change_find(client);
           EINA_LIST_FOREACH(snapshot->file_systems, l, fs)
             {
                if (fs->unique_id == msg->number)
                  {
                     if (msg->object_type == FILE_SYSTEM_TOTAL)
                       fs->usage.total += change;
                     else if (msg->object_type == FILE_SYSTEM_USED)
                       fs->usage.used += change;
                  }
             }
           break;
        case MESG_DEL:
           EINA_LIST_FOREACH_SAFE(snapshot->file_systems, l, l2, fs)
             {
                if (fs->unique_id == msg->number)
                  {
                     if ((client->event_file_system_del.callback) && (callback_fire(client)))
                       {
                          Enigmatic_Client_Event *ev = event_create(client, fs);
                          if (ev)
                            {
                               client->event_file_system_del.callback(client, ev,
                                                                      client->event_file_system_del.data);
                               free(ev);
                            }
                       }
                     free(fs);
                     snapshot->file_systems = eina_list_remove_list(snapshot->file_systems, l);
                  }
             }
           break;
        default:
           fprintf(stderr, "message_network!!!\n");
           exit(1);
     }
   client->changes |= FILE_SYSTEM;
}

static void
message_network(Enigmatic_Client *client)
{
   Eina_List *l, *l2;
   Network_Interface *iface, *iface2;
   Eina_Bool update = 1;
   int64_t change;
   Snapshot *snapshot;
   Message *msg = &client->message;

   snapshot = &client->snapshot;

   switch (msg->type)
     {
        case MESG_REFRESH:
           if (!snapshot->network_interfaces) update = 0;
           for (int i = 0; i < msg->number; i++)
             {
                Network_Interface *iface = malloc(sizeof(Network_Interface));
                EINA_SAFETY_ON_NULL_RETURN(iface);

                memcpy(iface, &client->buf.data[client->buf.index], sizeof(Network_Interface));
                client->buf.index += sizeof(Network_Interface);

                if (!update)
                  snapshot->network_interfaces = eina_list_append(snapshot->network_interfaces, iface);
                else
                  {
                     EINA_LIST_FOREACH(snapshot->network_interfaces, l, iface2)
                       {
                          if (iface2->unique_id == iface->unique_id)
                            {
                               iface2->total_in = iface->total_in;
                               iface2->total_out = iface->total_out;
                            }
                       }
                     free(iface);
                  }
             }
           break;
        case MESG_ADD:
           for (int i = 0; i < msg->number; i++)
             {
                Network_Interface *iface = malloc(sizeof(Network_Interface));
                EINA_SAFETY_ON_NULL_RETURN(iface);

                memcpy(iface, &client->buf.data[client->buf.index], sizeof(Network_Interface));
                client->buf.index += sizeof(Network_Interface);
                snapshot->network_interfaces = eina_list_append(snapshot->network_interfaces, iface);
                if ((client->event_network_iface_add.callback) && (callback_fire(client)))
                  {
                     Enigmatic_Client_Event *ev = event_create(client, iface);
                     if (ev)
                       {
                          client->event_network_iface_add.callback(client, ev,
                                                                   client->event_network_iface_add.data);
                          free(ev);
                       }
                  }
             }
           break;
        case MESG_MOD:
           change = change_find(client);
           EINA_LIST_FOREACH(snapshot->network_interfaces, l, iface)
             {
                if (iface->unique_id == msg->number)
                  {
                     if (msg->object_type == NETWORK_INCOMING)
                       iface->total_in += change;
                     else if (msg->object_type == NETWORK_OUTGOING)
                       iface->total_out += change;
                  }
             }
           break;
        case MESG_DEL:
           EINA_LIST_FOREACH_SAFE(snapshot->network_interfaces, l, l2, iface)
             {
                if (iface->unique_id == msg->number)
                  {
                     if ((client->event_network_iface_del.callback) && (callback_fire(client)))
                       {
                          Enigmatic_Client_Event *ev = event_create(client, iface);
                          if (ev)
                            {
                               client->event_network_iface_del.callback(client, ev,
                                                                        client->event_network_iface_del.data);
                               free(ev);
                            }
                       }
                     free(iface);
                     snapshot->network_interfaces = eina_list_remove_list(snapshot->network_interfaces, l);
                  }
             }
           break;
        default:
           fprintf(stderr, "message_network!!!\n");
           exit(1);
     }
   client->changes |= NETWORK;
}

static void
message_power(Enigmatic_Client *client)
{
   Eina_Bool *change;
   Snapshot *snapshot;
   Message *msg = &client->message;

   snapshot = &client->snapshot;

   switch (msg->type)
     {
        case MESG_REFRESH:
           memcpy(&snapshot->power, &client->buf.data[client->buf.index], sizeof(Eina_Bool));
           client->buf.index += sizeof(Eina_Bool);
           break;
        case MESG_MOD:
           change = (Eina_Bool *) &client->buf.data[client->buf.index];
           switch (msg->object_type)
             {
                case POWER_VALUE:
                   snapshot->power = *change;
                   if ((snapshot->power) && (client->event_power_supply_add.callback) && (callback_fire(client)))
                     {
                        Enigmatic_Client_Event *ev = event_create(client, &snapshot->power);
                        if (ev)
                          {
                             client->event_power_supply_add.callback(client, ev,
                                                                     client->event_power_supply_add.data);
                             free(ev);
                          }
                     }
                   else if ((client->event_power_supply_del.callback) && (callback_fire(client)))
                     {
                        Enigmatic_Client_Event *ev = event_create(client, &snapshot->power);
                        if (ev)
                          {
                             client->event_power_supply_del.callback(client, ev,
                                                                     client->event_power_supply_del.data);
                             free(ev);
                          }
                     }
                   break;
                default:
                   fprintf(stderr, "message_power!!!\n");
                   exit(1);
             }
           client->buf.index += 1;
           break;
        default:
           break;
     }
   client->changes |= POWER;
}

static void
message_sensors(Enigmatic_Client *client)
{
   Eina_List *l, *l2;
   Sensor *sensor, *s1;
   Eina_Bool update = 1;
   float change;
   Snapshot *snapshot;
   Message *msg = &client->message;

   snapshot = &client->snapshot;

   switch (msg->type)
     {
        case MESG_REFRESH:
           if (!snapshot->sensors) update = 0;
           for (int i = 0; i < msg->number; i++)
             {
                Sensor *sensor = malloc(sizeof(Sensor));
                EINA_SAFETY_ON_NULL_RETURN(sensor);

                memcpy(sensor, &client->buf.data[client->buf.index], sizeof(Sensor));
                client->buf.index += sizeof(Sensor);

                if (!update)
                  snapshot->sensors = eina_list_append(snapshot->sensors, sensor);
                else
                  {
                     EINA_LIST_FOREACH(snapshot->sensors, l, s1)
                       {
                          if (s1->unique_id == sensor->unique_id)
                            sensor->value = s1->value;
                       }
                     free(sensor);
                  }
             }
           break;
        case MESG_ADD:
           for (int i = 0; i < msg->number; i++)
             {
                Sensor *sensor = malloc(sizeof(Sensor));
                EINA_SAFETY_ON_NULL_RETURN(sensor);

                memcpy(sensor, &client->buf.data[client->buf.index], sizeof(Sensor));
                client->buf.index += sizeof(Sensor);
                snapshot->sensors = eina_list_append(snapshot->sensors, sensor);
                if ((client->event_sensor_add.callback) && (callback_fire(client)))
                  {
                     Enigmatic_Client_Event *ev = event_create(client, sensor);
                     if (ev)
                       {
                          client->event_sensor_add.callback(client, ev, client->event_sensor_add.data);
                          free(ev);
                       }
                  }
             }
           break;
        case MESG_MOD:
           change = change_float(client);
           EINA_LIST_FOREACH(snapshot->sensors, l, sensor)
             {
                if (sensor->unique_id == msg->number)
                  {
                     if (msg->object_type == SENSOR_VALUE)
                       sensor->value = FLOAT_VALID(sensor->value + change);
                  }
             }
           break;
        case MESG_DEL:
           EINA_LIST_FOREACH_SAFE(snapshot->sensors, l, l2, sensor)
             {
                if (sensor->unique_id == msg->number)
                  {
                     if ((client->event_sensor_del.callback) && (callback_fire(client)))
                       {
                          Enigmatic_Client_Event *ev = event_create(client, sensor);
                          if (ev)
                            {
                               client->event_sensor_del.callback(client, ev, client->event_sensor_del.data);
                               free(ev);
                            }
                       }
                     free(sensor);
                     snapshot->sensors = eina_list_remove_list(snapshot->sensors, l);
                  }
             }
           break;
        default:
           fprintf(stderr, "message_sensors!!!\n");
           exit(1);
     }
   client->changes |= SENSOR;
}

static void
message_batteries(Enigmatic_Client *client)
{
   Eina_List *l, *l2;
   Battery *bat, *b1;
   Eina_Bool update = 1;
   float fchange = 0;
   int32_t change = 0;
   Snapshot *snapshot;
   Message *msg = &client->message;

   snapshot = &client->snapshot;

   switch (msg->type)
     {
        case MESG_REFRESH:
           if (!snapshot->batteries) update = 0;
           for (int i = 0; i < msg->number; i++)
             {
                Battery *bat = malloc(sizeof(Battery));
                EINA_SAFETY_ON_NULL_RETURN(bat);

                memcpy(bat, &client->buf.data[client->buf.index], sizeof(Battery));
                client->buf.index += sizeof(Battery);

                if (!update)
                  snapshot->batteries = eina_list_append(snapshot->batteries, bat);
                else
                  {
                     EINA_LIST_FOREACH(snapshot->batteries, l, b1)
                       {
                          if (b1->unique_id == bat->unique_id)
                            {
                               bat->percent = b1->percent;
                               bat->charge_full = b1->charge_full;
                               bat->charge_current = b1->charge_current;
                            }
                       }
                     free(bat);
                  }
             }
           break;
        case MESG_ADD:
           for (int i = 0; i < msg->number; i++)
             {
                Battery *bat = malloc(sizeof(Battery));
                EINA_SAFETY_ON_NULL_RETURN(bat);

                memcpy(bat, &client->buf.data[client->buf.index], sizeof(Battery));
                snapshot->batteries = eina_list_append(snapshot->batteries, bat);
                client->buf.index += sizeof(Battery);
                if ((client->event_battery_add.callback) && (callback_fire(client)))
                  {
                     Enigmatic_Client_Event *ev = event_create(client, bat);
                     if (ev)
                       {
                          client->event_battery_add.callback(client, ev, client->event_battery_add.data);
                          free(ev);
                       }
                  }
             }
           break;
        case MESG_MOD:
           if (client->change == CHANGE_FLOAT)
             fchange = change_float(client);
           else
             change = change_find(client);
           EINA_LIST_FOREACH(snapshot->batteries, l, bat)
             {
                if (bat->unique_id == msg->number)
                  {
                     if (msg->object_type == BATTERY_PERCENT)
                       bat->percent = FLOAT_VALID(bat->percent + fchange);
                     else if (msg->object_type == BATTERY_FULL)
                       bat->charge_full += change;
                     else if (msg->object_type == BATTERY_CURRENT)
                       bat->charge_current += change;
                  }
             }
           break;
        case MESG_DEL:
           EINA_LIST_FOREACH_SAFE(snapshot->batteries, l, l2, bat)
             {
                if (bat->unique_id == msg->number)
                  {
                     if ((client->event_battery_del.callback) && (callback_fire(client)))
                       {
                          Enigmatic_Client_Event *ev = event_create(client, bat);
                          if (ev)
                            {
                               client->event_battery_del.callback(client, ev, client->event_battery_del.data);
                               free(ev);
                            }
                       }
                     free(bat);
                     snapshot->batteries = eina_list_remove_list(snapshot->batteries, l);
                  }
             }
           break;
        default:
           fprintf(stderr, "message_batteries!!!\n");
           exit(1);
     }
   client->changes |= BATTERY;
}

static void
message_memory(Enigmatic_Client *client)
{
   int64_t change;
   Snapshot *snapshot;
   Message *msg = &client->message;

   snapshot = &client->snapshot;

   switch (msg->type)
     {
        case MESG_REFRESH:
           memcpy(&snapshot->meminfo, &client->buf.data[client->buf.index], sizeof(Meminfo));
           client->buf.index += sizeof(Meminfo);
           break;
        case MESG_MOD:
           change = change_find(client);
           switch (msg->object_type)
             {
                case MEMORY_TOTAL:
                   snapshot->meminfo.total += (change * 4096);
                   break;
                case MEMORY_USED:
                   snapshot->meminfo.used += (change * 4096);
                   break;
                case MEMORY_CACHED:
                   snapshot->meminfo.cached += (change * 4096);
                   break;
                case MEMORY_BUFFERED:
                   snapshot->meminfo.buffered += (change * 4096);
                   break;
                case MEMORY_SHARED:
                   snapshot->meminfo.shared += (change * 4096);
                   break;
                case MEMORY_SWAP_TOTAL:
                   snapshot->meminfo.swap_total += (change * 4096);
                   break;
                case MEMORY_SWAP_USED:
                   snapshot->meminfo.swap_used += (change * 4096);
                   break;
                case MEMORY_VIDEO_USED:
                   snapshot->meminfo.video[msg->number].used += (change * 4096);
                   break;
                case MEMORY_VIDEO_TOTAL:
                   snapshot->meminfo.video[msg->number].total += (change * 4096);
                   break;
                default:
                   fprintf(stderr, "message_memory!!!\n");
                   exit(1);
             }
           break;
        default:
           break;
     }
   client->changes |= MEMORY;
}

static void
message_cores(Enigmatic_Client *client)
{
   Eina_List *l, *l2;
   Cpu_Core *core, *c1;
   Eina_Bool update = 1;
   int32_t change;
   Snapshot *snapshot;
   Message *msg = &client->message;

   snapshot = &client->snapshot;

   switch (msg->type)
     {
        case MESG_REFRESH:
           if (!snapshot->cores) update = 0;
           for (int i = 0; i < msg->number; i++)
             {
                Cpu_Core *core = malloc(sizeof(Cpu_Core));
                EINA_SAFETY_ON_NULL_RETURN(core);

                memcpy(core, &client->buf.data[client->buf.index], sizeof(Cpu_Core));
                client->buf.index += sizeof(Cpu_Core);

                if (!update)
                  snapshot->cores = eina_list_append(snapshot->cores, core);
                else
                  {
                     EINA_LIST_FOREACH(snapshot->cores, l, c1)
                       {
                          if (c1->unique_id == core->unique_id)
                            {
                               core->percent = c1->percent;
                               core->temp = c1->temp;
                               core->freq = c1->freq;
                            }
                       }
                     free(core);
                  }
             }
           break;
        case MESG_ADD:
           for (int i = 0; i < msg->number; i++)
             {
                Cpu_Core *core = malloc(sizeof(Cpu_Core));
                EINA_SAFETY_ON_NULL_RETURN(core);

                memcpy(core, &client->buf.data[client->buf.index], sizeof(Cpu_Core));
                client->buf.index += sizeof(Cpu_Core);

                snapshot->cores = eina_list_append(snapshot->cores, core);
                if ((client->event_cpu_add.callback) && (callback_fire(client)))
                  {
                     Enigmatic_Client_Event *ev = event_create(client, core);
                     if (ev)
                       {
                          client->event_cpu_add.callback(client, ev, client->event_cpu_add.data);
                          free(ev);
                       }
                  }
             }
           break;
        case MESG_MOD:
           change = change_find(client);

           EINA_LIST_FOREACH(snapshot->cores, l, core)
             {
                if (core->unique_id == msg->number)
                  {
                     if (msg->object_type == CPU_CORE_PERC)
                       core->percent += change;
                     else if (msg->object_type == CPU_CORE_TEMP)
                       core->temp += change;
                     else if (msg->object_type == CPU_CORE_FREQ)
                       core->freq += change;
                  }
             }
           break;
        case MESG_DEL:
           EINA_LIST_FOREACH_SAFE(snapshot->cores, l, l2, core)
             {
                if (core->unique_id == msg->number)
                  {
                     if ((client->event_cpu_del.callback) && (callback_fire(client)))
                       {
                          Enigmatic_Client_Event *ev = event_create(client, core);
                          if (ev)
                            {
                               client->event_cpu_del.callback(client, ev, client->event_cpu_del.data);
                               free(ev);
                            }
                       }
                     free(core);
                     snapshot->cores = eina_list_remove_list(snapshot->cores, l);
                  }
             }
           break;
        default:
           fprintf(stderr, "message_cores!!!\n");
           exit(1);
     }
   client->changes |= CPU_CORE;
}

static void
message_refresh(Enigmatic_Client *client)
{
   Message *msg = &client->message;

   switch (msg->object_type)
     {
        case CPU_CORE:
           message_cores(client);
           break;
        case MEMORY:
           message_memory(client);
           break;
        case SENSOR:
           message_sensors(client);
           break;
        case POWER:
           message_power(client);
           break;
        case BATTERY:
           message_batteries(client);
           break;
        case NETWORK:
           message_network(client);
           break;
        case FILE_SYSTEM:
           message_file_system(client);
           break;
        case PROCESS:
           message_processes(client);
           break;
        default:
           break;
     }
}

static void
message_add(Enigmatic_Client *client)
{
   Message *msg = &client->message;

   switch (msg->object_type)
     {
        case CPU_CORE:
           message_cores(client);
           break;
        case MEMORY:
           break;
        case SENSOR:
           message_sensors(client);
           break;
        case POWER:
           message_power(client);
           break;
        case BATTERY:
           message_batteries(client);
           break;
        case NETWORK:
           message_network(client);
           break;
        case FILE_SYSTEM:
           message_file_system(client);
           break;
        case PROCESS:
           message_processes(client);
           break;
        default:
           break;
     }
}

static void
message_mod(Enigmatic_Client *client)
{
   Message *msg = &client->message;

   memcpy(&client->change, &client->buf.data[client->buf.index], sizeof(Change));
   client->buf.index += sizeof(Change);

   switch (msg->object_type)
     {
        case CPU_CORE_PERC:
        case CPU_CORE_TEMP:
        case CPU_CORE_FREQ:
           message_cores(client);
           break;
        case MEMORY_TOTAL:
        case MEMORY_USED:
        case MEMORY_CACHED:
        case MEMORY_BUFFERED:
        case MEMORY_SHARED:
        case MEMORY_SWAP_TOTAL:
        case MEMORY_SWAP_USED:
        case MEMORY_VIDEO_TOTAL:
        case MEMORY_VIDEO_USED:
           message_memory(client);
           break;
        case SENSOR_VALUE:
           message_sensors(client);
           break;
        case POWER_VALUE:
           message_power(client);
           break;
        case BATTERY_PERCENT:
        case BATTERY_FULL:
        case BATTERY_CURRENT:
           message_batteries(client);
           break;
        case NETWORK_INCOMING:
        case NETWORK_OUTGOING:
           message_network(client);
           break;
        case FILE_SYSTEM_TOTAL:
        case FILE_SYSTEM_USED:
           message_file_system(client);
           break;
        case PROCESS_MEM_SIZE:
        case PROCESS_CPU_USAGE:
        case PROCESS_NUM_THREAD:
        case PROCESS_PRIORITY:
           message_processes(client);
           break;
        default:
           break;
      }
}

static void
message_del(Enigmatic_Client *client)
{
   Message *msg = &client->message;

   switch (msg->object_type)
     {
        case CPU_CORE:
           message_cores(client);
           break;
        case MEMORY:
           break;
        case SENSOR:
           message_sensors(client);
           break;
        case BATTERY:
           message_batteries(client);
           break;
        case NETWORK:
           message_network(client);
           break;
        case FILE_SYSTEM:
           message_file_system(client);
           break;
        case PROCESS:
           message_processes(client);
           break;
        default:
           break;
      }
}

static void
event_message(Enigmatic_Client *client)
{
   memcpy(&client->message, &client->buf.data[client->buf.index], sizeof(Message));
   client->buf.index += sizeof(Message);
   switch (client->message.type)
     {
        case MESG_ERROR:
           break;
        case MESG_REFRESH:
           message_refresh(client);
           break;
        case MESG_ADD:
           message_add(client);
           break;
        case MESG_MOD:
           message_mod(client);
           break;
        case MESG_DEL:
           message_del(client);
           break;
     }
}

static void
event_block_end(Enigmatic_Client *client)
{
   client->snapshot.time = client->header.time;

   if ((!client->follow) && (client->event_snapshot.callback) && (callback_fire(client)))
     {
        client->event_snapshot.callback(client, &client->snapshot, client->event_snapshot.data);
     }
}

static void
event_broadcast(Enigmatic_Client *client)
{
   memcpy(&client->interval, &client->buf.data[client->buf.index], sizeof(Interval));
   client->buf.index += sizeof(Interval);
   client->buf.index += sizeof(specialfriend);
   if (client->snapshot.last_record)
     {
        if ((client->event_record_delay.callback) && (callback_fire(client)))
          {
             uint32_t *delay = malloc(sizeof(uint32_t));
             EINA_SAFETY_ON_NULL_RETURN(delay);

             *delay = client->header.time - client->snapshot.time;
             Enigmatic_Client_Event *ev = event_create(client, delay);
             if (ev)
               {
                  client->event_record_delay.callback(client, ev,
                                                      client->event_record_delay.data);
                  free(ev);
               }
             free(delay);
          }
        client->snapshot.last_record = 0;
     }
}

static void
event_last_record(Enigmatic_Client *client)
{
   client->snapshot.last_record = 1;
   free_snapshot(&client->snapshot);
}

static void
event_end_of_file(Enigmatic_Client *client EINA_UNUSED)
{

}

Enigmatic_Client *
client_open(void)
{
   return client_path_open(enigmatic_log_path());
}

Enigmatic_Client *
client_path_open(char *filename)
{
   Enigmatic_Client *client = calloc(1, sizeof(Enigmatic_Client));
   EINA_SAFETY_ON_NULL_RETURN_VAL(client, NULL);

   client->fd = -1;
   client->directory = enigmatic_log_directory();
   client->filename = filename;

   char *ext = strrchr(filename, '.');
   if ((ext) && (!strcmp(ext, ".lz4")))
     client->compressed = 1;
   else
     {
        client->fd = open(client->filename, O_RDONLY);
        if (client->fd == -1)
          ERROR("No log");
     }

   return client;
}

static Eina_Bool
client_reopen(Enigmatic_Client *client, char *filename)
{
   client_reset(client);

   if (client->fd != -1)
     close(client->fd);
   if (client->filename)
     free(client->filename);
   if (client->directory)
     free(client->directory);
   client->fd = -1;
   client->directory = enigmatic_log_directory();
   client->filename = filename;

   char *ext = strrchr(filename, '.');
   if ((ext) && (!strcmp(ext, ".lz4")))
     client->compressed = 1;
   else
     {
        client->fd = open(client->filename, O_RDONLY);
        if (client->fd == -1)
          ERROR("No log");
        client->compressed = 0;
     }

   return 1;
}

void
client_del(Enigmatic_Client *client)
{
   if (client->follow)
     {
#if defined(__linux__)
        eio_monitor_del(client->mon);
        ecore_event_handler_del(client->handler);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
        ecore_thread_cancel(client->thread);
        ecore_thread_wait(client->thread, 1.0);
#endif
     }
   free_snapshot(&client->snapshot);
   if (client->fd != -1)
     close(client->fd);
   free(client->filename);
   free(client->directory);
   free(client);
}

static size_t
get_block_size(const LZ4F_frameInfo_t *info)
{
   switch (info->blockSizeID)
     {
        case LZ4F_default:
        case LZ4F_max64KB:
          return 1 << 16;
        case LZ4F_max256KB:
          return 1 << 18;
        case LZ4F_max1MB:
          return 1 << 20;
        case LZ4F_max4MB:
          return 1 << 22;
        default:
          ERROR("frame spec");
     }
}

// WIP
void
client_read(Enigmatic_Client *client)
{
   struct stat st;
   int n;
   Eina_Bool eof = 0;
   LZ4F_dctx *dctx = NULL;

   size_t status = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
   if (LZ4F_isError(status))
     ERROR("create decompress context");

   if (client->truncated)
     {
        close(client->fd);
        client->fd = open(client->filename, O_RDONLY);
        if (client->fd == -1)
          ERROR("open() %s\n", strerror(errno));
        client_reset(client);
     }

   if (!client->compressed)
     {
        if (fstat(client->fd, &st) == -1)
          ERROR("fstat() %s\n", strerror(errno));
     }
   else
     {
        client->zbuf.data = (uint8_t *) enigmatic_log_decompress(client->filename, &client->zbuf.length);
        st.st_size = client->zbuf.length;
     }

   client->changes = 0;
   size_t offset = 0;

   while ((!eof) && ((st.st_size - client->offset) > 0))
     {
        client->zbuf.length = st.st_size - client->offset;
        client->file_size = st.st_size;

        if (!client->compressed)
          {
             client->zbuf.data = malloc(client->zbuf.length * sizeof(uint8_t));
             EINA_SAFETY_ON_NULL_RETURN(client->zbuf.data);
          }

        uint32_t bytes = 0;

        while ((!client->compressed) && (bytes < client->zbuf.length))
          {
             char chunk[16384];
             size_t wanted = (client->zbuf.length - bytes) >= sizeof(chunk) ? sizeof(chunk) : client->zbuf.length - bytes;
             n = read(client->fd, chunk, wanted);
             if (n == 0)
               {
                  eof = 1;
                  break;
               }
             else if (n == -1)
               ERROR("read() %s", strerror(errno));

             memcpy(&client->zbuf.data[bytes], chunk, n);
             bytes += n;
             client->offset +=n;
          }

        size_t compressed_size = client->zbuf.length;

        LZ4F_frameInfo_t info;

        while ((!eof) && (offset < client->zbuf.length))
          {
             uint8_t *src = client->zbuf.data + offset;
             status = LZ4F_getFrameInfo(dctx, &info, src, &compressed_size);
             if (LZ4F_isError(status))
               ERROR("getFrameInfo: %s", LZ4F_getErrorName(status));
             size_t block_size = get_block_size(&info);
             char *dst = malloc(block_size);
             EINA_SAFETY_ON_NULL_RETURN(dst);

             size_t pos = compressed_size;
             size_t src_size = block_size - compressed_size;
             size_t next_block = block_size;

             for (;next_block; pos += src_size)
               {
                  uint8_t *src_ptr = src + pos;
                  size_t dec_size = block_size;
                  src_size = block_size - pos;
                  next_block = LZ4F_decompress(dctx, dst, &dec_size, src_ptr, &src_size, NULL);
                  if (LZ4F_isError(next_block))
                    ERROR("decompress: %s", LZ4F_getErrorName(status));

                  client->buf.length += dec_size;
                  void *tmp = realloc(client->buf.data, client->buf.length);
                  EINA_SAFETY_ON_NULL_RETURN(tmp);
                  client->buf.data = tmp;
                  memcpy(&client->buf.data[client->buf.length - dec_size], dst, dec_size);
               }
             offset += pos;
             free(dst);
          }

        while ((client->buf.length) && (client->buf.index <= (client->buf.length - sizeof(Header))))
          {
             memcpy(&client->header, &client->buf.data[client->buf.index], sizeof(Header));
             client->buf.index += sizeof(Header);
             if (client->compressed) client->offset = client->buf.index;
             switch (client->header.event)
               {
                  case EVENT_ERROR:
                    break;
                  case EVENT_MESSAGE:
                    event_message(client);
                    break;
                  case EVENT_BROADCAST:
                    event_broadcast(client);
                    break;
                  case EVENT_BLOCK_END:
                    event_block_end(client);
                    break;
                  case EVENT_LAST_RECORD:
                    event_last_record(client);
                    break;
                  case EVENT_EOF:
                    event_end_of_file(client);
                    break;
                  default:
                    ERROR("Broken client ???");
               }
          }

        buffer_clear(&client->zbuf);
        buffer_clear(&client->buf);

        if (client->compressed)
          break;
     }
   LZ4F_freeDecompressionContext(dctx);
}

static Eina_Bool
cb_file_modified(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   struct stat st;
   Enigmatic_Client *client = data;

   if (stat(client->filename, &st) != -1)
     client->retries = 0;
   else
     {
        client->retries++;
        if (client->retries == 10)
          {
             fprintf(stderr, "ERROR: %s (%s)\n", client->filename, strerror(errno));
             ecore_main_loop_quit();
             return 0;
          }
        return 1;
     }

   if ((st.st_size) && (st.st_size < client->file_size))
     {
        client->truncated = 1;
        client_read(client);
     }
   else if (st.st_size > client->file_size)
     {
        client_read(client);
        if (client->event_snapshot_init.callback)
          {
             client->event_snapshot_init.callback(client, &client->snapshot, client->event_snapshot_init.data);
             client->event_snapshot_init.callback = NULL;
          }
        if (client->event_snapshot.callback)
          client->event_snapshot.callback(client, &client->snapshot, client->event_snapshot.data);
     }

   return 1;
}

#if defined(__FreeBSD__) || defined(__OpenBSD__)

static void
cb_thread_fallback(void *data, Ecore_Thread *thread)
{
   Enigmatic_Client *client = data;
   while (!ecore_thread_check(thread))
     {
        cb_file_modified(client, 0, NULL);
        usleep(10000);
     }
}

#endif

void
client_event_callback_add(Enigmatic_Client *client, Enigmatic_Client_Event_Type type, Event_Callback *cb_event, void *data)
{
   switch (type)
     {
        case EVENT_CPU_ADD:
           client->event_cpu_add.callback = cb_event;
           client->event_cpu_add.data = data;
           break;
        case EVENT_CPU_DEL:
           client->event_cpu_del.callback = cb_event;
           client->event_cpu_del.data = data;
           break;
        case EVENT_BATTERY_ADD:
           client->event_battery_add.callback = cb_event;
           client->event_battery_add.data = data;
           break;
        case EVENT_BATTERY_DEL:
           client->event_battery_del.callback = cb_event;
           client->event_battery_del.data = data;
           break;
        case EVENT_POWER_SUPPLY_ADD:
           client->event_power_supply_add.callback = cb_event;
           client->event_power_supply_add.data = data;
           break;
        case EVENT_POWER_SUPPLY_DEL:
           client->event_power_supply_del.callback = cb_event;
           client->event_power_supply_del.data = data;
           break;
        case EVENT_SENSOR_ADD:
           client->event_sensor_add.callback = cb_event;
           client->event_sensor_add.data = data;
           break;
        case EVENT_SENSOR_DEL:
           client->event_sensor_del.callback = cb_event;
           client->event_sensor_del.data = data;
           break;
        case EVENT_NETWORK_IFACE_ADD:
           client->event_network_iface_add.callback = cb_event;
           client->event_network_iface_add.data = data;
           break;
        case EVENT_NETWORK_IFACE_DEL:
           client->event_network_iface_del.callback = cb_event;
           client->event_network_iface_del.data = data;
           break;
        case EVENT_FILE_SYSTEM_ADD:
           client->event_file_system_add.callback = cb_event;
           client->event_file_system_add.data = data;
           break;
        case EVENT_FILE_SYSTEM_DEL:
           client->event_file_system_del.callback = cb_event;
           client->event_file_system_del.data = data;
           break;
        case EVENT_PROCESS_ADD:
           client->event_process_add.callback = cb_event;
           client->event_process_add.data = data;
           break;
        case EVENT_PROCESS_DEL:
           client->event_process_del.callback = cb_event;
           client->event_process_del.data = data;
           break;
        case EVENT_RECORD_DELAY:
           client->event_record_delay.callback = cb_event;
           client->event_record_delay.data = data;
           break;
     }
}

void
client_monitor_add(Enigmatic_Client *client, Snapshot_Callback *cb_event_change_init, Snapshot_Callback *cb_event_change, void *data)
{
   client->follow = 1;
   client->event_snapshot_init.callback = cb_event_change_init;
   client->event_snapshot_init.data = data;
   client->event_snapshot.callback = cb_event_change;
   client->event_snapshot.data = data;
#if defined(__linux__)
   client->mon = eio_monitor_add(client->directory);
   client->handler =
      ecore_event_handler_add(EIO_MONITOR_FILE_MODIFIED, cb_file_modified, client);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
   client->thread = ecore_thread_run(cb_thread_fallback, NULL, NULL, client);
#endif
}

void
client_snapshot_callback_set(Enigmatic_Client *client, Snapshot_Callback *cb_event_change, void *data)
{
   client->event_snapshot.callback = cb_event_change;
   client->event_snapshot.data = data;
}

Enigmatic_Client *
client_add(void)
{
   Enigmatic_Client *client = calloc(1, sizeof(Enigmatic_Client));
   EINA_SAFETY_ON_NULL_RETURN_VAL(client, NULL);

   client->fd = -1;
   client->directory = enigmatic_log_directory();

   return client;
}

void
client_follow_enabled_set(Enigmatic_Client *client, Eina_Bool enabled)
{
   client->follow = enabled;
}

static uint32_t
time_to_hour(time_t t)
{
   struct tm *tm_out, tm_buf;

   tm_out = localtime_r(&t, &tm_buf);

   return tm_out->tm_hour;
}

static uint32_t
time_datestamp(time_t t)
{
   struct tm *tm_out, tm_buf;
   uint32_t date = 0;

   tm_out = localtime_r(&t, &tm_buf);

   date += tm_out->tm_mday;
   date += (1 + tm_out->tm_mon) * 100;
   date += (1900 + tm_out->tm_year) * 10000;

   return date;
}

static uint32_t
path_datestamp(const char *path)
{
   struct stat st;

   if (stat(path, &st) == -1) return 0;

   return time_datestamp(st.st_mtime);
}

static Eina_List *
client_replay_hours(Enigmatic_Client *client)
{
   int hour_last, hours;
   time_t t;
   struct tm *tm_out, tm_buf;
   Eina_Bool last_is_current = 0;
   Eina_List *files = NULL;

   if (!client->replay.enabled) return NULL;
   if (!client->replay.end_time) return NULL;

   if (client->replay.start_time >= client->replay.end_time)
     return NULL;

   t = (time_t) client->replay.start_time;

   hours = (time_t) (client->replay.end_time - t) / 3600;
   if (hours > 24) return NULL;

   t = (time_t) client->replay.end_time;
   tm_out = localtime_r(&t, &tm_buf);
   hour_last = tm_out->tm_hour;

   t = time(NULL);
   tm_out = localtime_r(&t, &tm_buf);
   if (tm_out->tm_hour == hour_last)
     last_is_current = 1;

   int today = time_datestamp(t);
   int yday = time_datestamp(t - (24 * 3600));
   int datestamp;

   for (int i = 0; i <= hours; i++)
     {
        char path[PATH_MAX];
        int hour = time_to_hour((time_t) client->replay.start_time + (3600 * i));

        if ((i) && (hour == hour_last) && (last_is_current))
          {
             char *tmp = enigmatic_log_path();
             snprintf(path, sizeof(path), "%s", tmp);
             free(tmp);
         }
        else
          {
             snprintf(path, sizeof(path), "%s/%02i.lz4", client->directory, hour);
          }

        datestamp = path_datestamp(path);
        if ((ecore_file_exists(path)) && ((datestamp == today) || (datestamp == yday)))
          files = eina_list_append(files, strdup(path));
        else fprintf(stderr, "cowardly ignoring: %s\n", path);
     }

   return files;
}

Eina_Bool
client_replay(Enigmatic_Client *client)
{
   char *path;
   Eina_List *files = client_replay_hours(client);
   if (!files) return 0;

   EINA_LIST_FREE(files, path)
     {
        client_reopen(client, path);
        client_read(client);
     }

   return 1;
}

void
client_replay_time_start_set(Enigmatic_Client *client, uint32_t secs)
{
   client->replay.enabled = 1;
   client->replay.start_time = secs;
}

void
client_replay_time_end_set(Enigmatic_Client *client, uint32_t secs)
{
   client->replay.end_time = secs;
}

