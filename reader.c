#include "sku.h"

static void chomp(char *x)/*{{{*/
{
  int len;
  len = strlen(x);
  if ((len > 0) && (x[len-1] == '\n')) {
    x[len-1] = '\0';
  }
}
/*}}}*/
void read_grid(struct layout **lay, int **state)/*{{{*/
{
  int rmap[256];
  int valid[256];
  int i, c;
  char buffer[256];
  struct layout *my_lay;

  fgets(buffer, sizeof(buffer), stdin);
  chomp(buffer);
  if (strncmp(buffer, "#layout: ", 9)) {
    fprintf(stderr, "Input does not start with '#layout: ', giving up.\n");
    exit(1);
  }

  my_lay = genlayout(buffer + 9);
  *state = new_array(int, my_lay->nc);
  

  for (i=0; i<256; i++) {
    rmap[i] = -1;
    valid[i] = 0;
  }
  rmap['*'] = -2;
  for (i=0; i<my_lay->ns; i++) {
    rmap[my_lay->symbols[i]] = i;
    valid[my_lay->symbols[i]] = 1;
  }
  valid['.'] = 1;
  valid['*'] = 1;

  for (i=0; i<my_lay->nc; i++) {
    do {
      c = getchar();
      if (c == EOF) {
        fprintf(stderr, "Ran out of input data!\n");
        exit(1);
      }
      if (valid[c]) {
        (*state)[i] = rmap[c];
      }
    } while (!valid[c]);
  }
  *lay = my_lay;
}
/*}}}*/
