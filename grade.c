
#include "sku.h"

struct option {
  int opt_flag;
  const char *name;
};

#define N_OPTIONS 5

const static struct option solve_options[N_OPTIONS] = {
  { OPT_NO_ROWCOL_ALLOC, "Row/col alloc " },
  { OPT_NO_UNIQUES,      "Uniques       " },
  { OPT_NO_SUBSETTING,   "Subsetting    " },
  { OPT_NO_CLUSTERING,   "Clustering    " },
  { OPT_NO_UCD,          "Unm. cand del." }
};

void grade(int options)
{
  struct layout *lay;
  int *state;
  int *copy;
  int limit, i, j;
  char *result;
  int count;

  read_grid(&lay, &state, options);
  copy = new_array(int, lay->nc);
  limit = 1 << N_OPTIONS;
  result = new_array(char, limit);
  for (i=0; i<limit; i++) {
    int flags, n_sol;
    flags = 0;
    for (j=0; j<N_OPTIONS; j++) {
      if (i & (1<<j)) {
        flags |= solve_options[j].opt_flag;
      }
    }
    memcpy(copy, state, lay->nc * sizeof(int));
    n_sol = infer(lay, copy, NULL, 0, 0, flags);
    if (n_sol == 1) {
      result[i] = 1;
    } else {
      result[i] = 0;
    }
  }

  /* Print result. */
  for (i=0; i<N_OPTIONS; i++) {
    int mask = 1<<i;
    fprintf(stderr, "%s ", solve_options[i].name);
    for (j=0; j<limit; j++) {
      if (j & mask) {
        fprintf(stderr, " -");
      } else {
        fprintf(stderr, " Y");
      }
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "-------------------------------------------------------------------------------\n");
  fprintf(stderr, "               ");
  for (j=0; j<limit; j++) {
    if (result[j]) {
      fprintf(stderr, " #");
    } else {
      fprintf(stderr, " -");
    }
  }
  fprintf(stderr, "\n");

  /* Work out the minimal set(s) of techniques that will do. */
  for (i=0; i<limit; i++) {
    if (result[i]) {
      for (j=0; j<limit; j++) {
        if (((i & j) == j) && (i != j)) {
          result[j] = 0;
        }
      }
    }
  }

  fprintf(stderr, "\nMINIMAL OPTIONS TO SOLVE:\n");
  count = 1;
  for (i=0; i<limit; i++) {
    if (result[i]) {
      fprintf(stderr, "%4d: ", count++);
      if (i==(limit-1)) {
        fprintf(stderr, " (no special techniques required)\n");
      } else {
        for (j=0; j<N_OPTIONS; j++) {
          int mask = 1<<j;
          if (!(i & mask)) {
            fprintf(stderr, "<%s> ", solve_options[j].name);
          }
        }
      }
      fprintf(stderr, "\n");
    }
  }

  free(result);
  free(copy);
  free(state);
}
