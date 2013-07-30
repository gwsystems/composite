#include <limits.h>
#include <assert.h>

/*

code comes from http://www.cs.tut.fi/~jkorpela/round.html

*/
long
round(double x)
{
      assert(x >= LONG_MIN-0.5);
      assert(x <= LONG_MAX+0.5);
      if (x >= 0)
         return (long) (x+0.5);
      return (long) (x-0.5);
}
