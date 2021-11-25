#include "Enigmatic_Client.h"
#include <Elementary.h>

typedef struct {
   Evas_Object *win;
   Evas_Object *tb;

   Evas_Object *used;
   Evas_Object *cached;
   Evas_Object *buffered;
   Evas_Object *shared;
   Evas_Object *swap;
   Evas_Object *video[MEM_VIDEO_CARD_MAX];

} Win_Data;

static Evas_Object *
_label_mem(Evas_Object *parent, const char *text)
{
   Evas_Object *lb = elm_label_add(parent);
   evas_object_size_hint_weight_set(lb, 0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(lb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(lb, eina_slstr_printf("%s",text));

   return lb;
}

static Evas_Object *
_progress_add(Evas_Object *parent)
{
   Evas_Object *pb = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(pb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(pb, 1.0);

   return pb;
}

const char *
size_format(unsigned long long bytes, Eina_Bool simple)
{
   unsigned long powi = 1;
   unsigned long long value;
   unsigned int precision = 2, powj = 1;
   int i = 0;
   static const char *units[8] = {
      _("B"), _("KiB"), _("MiB"), _("GiB"),
      _("TiB"), _("PiB"), _("EiB"), _("ZiB"),
   };

   value = bytes;
   while (value > 1024)
     {
       if ((value / 1024) < powi) break;
       powi *= 1024;
       ++i;
       if (i == 7) break;
     }

   if (!i) precision = 0;

   while (precision > 0)
     {
        powj *= 10;
        if ((value / powi) < powj) break;
        --precision;
     }

   if (simple)
     return eina_slstr_printf("%1.*f %c", precision, (double) value / powi, units[i][0]);

   return eina_slstr_printf("%1.*f %s", precision, (double) value / powi, units[i]);
}

static void
cb_event_change(Enigmatic_Client *client, Snapshot *s, void *data)
{
   Evas_Object *pb;
   double ratio, value;
   Meminfo *memory;
   Win_Data *pd;

   if (!client_event_is_snapshot(client)) return;

   pd = data;
   memory = &s->meminfo;

   ratio = memory->total / 100.0;

   if (client->changes & MEMORY_USED)
     {
        pb = pd->used;
        value = memory->used / ratio;
        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        size_format(memory->used, 0),
                        size_format(memory->total, 0)));
     }

   if (client->changes & MEMORY_CACHED)
     {
        pb = pd->cached;
        value = memory->cached / ratio;
        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        size_format(memory->cached, 0),
                        size_format(memory->total, 0)));
     }

   if (client->changes & MEMORY_BUFFERED)
     {
        pb = pd->buffered;
        value = memory->buffered / ratio;
        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        size_format(memory->buffered, 0),
                        size_format(memory->total, 0)));
     }

   if (client->changes & MEMORY_SHARED)
     {
        pb = pd->shared;
        value = memory->shared / ratio;
        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        size_format(memory->shared, 0),
                        size_format(memory->total, 0)));
     }

   if (client->changes & (MEMORY_SWAP_TOTAL | MEMORY_SWAP_USED))
     {
        pb = pd->swap;
        if (memory->swap_total)
          {
             elm_object_disabled_set(pb, 0);
             ratio = memory->swap_total / 100.0;
             value = memory->swap_used / ratio;
          }
        else
          {
             elm_object_disabled_set(pb, 1);
             value = 0.0;
          }

        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        size_format(memory->swap_used, 0),
                        size_format(memory->swap_total, 0)));
     }

   if (!(client->changes & (MEMORY_VIDEO_TOTAL | MEMORY_VIDEO_USED))) return;

   for (int i = 0; i < memory->video_count; i++)
     {
        pb = pd->video[i];
        if (!pb) break;
        if (memory->video[i].total) elm_object_disabled_set(pb, 0);
        else elm_object_disabled_set(pb, 1);
        ratio = memory->video[i].total / 100.0;
        value = memory->video[i].used / ratio;
        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        size_format(memory->video[i].used, 0),
                        size_format(memory->video[i].total, 0)));
     }
}

static void
cb_event_change_init(Enigmatic_Client *client, Snapshot *s, void *data)
{
   Evas_Object *tb, *lb, *pb;
   Win_Data *pd = data;

   tb = pd->tb;

   lb = _label_mem(tb, _("Used"));
   pd->used = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 1, 1, 1);
   elm_table_pack(tb, pb, 2, 1, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   lb = _label_mem(tb, _("Cached"));
   pd->cached = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 2, 1, 1);
   elm_table_pack(tb, pb, 2, 2, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   lb = _label_mem(tb, _("Buffered"));
   pd->buffered = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 3, 1, 1);
   elm_table_pack(tb, pb, 2, 3, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   lb = _label_mem(tb, _("Shared"));
   pd->shared = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 4, 1, 1);
   elm_table_pack(tb, pb, 2, 4, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   lb = _label_mem(tb, _("Swapped"));
   pd->swap = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 5, 1, 1);
   elm_table_pack(tb, pb, 2, 5, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   for (int i = 0; i < s->meminfo.video_count; i++)
     {
        lb = _label_mem(tb, _("Video"));
        pd->video[i] = pb = _progress_add(tb);
        elm_table_pack(tb, lb, 1, 6 + i, 1, 1);
        elm_table_pack(tb, pb, 2, 6 + i, 1, 1);
        evas_object_show(lb);
        evas_object_show(pb);
     }
   evas_object_resize(pd->win, ELM_SCALE_SIZE(380), ELM_SCALE_SIZE(280));
   evas_object_show(pd->win);
}

static void
cb_win_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Win_Data *pd = data;

   free(pd);
}

int
elm_main(int argc, char **argv)
{
   Evas_Object *win, *fr, *bx, *tb;

   if (!enigmatic_launch()) return 1;

   Enigmatic_Client *client = client_open();
   EINA_SAFETY_ON_NULL_RETURN_VAL(client, 1);

   Win_Data *pd = calloc(1, sizeof(Win_Data));
   EINA_SAFETY_ON_NULL_RETURN_VAL(pd, 2);

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   pd->win = win = elm_win_util_standard_add("memories", _("Memories"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(win, EVAS_HINT_FILL, EVAS_HINT_FILL);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(bx);
   elm_object_content_set(win, bx);

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(fr, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(fr, "pad_medium");
   evas_object_show(fr);

   pd->tb = tb = elm_table_add(win);
   evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_padding_set(tb, 8, 2);
   evas_object_show(tb);

   elm_object_content_set(fr, tb);
   elm_box_pack_end(bx, fr);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, cb_win_del, pd);

   client_monitor_add(client, cb_event_change_init, cb_event_change, pd);

   ecore_main_loop_begin();

   client_del(client);

   return 0;
}

ELM_MAIN();
