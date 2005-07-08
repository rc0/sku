#ifndef SKU_H
#define SKU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* To cope with interlocked puzzles where some cells may be in 6 logical groups. */
#define NDIM 6

struct cell {
  char *name;           /* cell name for verbose + debug output. */
  int index;            /* self-index (to track reordering during geographical sort.) */
  short prow, pcol;     /* coordinates for printing to text output */
  short rrow, rcol;     /* raw coordinates for printing to formatted output (SVG etc) */
  short sy180, sy90;    /* indices of cells that are 180/90 deg away clockwise (-1 for none) */
  short group[NDIM];    /* table of groups the cell is in (-1 for unused dimensions) */
};

struct dline
{
  /* Start and end of a line for formatted output. */
  short x0, y0;
  short x1, y1;
};

/* examples are for the regular 9x9 puzzle. */
struct layout {
  int n;                /* sqrt(ns) */
  int ns;               /* number of symbols, e.g. 9 (=groupsize)*/
  int nc;               /* number of cells, e.g. 81 */
  int ng;               /* number of groups, e.g. 27 */
  int prows;            /* #rows for generating print grid */
  int pcols;            /* #cols for generating print grid */
  
  /* For generating the lines on the formatted output. */
  int n_thinlines;
  struct dline *thinlines;
  int n_thicklines;
  struct dline *thicklines;

  const char *symbols;        /* [ns] table of the symbols */
  struct cell *cells;   /* [nc] table of cell definitions */
  short *groups;        /* [ng*ns] table of cell indices in each of the groups */
  char **group_names;    /* [ng] array of strings. */
};

struct subgrid {
  int yoff;
  int xoff;
  char *name;
};

enum corner {NW, NE, SE, SW};

struct subgrid_link {
  int index0;
  enum corner corner0;
  int index1;
  enum corner corner1;
};

struct super_layout {
  int n_subgrids;
  struct subgrid *subgrids;
  int n_links;
  struct subgrid_link *links;
};

/* ============================================================================ */

#define new_array(T, n) (T *) malloc((n) * sizeof(T))

#define OPT_VERBOSE (1<<0)
#define OPT_FIRST_ONLY (1<<1)
#define OPT_STOP_ON_2 (1<<2)
#define OPT_SPECULATE (1<<3)
#define OPT_SYM_180 (1<<4)
#define OPT_SYM_90 (1<<5)

/* ============================================================================ */

/* In util.c */
extern int count_bits(unsigned int a);
extern int decode(unsigned int a);
extern char *tobin(int n, int x);
extern void show_symbols_in_set(int ns, const char *symbols, int bitmap);

/* In infer.c */
int infer(struct layout *lay, int *state, int *order, int iter, int solvepos, int options);

/* In superlayout.c */
extern void superlayout_5(struct super_layout *superlay);
extern void superlayout_8(struct super_layout *superlay);
extern void superlayout_9(struct super_layout *superlay);
extern void superlayout_11(struct super_layout *superlay);
extern void layout_N_superlay(int N, const struct super_layout *superlay, struct layout *lay);

/* In layout.c */
extern void find_symmetries(struct layout *lay);
extern void layout_NxN(int N, struct layout *lay);
extern void debug_layout(struct layout *lay);

/* In reader.c */
extern void read_grid(struct layout *lay, int *state);

/* In blank.c */
extern void blank(struct layout *lay);

/* In display.c */
void display(FILE *out, struct layout *lay, int *state);

/* In solve.c */
extern void solve(struct layout *lay, int options);
extern void solve_any(struct layout *lay, int options);

/* In reduce.c */
extern int inner_reduce(struct layout *lay, int *state, int options);
extern void reduce(struct layout *lay, int iters_for_min, int options);

/* In mark.c */
extern void mark_cells(struct layout *lay, int grey_cells, int options);
  
/* In svg.c */
extern void format_output(struct layout *lay, int options);

#endif /* SKU_H */

