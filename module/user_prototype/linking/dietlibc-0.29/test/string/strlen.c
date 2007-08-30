#include <assert.h>
#include <string.h>

int main() {
  const char* x="foo bar baz";
  assert(strlen("")==0);
  assert(strlen(x)==11);
  assert(strlen(x+5)==6);
  return 0;
}
