#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>

#define EXPERIMENTING 1

#define HEADER_MAGIC 0xf00dcafe

typedef enum
{
   EVENT_ERROR       = 0,
   EVENT_MESSAGE     = 1,
   EVENT_BROADCAST   = 2,
   EVENT_BLOCK_END   = 3,
   EVENT_LAST_RECORD = 4,
   EVENT_EOF         = 5,
} Event;

typedef enum
{
   CPU_CORE           = 1,
   CPU_CORE_PERC      = 2,
   CPU_CORE_TEMP      = 3,
   CPU_CORE_FREQ      = 4,
   MEMORY             = 5,
   MEMORY_TOTAL       = 6,
   MEMORY_USED        = 7,
   MEMORY_CACHED      = 8,
   MEMORY_BUFFERED    = 9,
   MEMORY_SHARED      = 10,
   MEMORY_SWAP_TOTAL  = 11,
   MEMORY_SWAP_USED   = 12,
   MEMORY_VIDEO_TOTAL = 13,
   MEMORY_VIDEO_USED  = 14,
   SENSOR             = 15,
   SENSOR_VALUE       = 16,
   POWER              = 17,
   POWER_VALUE        = 18,
   BATTERY            = 19,
   BATTERY_FULL       = 20,
   BATTERY_CURRENT    = 21,
   BATTERY_PERCENT    = 22,
   NETWORK            = 23,
   NETWORK_INCOMING   = 24,
   NETWORK_OUTGOING   = 25,
   FILE_SYSTEM        = 26,
   FILE_SYSTEM_TOTAL  = 27,
   FILE_SYSTEM_USED   = 28,
   PROCESS            = 29,
   PROCESS_MEM_SIZE   = 30,
   PROCESS_CPU_USAGE  = 31,
   PROCESS_NUM_THREAD = 32,
   PROCESS_PRIORITY   = 33,
} Object_Type;

typedef enum
{
   INTERVAL_NORMAL = 1,
   INTERVAL_MEDIUM = 3,
   INTERVAL_SLOW   = 5,
} Interval;

typedef enum
{
   CHANGE_FLOAT  = 1,
   CHANGE_I8     = 2,
   CHANGE_I16    = 3,
   CHANGE_I32    = 4,
   CHANGE_I64    = 5,
   CHANGE_I128   = 6,
   CHANGE_STRING = 7
} Change;

typedef enum
{
   MESG_ERROR     = 1,
   MESG_REFRESH   = 2,
   MESG_ADD       = 3,
   MESG_MOD       = 4,
   MESG_DEL       = 5,
} Message_Type;

typedef struct
{
   Message_Type  type;
   Object_Type   object_type;
   unsigned int  number;
} Message;

typedef struct
{
   Event        event;
   uint32_t     time;
} Header;

#endif
