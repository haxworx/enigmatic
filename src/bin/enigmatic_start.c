#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include "enigmatic_util.h"

// Having clients launch the daemon on-demand isn't really ideal.
// For the sake of testing's sake, launch and then see if we have
// a pidfile after a second.
//
// Spinning up enigmatic daemon and then shutting it down, takes
// some time (not much but some). It should really "stick" around.
//
// There's also a race between a shutdown (enigmatic -s) and a
// subsequent launch where the daemon could be shutting down and
// this little helper thinks it has successfully launched.

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
   int running = 0;

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
        char *path = enigmatic_pidfile_path();
        for (int i = 0; i < 20; i++)
          {
             if (stat(path, &st) != -1)
               running = 1;
             else running = 0;

             usleep(1000000 / 20);
          }
        free(path);
     }

   return !running;
}
