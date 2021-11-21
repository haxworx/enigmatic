#include <Elementary.h>
#include "Enigmatic.h"
#include "enigmatic_log.h"

static Eina_Bool
test_log_compress(void)
{
   char buf[PATH_MAX], buf2[PATH_MAX];
   char *tok, *paths = getenv("PATH");
   const char *path = NULL;
   uint32_t sz = 0;

   tok = strtok(paths, ":");
   while (tok)
     {
        snprintf(buf, sizeof(buf), "%s/enlightenment", tok);
        if (ecore_file_exists(buf))
          {
             path = buf;
             break;    
          }
        tok = strtok(NULL, ":");
     }
   
    if (!path)
      {
         fprintf(stderr, "enlightenment could not be found.\n");
         return EINA_FALSE;
      } 

    getcwd(buf2, PATH_MAX);
    Eina_Slstr *tmp = eina_slstr_printf("%s/enlightenment", buf2);
    ecore_file_cp(path, tmp);

    enigmatic_log_compress(tmp, EINA_FALSE);
    ecore_file_remove(tmp);

    char *b = enigmatic_log_decompress(eina_slstr_printf("%s.lz4", tmp), &sz);
    EINA_SAFETY_ON_NULL_RETURN_VAL(b, EINA_FALSE);

    FILE *f = fopen("enlightenment", "wb");
    if (f)
      {
         fwrite(b, sz, 1, f);
         fclose(f);
      }
    free(b);

    return system(eina_slstr_printf("/bin/diff %s %s", path, "enlightenment"));
}

int elm_main(int argc, char **argv)
{
    printf("test_log_compress => %s \n", test_log_compress() == 0 ? "OK" : "FAIL" );

    return 0;
}

ELM_MAIN();
