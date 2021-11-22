#include "colors.h"
#include <Eina.h>

// config for colors/sizing
#define COLOR_CPU_NUM 5
static const Color_Point cpu_colormap_in[] = {
   {   0, 0xff202020 },
   {  25, 0xff2030a0 },
   {  50, 0xffa040a0 },
   {  75, 0xffff9040 },
   { 100, 0xffffffff },
   { 256, 0xffffffff }
};
#define COLOR_FREQ_NUM 4
static const Color_Point freq_colormap_in[] = {
   {   0, 0xff202020 },
   {  33, 0xff285020 },
   {  67, 0xff30a060 },
   { 100, 0xffa0ff80 },
   { 256, 0xffa0ff80 }
};

#define COLOR_TEMP_NUM 5
static const Color_Point temp_colormap_in[] = {
   {  0,  0xff57bb8a },
   {  25, 0xffa4c073 },
   {  50, 0xfff5ce62 },
   {  75, 0xffe9a268 },
   { 100, 0xffdd776e },
   { 256, 0xffdd776e }
};

unsigned int cpu_colormap[256];
unsigned int freq_colormap[256];
unsigned int temp_colormap[256];

static void
_color_init(const Color_Point *col_in, unsigned int n, unsigned int *col)
{
   unsigned int pos, interp, val, dist, d;
   unsigned int a, r, g, b;
   unsigned int a1, r1, g1, b1, v1;
   unsigned int a2, r2, g2, b2, v2;

   // wal colormap_in until colormap table is full
   for (pos = 0, val = 0; pos < n; pos++)
     {
        //
        // get first color and value position
        v1 = col_in[pos].val;
        a1 = AVAL(col_in[pos].color);
        r1 = RVAL(col_in[pos].color);
        g1 = GVAL(col_in[pos].color);
        b1 = BVAL(col_in[pos].color);
        // get second color and valuje position
        v2 = col_in[pos + 1].val;
        a2 = AVAL(col_in[pos + 1].color);
        r2 = RVAL(col_in[pos + 1].color);
        g2 = GVAL(col_in[pos + 1].color);
        b2 = BVAL(col_in[pos + 1].color);
        // get distance between values (how many entires to fill)
        dist = v2 - v1;
        // walk over the span of colors from point a to point b
        for (interp = v1; interp < v2; interp++)
          {
             // distance from starting point
             d = interp - v1;
             // calculate linear interpolation between start and given d
             a = ((d * a2) + ((dist - d) * a1)) / dist;
             r = ((d * r2) + ((dist - d) * r1)) / dist;
             g = ((d * g2) + ((dist - d) * g1)) / dist;
             b = ((d * b2) + ((dist - d) * b1)) / dist;
             // write out resulting color value
             col[val] = ARGB(a, r, g, b);
             val++;
          }
     }
}

void
colors_init(void)
{
   static Eina_Bool init = 0;

   if (!init)
     {
        _color_init(cpu_colormap_in, COLOR_CPU_NUM, cpu_colormap);
        _color_init(freq_colormap_in, COLOR_FREQ_NUM, freq_colormap);
        _color_init(temp_colormap_in, COLOR_TEMP_NUM, temp_colormap);
        init = 1;
     }
}
