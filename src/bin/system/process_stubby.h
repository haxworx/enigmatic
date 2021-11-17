#ifndef __PROC_STUBBY_H__
#define __PROC_STUBBY_H__

#include "process.h"

typedef struct _Proc_Stubby
{
   pid_t    pid;
   uid_t    uid;
   int8_t   priority;
   int      numthreads;
   uint64_t mem_size;
   int16_t  cpu_usage;
   char    *command;
   char    *path;
} Proc_Stubby;

Proc_Stubby *
proc_stubby(Proc_Info *proc);

void
proc_stubby_free(Proc_Stubby *stubby);

#endif
