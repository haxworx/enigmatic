#include "Enigmatic_Client.h"
#include <Elementary.h>

static Evas_Object *_win, *_bx;

static Eina_List *_bars = NULL;

static void
cb_event_change(Enigmatic_Client *client, Snapshot *s, void *data)
{
   char buf[32];
   struct tm tm_out;
   time_t t = (time_t) s->time;

   if (!client_event_is_snapshot(client)) return;

   localtime_r((time_t *) &t, &tm_out);
   strftime(buf, sizeof(buf) - 1, "%Y-%m-%d %H:%M:%S", &tm_out);

   printf("%s\n", buf);

   Eina_List *l;
   Cpu_Core *core;

   int n = 0;
   EINA_LIST_FOREACH(s->cores, l, core)
     {
        printf("%s\t=> %i%% => %i => %iC\n", core->name, core->percent, core->freq, core->temp);
        Evas_Object *pb = eina_list_nth(_bars, n++);
        elm_progressbar_value_set(pb, core->percent / 100.0);
     }
}

static void
cb_cpu_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Cpu_Core *core = event->data;
   printf("add %s => %i\n", core->name, core->id);
}

static void
cb_cpu_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Cpu_Core *core = event->data;
   printf("del %s => %i\n", core->name, core->id);
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
   Evas_Object *bx;
   Eina_List *l, *cores = s->cores;

   client_event_callback_add(client, EVENT_CPU_ADD, cb_cpu_add, NULL);
   client_event_callback_add(client, EVENT_CPU_DEL, cb_cpu_del, NULL);
   client_event_callback_add(client, EVENT_RECORD_DELAY, cb_recording_delay, NULL);

   bx = _bx;

   Cpu_Core *core;
   EINA_LIST_FOREACH(cores, l, core)
     {
        Evas_Object *pb = elm_progressbar_add(_win);
        elm_progressbar_span_size_set(pb, 120);
        evas_object_show(pb);
        _bars = eina_list_append(_bars, pb);
        elm_box_pack_end(bx, pb);
     }
}

static void
cb_win_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   ecore_main_loop_quit();
   eina_list_free(_bars);
}

int
elm_main(int argc, char **argv)
{
   Evas_Object *win, *scr, *bx;
   Enigmatic_Client *client;

   if (!enigmatic_launch()) return 1;

   client = client_open();
   EINA_SAFETY_ON_NULL_RETURN_VAL(client, 1);

   _win = win = elm_win_util_standard_add("cpeew", NULL);
   evas_object_size_hint_weight_set(win, 1.0, 1.0);
   evas_object_size_hint_align_set(win, -1.0, -1.0);
   elm_win_autodel_set(win, 1);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, cb_win_del, NULL);

   scr = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scr, 1.0, 1.0);
   evas_object_size_hint_align_set(scr, -1.0, -1.0);
   evas_object_show(scr);

   _bx = bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, 1.0, 1.0);
   evas_object_size_hint_align_set(bx, -1.0, -1.0);
   evas_object_show(bx);

   elm_object_content_set(scr, bx);
   elm_object_content_set(win, scr);
   evas_object_resize(win, 160, 240);
   elm_win_center(win, 1, 1);
   evas_object_show(win);

   client_monitor_add(client, cb_event_change_init, cb_event_change, NULL);

   ecore_main_loop_begin();

   client_del(client);

   return 0;
}

ELM_MAIN();
