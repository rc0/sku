/*
 *  sku - analysis tool for Sudoku puzzles
 *  Copyright (C) 2005  Richard P. Curnow
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef SKU_H
#define SKU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* To cope with interlocked puzzles where some cells may be in 8 logical groups
 * (namely, the ones in the overlapping blocks if diagonals mode is on.) */
#define NDIM 8

struct cell {/*{{{*/
  char *name;           /* cell name for verbose + debug output. */
  int index;            /* self-index (to track reordering during geographical sort.) */
  int is_overlap;
  int is_terminal;      /* Hack, need to do this properly. */
  short prow, pcol;     /* coordinates for printing to text output */
  short rrow, rcol;     /* raw coordinates for printing to formatted output (SVG etc) */
  short isym;           /* index of next cell in same symmetry group (circular ring) */
  short nbr[4];         /* indices of N,E,S,W neighbouring cells (-1 => none, i.e. at edge) */
  short group[NDIM];    /* table of groups the cell is in (-1 for unused dimensions) */
};
/*}}}*/

#define SYM(x) (lay->cells[(x)].isym)

struct displacement {/*{{{*/
  int dy;
  int dx;
};
/*}}}*/
extern struct displacement nbr_dis[4];

struct dline/*{{{*/
{
  /* Start and end of a line for formatted output. */
  short x0, y0;
  short x1, y1;
};
/*}}}*/
struct layout {/*{{{*/
  /* examples are for the regular 9x9 puzzle. */
  char *name;           /* tag for writing in header card */
  int ns;               /* number of symbols, e.g. 9 (=groupsize)*/
  int nc;               /* number of cells, e.g. 81 */
  int ng;               /* number of groups, e.g. 27 */
  int prows;            /* #rows for generating print grid */
  int pcols;            /* #cols for generating print grid */
 
  int is_additive;      /* Layout supports additive constraints. */
  
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
/*}}}*/
struct subgrid {/*{{{*/
  int yoff;
  int xoff;
  char *name;
};
/*}}}*/

enum corner {NW, NE, SE, SW};

struct subgrid_link {/*{{{*/
  int index0;
  enum corner corner0;
  int index1;
  enum corner corner1;
};
/*}}}*/
struct super_layout {/*{{{*/
  int n_subgrids;
  struct subgrid *subgrids;
  int n_links;
  struct subgrid_link *links;
};
/*}}}*/

/* ============================================================================ */
struct constraint {/*{{{*/
  int do_lines;
  int do_subsets;
  int do_onlyopt;
  int max_partition_size;
  int is_default;
};
/*}}}*/

#define MAX_PARTITION_SIZE 5
const extern struct constraint cons_all, cons_none;

/* ============================================================================ */

struct clusters {/*{{{*/
  unsigned char *cells;
  short total[256];
};
/*}}}*/
struct point {/*{{{*/
  double y;
  double x;
};
/*}}}*/
struct cluster_coords {/*{{{*/
  /* Line segments. */
  int n_points;
  struct point *points;
  unsigned char *type;

  /* Cluster totals */
  int n_numbers;
  struct point *numbers;
  int *values;
};
 /*}}}*/

/* ============================================================================ */

/* A regular empty cell, waiting to be solved */
#define CELL_EMPTY -1
/* The same, but the cell is 'greyed'.  (If the puzzle only requires a subset
 * of the cells to be solved, they are marked in grey.) */
#define CELL_MARKED -2
/* Denotes a cell which we don't try to operate on.  (e.g. To find out which
 * cells can be totally ignored in trying to solve for just the greyed cells.)
 */
#define CELL_BARRED -3

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
#define OPT_HINT (1<<8)
#define OPT_ALLOW_TRIVIAL (1<<9)
#define OPT_SHOW_ALL (1<<10)
#define OPT_ONLYOPT_FIRST (1<<11)
#define OPT_SCORE (1<<12)
#define OPT_SOLVE_MARKED (1<<13)
#define OPT_SOLVE_MINIMAL (1<<14)

/* ============================================================================ */

/* In util.c */
extern int count_bits(unsigned int a);
extern int decode(unsigned int a);
extern char *tobin(int n, int x);
extern void show_symbols_in_set(int ns, const char *symbols, int bitmap);
extern void setup_terminals(struct layout *lay);
extern struct clusters *mk_clusters(int nc);
extern void free_clusters(struct clusters *x);

/* In infer.c */
int infer(struct layout *lay, int *state, int *order, int *score, const struct constraint *cons, int options);

/* In superlayout.c */
extern void superlayout_5(struct super_layout *superlay);
extern void superlayout_8(struct super_layout *superlay);
extern void superlayout_9(struct super_layout *superlay);
extern void superlayout_11(struct super_layout *superlay);
extern void layout_MxN_superlay(int M, int N, int x_layout,
    const struct super_layout *superlay, struct layout *lay, int options);
extern void free_superlayout(struct super_layout *superlay);

/* In layout_mxn.c */
extern void layout_MxN(int M, int N, int x_layout, struct layout *lay, int options);
extern void free_layout(struct layout *lay);
extern void free_layout_lite(struct layout *lay);

/* In genlayout.c */
extern void find_symmetries(struct layout *lay, int options);
extern void debug_layout(struct layout *lay);
extern struct layout *genlayout(const char *name, int options);

/* In reader.c */
extern void read_grid(struct layout **lay, int **state, struct clusters **clus, int options);

/* In blank.c */
extern void blank(struct layout *lay);

/* In display.c */
void display(FILE *out, struct layout *lay, int *state, struct clusters *clus);

/* In solve.c */
extern void solve(const struct constraint *simplify_cons, int options);
extern void solve_any(int options);

/* In reduce.c */
extern int inner_reduce(struct layout *lay, int *state, const struct constraint *simplify_cons, int options);
extern void reduce(int iters_for_min,
    const struct constraint *simplify_cons,
    const struct constraint *required_cons, 
    int options);

/* In mark.c */
extern void mark_cells(int grey_cells, const struct constraint *simplify_cons, int options);


/* In grade.c */
extern void grade(int options);
  
/* In svg.c */
extern void format_output(int options);

/* In tidy.c */
extern void tidy(int options);

/* In clusters.c */
extern struct cluster_coords *mk_cluster_coords(const struct layout *lay, const struct clusters *clus);
extern void free_cluster_coords(struct cluster_coords *x);

#endif /* SKU_H */

