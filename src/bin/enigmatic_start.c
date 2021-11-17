#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include "enigmatic_util.h"

static void
launch(void)
{
   char *paths = getenv("PATH");
   char path[PATH_MAX];
   char *t;
   struct stat st;
   int ok = 0;

   if (!paths) return;

   t = strtok(paths, ":");
   while (t)
     {
        snprintf(path, sizeof(path), "%s/%s", t, "enigmatic");
        if ((stat(path, &st)) == 0)
          {
             ok = 1;
             break;
          }
        t = strtok(NULL, ":");
     }
   if (ok)
     {
        char *args[2];
        args[0] = path;
        args[1] = NULL;
        execv(args[0], args);
     }
}

int
main(int argc, char **argv)
{
   pid_t pid;
   int ret = 1;

   signal(SIGCHLD, SIG_IGN);

   pid = fork();
   if (pid == -1)
     {
        fprintf(stderr, "fork()\n");
        exit(1);
     }
   else if (pid == 0)
     {
        close(fileno(stdin));
        close(fileno(stdout));
        close(fileno(stderr));
        launch();
     }
   else
     {
        struct stat st;
        char *path = enigmatic_log_path();
        // We have a log?
        for (int i = 0; i < 20; i++)
          {
             if (stat(path, &st) != -1)
               {
                  ret = 0;
                  break;
               }
             usleep(1000000 / 20);
          }
        free(path);
     }
   return ret;
}
