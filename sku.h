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
  int is_overlap;
  short prow, pcol;     /* coordinates for printing to text output */
  short rrow, rcol;     /* raw coordinates for printing to formatted output (SVG etc) */
  short isym;           /* index of next cell in same symmetry group (circular ring) */
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
  char *name;           /* tag for writing in header card */
  int ns;               /* number of symbols, e.g. 9 (=groupsize)*/
  int nc;               /* number of cells, e.g. 81 */
  int ng;               /* number of groups, e.g. 27 */
  int prows;            /* #rows for generating print grid */
  int pcols;            /* #cols for generating print grid */
  
  /* For generating the lines on the formatted output. */
  int n_thinlines;
  struct dline *thinlines;
  int n_mediumlines;
  struct dline *mediumlines;
  int n_thicklines;
  struct dline *thicklines;

  const char *symbols;        /* [ns] table of the symbols */
  struct cell *cells;   /* [nc] table of cell definitions */
  char *is_block;       /* 1 flag per group: is it one of the MxN mini-rectangle groups (1) or a row/col (0) */
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

#define new(T) (T *) malloc(sizeof(T))
#define new_array(T, n) (T *) malloc((n) * sizeof(T))

#define OPT_VERBOSE (1<<0)
#define OPT_FIRST_ONLY (1<<1)
#define OPT_STOP_ON_2 (1<<2)
#define OPT_SPECULATE (1<<3)
#define OPT_SYM_180 (1<<4)
#define OPT_SYM_90 (1<<5)
#define OPT_SYM_HORIZ (1<<6)
#define OPT_SYM_VERT (1<<7)

#define OPT_NO_SUBSETTING (1<<10)
#define OPT_NO_ROWCOL_ALLOC (1<<11)
#define OPT_NO_UNIQUES (1<<12)
#define OPT_NO_CLUSTERING (1<<13)
#define OPT_NO_UCD (1<<14)

#define OPT_MAKE_EASIER (OPT_NO_SUBSETTING | OPT_NO_ROWCOL_ALLOC | OPT_NO_UNIQUES | OPT_NO_CLUSTERING | OPT_NO_UCD)

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
extern void layout_MxN_superlay(int M, int N, const struct super_layout *superlay, struct layout *lay, int options);

/* In layout_mxn.c */
extern void layout_MxN(int M, int N, struct layout *lay, int options);

/* In genlayout.c */
extern void find_symmetries(struct layout *lay, int options);
extern void debug_layout(struct layout *lay);
extern struct layout *genlayout(const char *name, int options);

/* In reader.c */
extern void read_grid(struct layout **lay, int **state, int options);

/* In blank.c */
extern void blank(struct layout *lay);

/* In display.c */
void display(FILE *out, struct layout *lay, int *state);

/* In solve.c */
extern void solve(int options);
extern void solve_any(int options);

/* In reduce.c */
extern int inner_reduce(struct layout *lay, int *state, int options);
extern void reduce(int iters_for_min, int options);

/* In mark.c */
extern void mark_cells(int grey_cells, int options);

/* In grade.c */
extern void grade(int options);
  
/* In svg.c */
extern void format_output(int options);

#endif /* SKU_H */

