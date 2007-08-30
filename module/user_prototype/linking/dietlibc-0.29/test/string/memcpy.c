#include <string.h>
#include <assert.h>

int main() {
  char test[100]="blubber";
  assert(memcpy(test,"blubber",8)==test);
  assert(!memcmp(test,"blubber",8));
  assert(memcpy(0,0,0)==0);
  assert(memcpy(test,"foobar",3) && test[2]=='o');
  return 0;
}
