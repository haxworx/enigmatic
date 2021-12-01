#include "Enigmatic.h"
#include "enigmatic_util.h"
#include <Ecore_File.h>
#include <sys/types.h>
#include <signal.h>

static char *
_pidfile_path(void)
{
   char path[PATH_MAX];
   snprintf(path, sizeof(path), "%s/%s/%s", enigmatic_cache_dir_get(), PACKAGE, PID_FILE_NAME);
   return strdup(path);
}

char *
enigmatic_pidfile_path(void)
{
   return _pidfile_path();
}

// Store only (see enigmatic_log_lock).
Eina_Bool
enigmatic_pidfile_create(Enigmatic *enigmatic)
{
   char path[PATH_MAX], *tmp;
   FILE *f;

   snprintf(path, sizeof(path), "%s/%s", enigmatic_cache_dir_get(), PACKAGE);
   if (!ecore_file_exists(path))
     ecore_file_mkdir(path);

   tmp = _pidfile_path();
   f = fopen(tmp, "w");
   if (!f)
     {
        free(tmp);
        return 0;
     }

   fprintf(f, "%i", enigmatic->pid);
   fclose(f);

   enigmatic->pidfile_path = tmp;

   return 1;
}

const char *
enigmatic_cache_dir_get(void)
{
   static char dir[PATH_MAX];
   static Eina_Bool found = 0;
   char *homedir;

   if (found)
     return dir;
   else
     {
        homedir = getenv("HOME");
        snprintf(dir, sizeof(dir), "%s/.cache", homedir);
        found = 1;
     }
   return dir;
}

void
enigmatic_pidfile_delete(Enigmatic *enigmatic)
{
   ecore_file_remove(enigmatic->pidfile_path);
   free(enigmatic->pidfile_path);
}

uint32_t
enigmatic_pidfile_pid_get(const char *path)
{
   pid_t pid;
   char buf[32];
   FILE *f = fopen(path, "r");
   if (!f)
     ERROR("No PID file.");

   if (fgets(buf, sizeof(buf), f))
     pid = atoll(buf);
   else
     ERROR("No PID file value.");

   fclose(f);

   return pid;
}

void
enigmatic_terminate(void)
{
   char *path = _pidfile_path();
   pid_t pid;

   pid = enigmatic_pidfile_pid_get(path);
   kill(pid, SIGINT);

   free(path);
}

Eina_Bool
enigmatic_launch(void)
{
   return !system(PACKAGE"_start");
}

Eina_Bool
enigmatic_running(void)
{
   return system("enigmatic -p");
}

char *
enigmatic_log_path(void)
{
   char path[PATH_MAX];

   snprintf(path, sizeof(path), "%s/%s", enigmatic_cache_dir_get(), PACKAGE);
   if (!ecore_file_exists(path))
     ecore_file_mkdir(path);

   snprintf(path, sizeof(path), "%s/%s/%s", enigmatic_cache_dir_get(), PACKAGE, LOG_FILE_NAME);

   return strdup(path);
}

char *
enigmatic_log_directory(void)
{
   char path[PATH_MAX];

   snprintf(path, sizeof(path), "%s/%s", enigmatic_cache_dir_get(), PACKAGE);

   return strdup(path);
}

