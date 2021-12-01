#include "system/process_stubby.h"
#include "uid.h"
#include "enigmatic_log.h"
#include <ctype.h>

static void
cb_process_free(void *data)
{
   Proc_Info *proc = data;

   DEBUG("del pid => %i => %s", proc->pid, proc->command);

   proc_info_free(proc);
}

static int
cb_process_cmp(const void *a, const void *b)
{
   Proc_Info *p1, *p2;

   p1 = (Proc_Info *) a;
   p2 = (Proc_Info *) b;

   return p1->pid - p2->pid;
}

void
enigmatic_log_stubby_list_write(Enigmatic *enigmatic, Eina_List *list)
{
   Proc_Stubby *stubby;
   int n;
   ssize_t len;
   Header *hdr;
   char *buf;

   n = eina_list_count(list);
   if (!n) return;

   len = sizeof(Header) + sizeof(Message);

   buf = malloc(len);
   EINA_SAFETY_ON_NULL_RETURN(buf);
   hdr = (Header *) &buf[0];
   hdr->time = enigmatic->poll_time;
   hdr->event = EVENT_MESSAGE;

   Message *msg = (Message *) &buf[sizeof(Header)];
   msg->type = MESG_REFRESH;
   msg->object_type = PROCESS;
   msg->number = n;

   enigmatic_log_write(enigmatic, buf, len);

   char *command, *path;
   EINA_LIST_FREE(list, stubby)
     {
        command = stubby->command; stubby->command = NULL;
        path = stubby->path; stubby->path = NULL;
        enigmatic_log_write(enigmatic, (char *) stubby, sizeof(Proc_Stubby));
        enigmatic_log_write(enigmatic, command, strlen(command) + 1);
        enigmatic_log_write(enigmatic, path, strlen(path) + 1);
        free(command);
        free(path);
        free(stubby);
     }
   free(buf);
}

static void
enigmatic_log_stubby_write(Enigmatic *enigmatic, Proc_Stubby *stubby)
{
   Header *hdr;
   char *buf;
   ssize_t len;

   len = sizeof(Header) + sizeof(Message);

   buf = malloc(len);
   EINA_SAFETY_ON_NULL_RETURN(buf);
   hdr = (Header *) &buf[0];
   hdr->time = enigmatic->poll_time;
   hdr->event = EVENT_MESSAGE;

   Message *msg = (Message *) &buf[sizeof(Header)];
   msg->type = MESG_ADD;
   msg->object_type = PROCESS;
   msg->number = 1;

   enigmatic_log_write(enigmatic, buf, len);

   char *command, *path;
   command = stubby->command;
   stubby->command = NULL;
   path = stubby->path;
   stubby->path = NULL;

   enigmatic_log_write(enigmatic, (char *) stubby, sizeof(Proc_Stubby));
   enigmatic_log_write(enigmatic, (char *) command, strlen(command) + 1);
   enigmatic_log_write(enigmatic, (char *) path, strlen(path) + 1);
   free(path);
   free(command);
   free(buf);
}

static void
processes_refresh(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *ordered = NULL, *stubbies = NULL;
   void *d = NULL;
   Proc_Info *proc;
   Proc_Stubby *stubby;
   int n;
   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);

   while (eina_iterator_next(it, &d))
     {
        proc = d;
        ordered = eina_list_append(ordered, proc);
     }
   eina_iterator_free(it);

   n = eina_list_count(ordered);
   if (!n) return;

   ordered = eina_list_sort(ordered, n, cb_process_cmp);
   EINA_LIST_FREE(ordered, proc)
     {
        stubby = proc_stubby(proc);
        if (stubby)
          stubbies = eina_list_append(stubbies, stubby);
     }
   enigmatic_log_stubby_list_write(enigmatic, stubbies);
}

Eina_Bool
enigmatic_monitor_processes(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *l, *processes;
   Proc_Info *proc, *p1;
   Eina_Bool changed = 0;

   processes = proc_info_all_get();
   if (!*cache_hash)
     {
        *cache_hash = eina_hash_int32_new(cb_process_free);
        EINA_LIST_FOREACH(processes, l, proc)
          {
             DEBUG("add pid => %i => %s", proc->pid, proc->command);
             proc->is_new = 1;
             int32_t pid = proc->pid;
             eina_hash_add(*cache_hash, &pid, proc);
          }
     }

   if (enigmatic->broadcast)
     {
        processes_refresh(enigmatic, cache_hash);
     }

   void *d = NULL;
   Eina_List *purge = NULL;

   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);
   while (eina_iterator_next(it, &d))
     {
        Proc_Info *p2 = d;
        Eina_Bool found = 0;
        EINA_LIST_FOREACH(processes, l, p1)
          {
             if ((p1->pid == p2->pid) && (p1->start_time == p2->start_time))
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          purge = eina_list_prepend(purge, p2);
     }
   eina_iterator_free(it);

   EINA_LIST_FREE(purge, proc)
     {
        Message msg;
        msg.type = MESG_DEL;
        msg.object_type = PROCESS;
        msg.number = proc->pid;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);

        int32_t pid = proc->pid;
        eina_hash_del(*cache_hash, &pid, NULL);
     }

   EINA_LIST_FREE(processes, proc)
     {
        int32_t pid = proc->pid;
        p1 = eina_hash_find(*cache_hash, &pid);
        if (!p1)
          {
             Proc_Stubby *stubby = proc_stubby(proc);
             enigmatic_log_stubby_write(enigmatic, stubby);
             free(stubby);

             DEBUG("add pid => %i => %s", proc->pid, proc->command);
             int32_t pid = proc->pid;
             eina_hash_add(*cache_hash, &pid, proc);
             continue;
          }

        Message msg;
        msg.type = MESG_MOD;

        if (p1->mem_size != proc->mem_size)
          {
             msg.object_type = PROCESS_MEM_SIZE;
             msg.number = p1->pid;

             enigmatic_log_diff(enigmatic, msg, (proc->mem_size / 4096) - (p1->mem_size / 4096));
             DEBUG("mem pid => %i => %s => %i", proc->pid, proc->command, (int) (proc->mem_size / 4096)  - (int) (p1->mem_size / 4096));
             changed = 1;
          }

        if ((p1->cpu_time != proc->cpu_time) || ((p1->cpu_time == proc->cpu_time) && !p1->was_zero))
          {
             msg.object_type = PROCESS_CPU_USAGE;
             msg.number = p1->pid;
             // XXX: value
             int val = (proc->cpu_time - p1->cpu_time) / enigmatic->interval;
             if (!val) p1->was_zero = 1;
             else p1->was_zero = 0;
             enigmatic_log_diff(enigmatic, msg, val);
             DEBUG("cpu pid => %i => %s => %i%%", proc->pid, proc->command, val);
             // Keep this in memory.
             p1->cpu_usage = (proc->cpu_time - p1->cpu_time) / enigmatic->interval;
             changed = 1;
          }

        if (p1->numthreads != proc->numthreads)
          {
             msg.object_type = PROCESS_NUM_THREAD;
             msg.number = p1->pid;
             // XXX: value
             enigmatic_log_diff(enigmatic, msg, proc->numthreads);
             DEBUG("threads pid => %i => %s => %i", proc->pid, proc->command, proc->numthreads);
             changed = 1;
          }

        if (p1->priority != proc->priority)
          {
             msg.object_type = PROCESS_PRIORITY;
             msg.number = p1->pid;
             // XXX: value
             enigmatic_log_diff(enigmatic, msg, proc->priority);
             DEBUG("priority pid => %i => %s => %i", proc->pid, proc->command, proc->priority);
             changed = 1;
          }

        if (!proc->is_new)
          {
             Proc_Info *prev = eina_hash_modify(*cache_hash, &pid, proc);
             if (prev)
               proc_info_free(prev);
          }
     }

   return changed;
}

