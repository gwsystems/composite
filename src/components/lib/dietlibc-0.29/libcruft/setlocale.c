#include <locale.h>

char *setlocale (int category, const char *locale) {
  (void)category;
  if (locale && (locale[0]!='C' || locale[1])) return 0;
  return "C";
}
