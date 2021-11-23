#include "Enigmatic.h"
#include "enigmatic_config.h"
#include "monitor/monitor.h"
#include "enigmatic_log.h"

static void
system_info_free(System_Info *info)
{
   eina_hash_free(info->cores);
   eina_hash_free(info->sensors);
   eina_hash_free(info->batteries);
   eina_hash_free(info->network_interfaces);
   eina_hash_free(info->file_systems);
   eina_hash_free(info->processes);
}

static Eina_Bool
cb_shutdown(void *data, int type, void *event EINA_UNUSED)
{
   Enigmatic *enigmatic = data;

   if (enigmatic->thread)
     {
        ecore_thread_cancel(enigmatic->thread);
        ecore_thread_wait(enigmatic->thread, 1.0);
     }

   ecore_thread_cancel(enigmatic->battery_thread);
   ecore_thread_cancel(enigmatic->sensors_thread);
   ecore_thread_cancel(enigmatic->power_thread);

   ecore_thread_wait(enigmatic->battery_thread, 1.0);
   ecore_thread_wait(enigmatic->sensors_thread, 1.0);
   ecore_thread_wait(enigmatic->power_thread, 1.0);

   ecore_event_handler_del(enigmatic->handler);

   ecore_main_loop_quit();

   return 0;
}

#define DEBUGTIME 1
static void
cb_system_log(void *data, Ecore_Thread *thread)
{
   System_Info *info;
   struct timespec ts;
   Enigmatic *enigmatic = data;

   enigmatic->info = info = calloc(1, sizeof(System_Info));
   EINA_SAFETY_ON_NULL_RETURN(enigmatic->info);

   while (!ecore_thread_check(thread))
     {
        clock_gettime(CLOCK_REALTIME, &ts);
        enigmatic->poll_time = ts.tv_sec;

        int64_t tdiff = ts.tv_nsec + (ts.tv_sec * 1000000000);

        if (enigmatic_log_rotate(enigmatic))
          enigmatic->broadcast = 1;

        if (enigmatic->broadcast)
          LOG_HEADER(enigmatic, EVENT_BROADCAST);

        if (enigmatic->interval == INTERVAL_NORMAL)
          monitor_cores(enigmatic, &info->cores);

        if ((enigmatic->broadcast) || (!(enigmatic->poll_count % (enigmatic->interval * 10))))
          {
             if (enigmatic->interval != INTERVAL_NORMAL)
               monitor_cores(enigmatic, &info->cores);

             monitor_memory(enigmatic, &info->meminfo);
             monitor_sensors(enigmatic, &info->sensors);
             monitor_power(enigmatic, &info->power);
             monitor_batteries(enigmatic, &info->batteries);
             monitor_network_interfaces(enigmatic, &info->network_interfaces);
             monitor_file_systems(enigmatic, &info->file_systems);
             monitor_processes(enigmatic, &info->processes);

             LOG_HEADER(enigmatic, EVENT_BLOCK_END);
          }

        // flush to disk.
        enigmatic_log_crush(enigmatic);

        enigmatic->broadcast = 0;
        if ((enigmatic->poll_count) && (!(enigmatic->poll_count % enigmatic->device_refresh_interval)))
          enigmatic->broadcast = 1;

        enigmatic->poll_count++;

        clock_gettime(CLOCK_REALTIME, &ts);
        int usecs = (((ts.tv_sec * 1000000000) + ts.tv_nsec) - tdiff) / 1000;
        if (usecs > 100000) usecs = 100000;

        usleep((1000000 / 10) - usecs);
#if DEBUGTIME
        printf("usecs is %i\n", usecs);
        clock_gettime(CLOCK_REALTIME, &ts);
        printf("want 100000 got %ld\n", (((ts.tv_sec * 1000000000) + ts.tv_nsec) - tdiff)/ 1000);
#endif
     }

   system_info_free(info);
   free(info);
}

static void
enigmatic_init(Enigmatic *enigmatic)
{
   enigmatic_log_lock(enigmatic);

   enigmatic->pid = getpid();
   // LOCK before...ok FIX pidfile create
   enigmatic_pidfile_create(enigmatic);
   enigmatic->device_refresh_interval = 900 * 10;
   enigmatic->log.hour = -1;
   enigmatic->log.rotate_every_hour = 1;
   enigmatic->log.save_history = 1;
   enigmatic->interval = INTERVAL_NORMAL;
   enigmatic->unique_ids = NULL;
   enigmatic->broadcast = 1;

   enigmatic_config_init();

   enigmatic->config = enigmatic_config_load();

   enigmatic_log_open(enigmatic);

   monitor_batteries_init();
   monitor_sensors_init();
   monitor_power_init(enigmatic);
}

static void
enigmatic_shutdown(Enigmatic *enigmatic)
{
   void *id;

   enigmatic_config_save(enigmatic->config);

   enigmatic_log_close(enigmatic);

   EINA_LIST_FREE(enigmatic->unique_ids, id)
     free(id);

   enigmatic_log_unlock(enigmatic);
   enigmatic_pidfile_delete(enigmatic);

   monitor_batteries_shutdown();
   monitor_sensors_shutdown();
   monitor_power_shutdown();

   enigmatic_config_shutdown();
}

void
usage(void)
{
   printf("%s [OPTIONS]\n"
          "Where OPTIONS can one of: \n"
          "   -s                 Stop enigmatic daemon.\n"
          "   -h | --help        This menu.\n",
          PACKAGE);
   exit(0);
}

int main(int argc, char **argv)
{
   Eina_Bool shutdown = EINA_FALSE;

   for (int i = 1; i < argc; i++)
     {
        if ((!strcasecmp(argv[i], "-h")) || (!strcasecmp(argv[i], "--help")))
          usage();
        else if (!strcmp(argv[i], "-s"))
          {
             shutdown = EINA_TRUE;
             break;
          }
     }

   ecore_init();

   if (shutdown)
     enigmatic_terminate();
   else
     {
        Enigmatic *enigmatic = calloc(1, sizeof(Enigmatic));
        EINA_SAFETY_ON_NULL_RETURN_VAL(enigmatic, 1);

        enigmatic_init(enigmatic);

        enigmatic->handler = ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, cb_shutdown, enigmatic);

        enigmatic->thread = ecore_thread_run(cb_system_log,
                                             NULL,
                                             NULL,
                                             enigmatic);
        ecore_main_loop_begin();

        enigmatic_shutdown(enigmatic);

        free(enigmatic);
     }

   ecore_shutdown();

   return 0;
}
