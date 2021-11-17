#include "process_stubby.h"
#include <stdlib.h>
#include <ctype.h>

Proc_Stubby *
proc_stubby(Proc_Info *proc)
{
   Proc_Stubby *stubby = malloc(sizeof(Proc_Stubby));
   EINA_SAFETY_ON_NULL_RETURN_VAL(stubby, NULL);

   stubby->pid = proc->pid;
   stubby->uid = proc->uid;
   stubby->command = strdup(proc->command);
   stubby->numthreads = proc->numthreads;
   stubby->priority = proc->priority;
   stubby->mem_size = proc->mem_size;

   if (!proc->arguments)
     stubby->path = strdup(proc->command);
   else
     {
        char *cp = proc->arguments;
        while (*cp)
          {
             if (isspace(*cp))
               {
                  cp--;
                  break;
               }
             cp++;
          }
        stubby->path = strndup(proc->arguments, (1 + cp - proc->arguments));
     }

   stubby->cpu_usage = 0;

   return stubby;
}

void
proc_stubby_free(Proc_Stubby *stubby)
{
   if (stubby->command)
     free(stubby->command);
   if (stubby->path)
     free(stubby->path);
   free(stubby);
}

