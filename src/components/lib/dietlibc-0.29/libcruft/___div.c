#include "dietwarning.h"
#include <stdlib.h>

div_t div(int numer, int denom) {
  div_t temp;
  temp.quot=numer/denom;
  temp.rem=numer%denom;
  return temp;
}

link_warning("div","warning: your code uses div(), which is completely superfluous!");
