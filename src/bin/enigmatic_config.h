#ifndef ENIGMATIC_CONFIG_H
#define ENIGMATIC_CONFIG_H

#define ENIGMATIC_CONFIG_VERSION 0x0001

typedef struct _Enigmatic_Config
{
   int version;
} Enigmatic_Config;

void
enigmatic_config_init(void);

void
enigmatic_config_shutdown(void);

Enigmatic_Config *
enigmatic_config_load(void);

Eina_Bool
enigmatic_config_save(Enigmatic_Config *config);

#endif
