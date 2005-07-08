
#include "sku.h"

int count_bits(unsigned int a)/*{{{*/
{
  a = (a & 0x55555555) + ((a>>1) & 0x55555555);
  a = (a & 0x33333333) + ((a>>2) & 0x33333333);
  a = (a & 0x0f0f0f0f) + ((a>>4) & 0x0f0f0f0f);
  a = (a & 0x00ff00ff) + ((a>>8) & 0x00ff00ff);
  a = (a & 0x0000ffff) + ((a>>16) & 0x0000ffff);
  return a;
}
/*}}}*/
int decode(unsigned int a)/*{{{*/
{
  /* This needs optimising?? */

  int r, m;
  m = 1;
  r = 0;
  if (!a) return -1;
  while (1) {
    if (a & m) return r;
    r++;
    m <<= 1;
  }
}
/*}}}*/
char *tobin(int n, int x)/*{{{*/
{
  int i;
  int mask;
  static char buffer[64];
  for (i=0; i<n; i++) {
    mask = 1<<i;
    buffer[i] = (x & mask) ? '1' : '0';
  }
  buffer[n] = 0;
  return buffer;
}
/*}}}*/
void show_symbols_in_set(int ns, const char *symbols, int bitmap)/*{{{*/
{
  int i, mask;
  int first = 1;
  for (i=0; i<ns; i++) {
    mask = 1<<i;
    if (bitmap & mask) {
      if (!first) fprintf(stderr, ",");
      first = 0;
      fprintf(stderr, "%c", symbols[i]);
    }
  }
}
/*}}}*/
