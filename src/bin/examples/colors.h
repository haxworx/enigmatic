#ifndef COLORS_H
#define COLORS_H

typedef struct _Color_Point {
   unsigned int val;
   unsigned int color;
} Color_Point;

extern unsigned int cpu_colormap[256];
extern unsigned int freq_colormap[256];
extern unsigned int temp_colormap[256];

void
colors_init(void);

#define AVAL(x) (((x) >> 24) & 0xff)
#define RVAL(x) (((x) >> 16) & 0xff)
#define GVAL(x) (((x) >>  8) & 0xff)
#define BVAL(x) (((x)      ) & 0xff)
#define ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

#endif
