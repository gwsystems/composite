#include <string.h>

static const unsigned long long magic = 0x0101010101010101LL;

size_t strlen(const char *s)
{
  const char *t = s;
  unsigned long long word;

  if (!s) return 0;

  /* Byte compare up until 64 bit word boundary */
  for (; ((unsigned long long) t & 7); t++)
    if (!*t) return t - s;

  /* Word compare */
  do {
    word = *((unsigned long long *) t); t += 8;
    word = (word - magic) &~ word;
    word &= (magic << 7);
  } while (word == 0);

  /* word & 0x8080808080808080 == word */
  word = (word - 1) & (magic << 8);
  word += (word << 32);
  word += (word << 16);
  word += (word << 8);
  t += word >> 56;
  return ((const char *) t) - 8 - s;
}


