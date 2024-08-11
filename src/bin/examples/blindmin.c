#include "Enigmatic_Client.h"
#include <Elementary.h>

#define NOISY 0

static Ecore_Exe *exe = NULL;

typedef struct
{
   const char *name;
   const char *command;
   const char *speech_format;
} Speech_Backend;

static Speech_Backend backends[] = {
   { .name  = "Festival", .command = "festival -i --pipe", .speech_format = "(SayText \"%s\")\n" },
};

static Speech_Backend *speech_backend = NULL;

static void
send_speech(const char *fmt, ...)
{
   char buf[4096];
   char *text;
   va_list ap;
   size_t len;

   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);

   len = strlen(buf) + strlen(speech_backend->speech_format) + 1;
   text = malloc(len);
   if (text)
     {
        snprintf(text, len, speech_backend->speech_format, buf);
        ecore_exe_send(exe, text, len);
        free(text);
     }
}

static void
cb_event_change(Enigmatic_Client *client, Snapshot *s, void *data)
{
   Eina_List *l;
   Proc_Stubby *proc;

   int cpu_count = eina_list_count(s->cores);

   int hot_ones = 0;
   EINA_LIST_FOREACH(s->processes, l, proc)
     {
        if (proc->cpu_usage > 90)
          hot_ones++;
     }

   if (hot_ones > cpu_count / 2) send_speech("hot potatoes");
}

static void
cb_cpu_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Cpu_Core *core = event->data;
   send_speech("add %s => %i\n", core->name, core->id);
}

static void
cb_cpu_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Cpu_Core *core = event->data;
   send_speech("del %s => %i\n", core->name, core->id);
}

static void
cb_battery_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Battery *bat = event->data;
   send_speech("add %s\n", bat->name);
}

static void
cb_battery_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Battery *bat = event->data;
   send_speech("del %s\n", bat->name);
}

static void
cb_power_supply_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Eina_Bool *power = event->data;
   send_speech("power supply add %i\n", *power);
}

static void
cb_power_supply_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Eina_Bool *power = event->data;
   send_speech("power supply gone %i\n", *power);
}

static void
cb_sensor_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Sensor *sensor = event->data;
   send_speech("add %s.%s\n", sensor->name, sensor->child_name);
}

static void
cb_sensor_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Sensor *sensor = event->data;
   send_speech("del %s.%s\n", sensor->name, sensor->child_name);
}

static void
cb_network_iface_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Network_Interface *iface = event->data;
   send_speech("bringing it up %s\n", iface->name);
}

static void
cb_network_iface_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Network_Interface *iface = event->data;
   send_speech("bringing it down %s\n", iface->name);
}

static void
cb_file_system_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   File_System *fs = event->data;
   send_speech("more space %s at %s\n", fs->mount, fs->path);
}

static void
cb_file_system_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   File_System *fs = event->data;
   send_speech("less space %s at %s\n", fs->mount, fs->path);
}

static void
cb_process_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   //Proc_Stubby *proc = event->data;
   //send_speech("add %i => %s\n", proc->pid, proc->command);
}

static void
cb_process_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   //Proc_Stubby *proc = event->data;
//   send_speech("del %i => %s\n", proc->pid, proc->command);
}

static void
cb_recording_delay(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   int *delay = event->data;
   printf("gap %i => total %i secs\n", event->time, *delay);
}

static void
cb_event_change_init(Enigmatic_Client *client, Snapshot *s, void *data)
{
   enigmatic_client_event_callback_add(client, EVENT_CPU_ADD, cb_cpu_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_CPU_DEL, cb_cpu_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_BATTERY_ADD, cb_battery_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_BATTERY_DEL, cb_battery_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_POWER_SUPPLY_ADD, cb_power_supply_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_POWER_SUPPLY_DEL, cb_power_supply_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_SENSOR_ADD, cb_sensor_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_SENSOR_DEL, cb_sensor_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_NETWORK_IFACE_ADD, cb_network_iface_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_NETWORK_IFACE_DEL, cb_network_iface_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_FILE_SYSTEM_ADD, cb_file_system_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_FILE_SYSTEM_DEL, cb_file_system_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_PROCESS_ADD, cb_process_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_PROCESS_DEL, cb_process_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_RECORD_DELAY, cb_recording_delay, NULL);
}

static void
monitor(void)
{
   if (!enigmatic_launch()) return;

   Enigmatic_Client *client = enigmatic_client_open();
   EINA_SAFETY_ON_NULL_RETURN(client);

   enigmatic_client_monitor_add(client, cb_event_change_init, cb_event_change, NULL);

   ecore_main_loop_begin();

   enigmatic_terminate();

   enigmatic_client_del(client);
}

static Eina_Bool
stdout_handler(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Data *ev;

   ev = event;

   if (!ev || !ev->size) return 1;
   if (ev->exe != exe) return 1;

   for (int i = 0; ev->lines[i].line != NULL; i++)
     {
        Ecore_Exe_Event_Data_Line *line = &ev->lines[i];
        printf("%s\n", line->line);
     }
   return 1;
}

int
elm_main(int argc, char **argv)
{
   speech_backend = &backends[0];
   printf("Using %s\n", speech_backend->name);

   exe = ecore_exe_pipe_run(speech_backend->command, ECORE_EXE_PIPE_WRITE | ECORE_EXE_PIPE_READ | ECORE_EXE_PIPE_READ_LINE_BUFFERED |
                            ECORE_EXE_PIPE_ERROR | ECORE_EXE_PIPE_ERROR_LINE_BUFFERED, NULL);

   ecore_event_handler_add(ECORE_EXE_EVENT_DATA, stdout_handler, NULL);
   ecore_event_handler_add(ECORE_EXE_EVENT_ERROR, stdout_handler, NULL);

   monitor();

   return 0;
}

ELM_MAIN();
