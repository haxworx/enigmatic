#include "system/machine.h"
#include "power.h"
#include "enigmatic_log.h"

static void
power_refresh(Enigmatic *enigmatic, Eina_Bool *ac)
{
   Message msg;
   msg.type = MESG_REFRESH;
   msg.object_type = POWER;
   msg.number = 1;
   enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, ac, sizeof(Eina_Bool));
}

Eina_Bool
monitor_power(Enigmatic *enigmatic, Eina_Bool *ac_prev)
{
   Eina_Bool ac = power_ac_present();

   if (enigmatic->broadcast)
     {
        power_refresh(enigmatic, &ac);
        *ac_prev = ac;
        return 1;
     }
   if (ac != *ac_prev)
     {
        DEBUG("power %i", ac);
        Message msg;
        msg.type = MESG_MOD;
        msg.object_type = POWER_VALUE;
        Change change = CHANGE_I8;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
        enigmatic_log_write(enigmatic, (char *) &change, sizeof(Change));
        enigmatic_log_write(enigmatic, (char *) &ac, 1);
        *ac_prev = ac;
        return 1;
     }
   return 0;
}

