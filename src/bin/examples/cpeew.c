#include "Enigmatic_Client.h"
#include "colors.h"
#include <Elementary.h>

static Evas_Object *_table = NULL;
static Eina_List *_objects = NULL;

static void
cb_event_change(Enigmatic_Client *client, Snapshot *s, void *data)
{
   Eina_List *l;
   Cpu_Core *core;
   Eina_Bool have_freq = cores_frequency();

   if (!(client->changes & CPU_CORE)) return;
   int n = 0;
   EINA_LIST_FOREACH(s->cores, l, core)
     {
        Evas_Object *lb = eina_list_nth(_objects, n++);
        Evas_Object *rec = evas_object_data_get(lb, "r2");
        int c = cpu_colormap[core->percent & 0xff];
        evas_object_color_set(rec, RVAL(c), GVAL(c), BVAL(c), AVAL(c));

        if (have_freq)
          {
             c = freq_colormap[core->freq & 0xff];
             rec = evas_object_data_get(lb, "r1");
             evas_object_color_set(rec, RVAL(c), GVAL(c), BVAL(c), AVAL(c));
          }

        elm_object_text_set(lb, eina_slstr_printf("%i%%", core->percent));
     }
}

static int
_row(int n)
{
   double i, f, value = sqrt(n);
   f = modf(value, &i);
   if (EINA_DBL_EQ(f, 0.0)) return value;
   return value + 1;
}

static void
cb_event_change_init(Enigmatic_Client *client, Snapshot *s, void *data)
{
   Evas_Object *tb, *rec, *lb;
   Eina_List *l, *cores = s->cores;

   tb = _table;

   int i = 0, row = 0, col = 0, w = _row(eina_list_count(s->cores));

   Cpu_Core *core;
   EINA_LIST_FOREACH(cores, l, core)
     {
        if (!(i % w))
          {
             row++;
             col = 0;
          }
        else col++;

        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_size_hint_min_set(rec, 86, 86);
        elm_table_pack(tb, rec, col, row, 1, 1);
        evas_object_show(rec);

        lb = elm_label_add(tb);
        evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(lb, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_data_set(lb, "r1", rec);

        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_size_hint_min_set(rec, 84, 84);
        elm_table_pack(tb, rec, col, row, 1, 1);
        evas_object_show(rec);
        evas_object_data_set(lb, "r2", rec);

        elm_object_text_set(lb, eina_slstr_printf("%i%%", core->percent));
        elm_table_pack(tb, lb, col, row, 1, 1);
        evas_object_show(lb);

        _objects = eina_list_append(_objects, lb);
        i++;
     }
}

static void
cb_win_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   ecore_main_loop_quit();
   eina_list_free(_objects);
}

int
elm_main(int argc, char **argv)
{
   Evas_Object *win, *bx, *tb;
   Enigmatic_Client *client;

   if (!enigmatic_launch()) return 1;

   colors_init();

   client = client_open();
   EINA_SAFETY_ON_NULL_RETURN_VAL(client, 1);

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   win = elm_win_util_standard_add("cpeew", "cpeew");
   evas_object_size_hint_weight_set(win, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(win, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_win_autodel_set(win, 1);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, cb_win_del, NULL);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(bx, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(bx);

   _table = tb = elm_table_add(win);
   elm_table_padding_set(tb, ELM_SCALE_SIZE(2), ELM_SCALE_SIZE(2));
   evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb, 0.5, 0.5);

   elm_box_pack_end(bx, tb);
   evas_object_show(tb);

   elm_object_content_set(win, bx);
   evas_object_resize(win, ELM_SCALE_SIZE(320), ELM_SCALE_SIZE(320));
   elm_win_center(win, 1, 1);
   evas_object_show(win);

   client_monitor_add(client, cb_event_change_init, cb_event_change, NULL);

   ecore_main_loop_begin();

   client_del(client);

   return 0;
}

ELM_MAIN();
