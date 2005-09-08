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

#include "sku.h"

/* ============================================================================ */

struct ws;
struct queue;

struct link {/*{{{*/
  /* Node for holding a cell or group index in a queue. */
  struct link *next;
  struct link *prev;
  int index;
  struct queue *q; /* which queue the link is on, NULL o/w */
  struct queue *base_q; /* which queue the link has to go back to when its resouce is updated. */
};
/*}}}*/

struct score {
  double foo;
};


/* Returns -1 if an error has been detected,
 * 0 if no work got done,
 * 1 if work was done. */
typedef int (*WORKER)(int, struct layout *, struct ws *, int, struct score *score);
  
struct queue {/*{{{*/
  struct link links; /* .index ignored */
  struct queue *next_to_run;
  struct queue *next_to_push;
  int opt;
  char *name;
  WORKER worker;
};
/*}}}*/
static struct queue *mk_queue(WORKER worker, struct queue *next_to_run, struct queue *next_to_push, int opt, char *name)/*{{{*/
{
  struct queue *x;
  x = new(struct queue);
  x->links.next = x->links.prev = &x->links;
  x->worker = worker;
  x->next_to_run = next_to_run;
  x->next_to_push = next_to_push;
  x->opt = opt;
  x->name = name;
  return x;
}
/*}}}*/
static void free_queue(struct queue *x)/*{{{*/
{
  free(x);
}
/*}}}*/
static struct link *dequeue(struct queue *q) /*{{{*/
{
  struct link *base, *first;
  base = &q->links;
  first = base->next;
  if (first == base) {
    /* Empty queue */
    return NULL;
  }
  first->next->prev = base;
  base->next = first->next;
  first->next = first->prev = first;
  first->q = NULL;
  return first;
}
/*}}}*/
static void enqueue(struct link *lk, struct queue *q)/*{{{*/
{
  struct link *base;
  if (lk->q) {
    fprintf(stderr, "Can't enqueue %d, it's already on a queue!!\n", lk->index);
    exit(2);
  }
  base = &q->links;
  lk->prev = base->prev;
  lk->next = base;
  base->prev->next = lk;
  base->prev = lk;
  lk->q = q;
}
/*}}}*/
static void move_to_queue(struct link *lk, struct queue *to_q)/*{{{*/
{
  struct link *base;
  if (lk->q == to_q) {
    return;
  }

  /* Take off old queue. */
  if (lk->q) {
    lk->next->prev = lk->prev;
    lk->prev->next = lk->next;
  }
  /* Push onto new queue */
  base = &to_q->links;
  lk->prev = base->prev;
  lk->next = base;
  lk->q = to_q;
  base->prev->next = lk;
  base->prev = lk;
}
/*}}}*/

/* ============================================================================ */

struct ws {/*{{{*/
  int nc, ng, ns;

  int *poss;
  int *todo;
  int solvepos;
  int spec_depth;
  int n_todo;
  int n_marked_todo;
  double score;

  /* Pointers/values passed in at the outer level. */
  int options;
  int *order;
  
  int *state;

  /* Queues for what to scan next. */
  struct queue *base_q;
  struct queue *base_cell_q;
  struct queue *base_line_q;
  struct queue *base_block_q;
  struct link *group_links;
  struct link *cell_links;
};
/*}}}*/
static struct ws *make_ws(int nc, int ng, int ns)/*{{{*/
{
  struct ws *ws = new(struct ws);
  int fill;
  int i;

  ws->nc = nc;
  ws->ng = ng;
  ws->ns = ns;
  ws->spec_depth = 0;

  fill = (1<<ns) - 1;
  ws->todo = new_array(int, ng);
  ws->poss = new_array(int, nc);
  ws->n_todo = 0;
  ws->n_marked_todo = -1;
  ws->score = 0.0;
  for (i=0; i<ng; i++) ws->todo[i] = fill;
  for (i=0; i<nc; i++) ws->poss[i] = fill;

  ws->group_links = new_array(struct link, ng);
  for (i=0; i<ng; i++) {
    ws->group_links[i].next = ws->group_links[i].prev = &ws->group_links[i];
    ws->group_links[i].index = i;
    ws->group_links[i].q = NULL;
  }
  ws->cell_links = new_array(struct link, nc);
  for (i=0; i<nc; i++) {
    ws->cell_links[i].next = ws->cell_links[i].prev = &ws->cell_links[i];
    ws->cell_links[i].index = i;
    ws->cell_links[i].q = NULL;
  }

  return ws;
}
/*}}}*/
static void set_base_queues(const struct layout *lay, struct ws *ws)/*{{{*/
{
  int gi, ci;
  for (gi=0; gi<lay->ng; gi++) {
    struct link *x;
    x = ws->group_links + gi;
    if (lay->is_block[gi]) {
      x->base_q = ws->base_block_q;
    } else {
      x->base_q = ws->base_line_q;
    }
  }
  for (ci=0; ci<lay->nc; ci++) {
    struct link *x;
    x = ws->cell_links + ci;
    x->base_q = ws->base_cell_q;
  }
}
/*}}}*/
static int *copy_array(int n, int *data)/*{{{*/
{
  int *result;
  result = new_array(int, n);
  memcpy(result, data, n * sizeof(int));
  return result;
}
/*}}}*/
static struct ws *clone_ws(const struct ws *src)/*{{{*/
{
  struct ws *ws;
  ws = new(struct ws);
  ws->nc = src->nc;
  ws->ng = src->ng;
  ws->ns = src->ns;
  ws->spec_depth = 1 + src->spec_depth;
  ws->n_todo = src->n_todo;
  ws->n_marked_todo = src->n_marked_todo;
  ws->solvepos = src->solvepos;
  ws->poss = copy_array(src->nc, src->poss); 
  ws->todo = copy_array(src->ng, src->todo);
  ws->options = src->options;
  ws->order = src->order;
  ws->state = NULL;
  ws->score = 0.0;

  /* When we need to clone, we know the queues must be empty!  We share this
   * with the outer context(s) since sharing is cheaper. */
  ws->base_q = src->base_q;
  ws->base_cell_q = src->base_cell_q;
  ws->base_line_q = src->base_line_q;
  ws->base_block_q = src->base_block_q;
  ws->group_links = src->group_links;
  ws->cell_links = src->cell_links;
  
  return ws;
}
/*}}}*/
static void free_cloned_ws(struct layout *lay, struct ws *ws)/*{{{*/
{
  struct queue *q;
  int i;
  
  /* Clean queues. */
  q = ws->base_q;
  while (q) {
    struct link *lk = &q->links;
    lk->next = lk->prev = lk;
    q = q->next_to_run;
  }
  for (i=0; i<lay->ng; i++) {
    struct link *lk = ws->group_links + i;
    lk->q = NULL;
  }
  for (i=0; i<lay->nc; i++) {
    struct link *lk = ws->cell_links + i;
    lk->q = NULL;
  }
  
  free(ws->poss);
  free(ws->todo);
  free(ws);
}
/*}}}*/
static void free_ws(struct ws *ws)/*{{{*/
{
  struct queue *q;
  free(ws->poss);
  free(ws->todo);

  q = ws->base_q;
  while (q) {
    struct queue *nq = q->next_to_run;
    free_queue(q);
    q = nq;
  }
  free(ws->group_links);
  free(ws->cell_links);

  free(ws);
}
/*}}}*/

/* ============================================================================ */

static int inner_infer(struct layout *lay, struct ws *ws);

/* ============================================================================ */

static void requeue_group(int gi, struct layout *lay, struct ws *ws)/*{{{*/
{
  struct link *lk = ws->group_links + gi;
  if (lk->base_q)  {
    move_to_queue(lk, lk->base_q);
  } else {
    /* No rules are available for this type of resource, ignore. */
  }
}
/*}}}*/
static void requeue_groups(struct layout *lay, struct ws *ws, int ic)/*{{{*/
{
  int i;
  struct cell *cell = lay->cells + ic;
  for (i=0; i<NDIM; i++) {
    int gi = cell->group[i];
    if (gi >= 0) requeue_group(gi, lay, ws);
    else break;
  }
}
/*}}}*/
static void requeue_cell(int ci, struct layout *lay, struct ws *ws)/*{{{*/
{
  if (ws->state[ci] != CELL_BARRED) {
    move_to_queue(ws->cell_links + ci, ws->base_cell_q);
  }
}
/*}}}*/

static void allocate(struct layout *lay, struct ws *ws, int is_init, int ic, int val)/*{{{*/
{
  int mask;
  int j, k;
  int NS;
  short *base;
  int other_poss;

  mask = 1<<val;
  NS = lay->ns;

  if (ws->state[ic] == CELL_MARKED) {
    --ws->n_marked_todo;
  }
  ws->state[ic] = val;

  other_poss = ws->poss[ic] & ~mask;
  ws->poss[ic] = 0;
  if (!is_init && ws->order) {
    ws->order[ic] = (ws->solvepos)++;
  }

  for (k=0; k<NDIM; k++) {
    int gg = lay->cells[ic].group[k];
    if (gg >= 0) {
      ws->todo[gg] &= ~mask;
      requeue_group(gg, lay, ws);
      base = lay->groups + gg*NS;
      for (j=0; j<NS; j++) {
        int jc;
        jc = base[j];
        if (ws->poss[jc] & mask) {
          ws->poss[jc] &= ~mask;
          requeue_cell(jc, lay, ws);
          requeue_groups(lay, ws, jc);
          if (lay->cells[ic].is_terminal) {
            lay->cells[ic].is_terminal = 0;
          }
        }
        if (ws->poss[jc] & other_poss) {
          /* The discovery of state[ic] has contributed to eventually solving [jc],
           * so [ic] is now non-terminal. */
          if (lay->cells[ic].is_terminal) {
            lay->cells[ic].is_terminal = 0;
          }
        }
      }
    } else {
      break;
    }
  }
}
/*}}}*/
static int try_group_allocate(int gi, struct layout *lay, struct ws *ws, int opt, struct score *score)/*{{{*/
{
  /* Return -1 if the solution is broken,
   *        0 if we didn't allocate anything,
   *        1 if we did. */
  int NS;
  short *base;
  int sym, mask;
  int found_any = 0;

  NS = lay->ns;
  base = lay->groups + gi*NS;
  for (sym=0; sym<NS; sym++) {
    mask = 1<<sym;
    if (ws->todo[gi] & mask) {
      int j, count, xic;
      xic = -1;
      count = 0;
      for (j=0; j<NS; j++) {
        int ic = base[j];
        if (ws->poss[ic] & mask) {
          xic = ic;
          count++;
          if (count > 1) break;
        }
      }
      if (count == 0) {
        if (!(ws->options & OPT_SPECULATE)) {
          fprintf(stderr, "Cannot allocate <%c> in <%s>\n",
              lay->symbols[sym], lay->group_names[gi]);
        }
        return -1;
      } else if (count == 1) {
        if (score) {
          score->foo += 1.0 / (double) count_bits(ws->todo[gi]);
          found_any = 1;
        } else {
          if (ws->state[xic] != CELL_BARRED) {
            if (ws->options & OPT_VERBOSE) {
              fprintf(stderr, "(%c) Allocate <%c> to <%s> (allocate in <%s>)\n",
                  (lay->is_block[gi] ? ' ' : 'l'),
                  lay->symbols[sym], lay->cells[xic].name, lay->group_names[gi]);
            }
            --ws->n_todo;
            allocate(lay, ws, 0, xic, sym);
            if (ws->options & OPT_HINT) {
              free_ws(ws);
              free_layout(lay);
              exit(0);
            }
            found_any = 1;
          }
        }
      }
    }
  }

  return found_any ? 1 : 0;

}
/*}}}*/
static int try_subsets(int gi, struct layout *lay, struct ws *ws, int opt, struct score *score)/*{{{*/
{
  /* Couldn't do any allocates in the group.
   * So try the more sophisticated analysis:
   * Analyse the group to find out which subset of cells can contain
   * each unallocated symbol.  If this subset is also a subset of some other
   * group, we can eliminate the symbol as a possibility from the rest of
   * that other group.
   */
  int NC, NS, NG;
  char *flags;
  int *counts;
  int sym;
  int n_poss_cells;
  short *base;
  int did_anything = 0;

  NS = lay->ns;
  NC = lay->nc;
  NG = lay->ng;
  flags = new_array(char, NC);
  counts = new_array(int, NG);

  base = lay->groups + gi*NS;
  for (sym=0; sym<NS; sym++) {
    int mask = (1 << sym);
    if (ws->todo[gi] & mask) {
      int j;
      int found_something = 0;
      memset(flags, 0, NC);
      memset(counts, 0, NG*sizeof(int));
      n_poss_cells = 0;
      for (j=0; j<NS; j++) {
        int ic = base[j];
        if (ws->poss[ic] & mask) {
          int m;
          ++n_poss_cells;
          flags[ic] = 1;
          for (m=0; m<NDIM; m++) {
            int gm = lay->cells[ic].group[m];
            if (gm >= 0) {
              if (gm != gi) ++counts[gm];
            } else {
              break;
            }
          }
        }
      }
      for (j=0; j<NG; j++) {
        if (counts[j] == n_poss_cells) {
          int m;
          short *base = lay->groups + j*NS;
          for (m=0; m<NS; m++) {
            int ic = base[m];
            if (!flags[ic]) { /* cell not in the original group. */
              if (ws->poss[ic] & mask) {
                if (score) {
                } else {
                  if (ws->options & OPT_VERBOSE) {
                    fprintf(stderr, "(s) Removing <%c> from <%s> (in <%s> due to placement in <%s>)\n",
                        lay->symbols[sym], lay->cells[ic].name,
                        lay->group_names[j], lay->group_names[gi]);
                  }
                  ws->poss[ic] &= ~mask;
                  requeue_cell(ic, lay, ws);
                  requeue_groups(lay, ws, ic);
                  found_something = 1;
                }
                did_anything = 1;
              }
            }
          }
        }
      }
    }
  }
  free(counts);
  free(flags);
  return did_anything ? 1 : 0;
}
/*}}}*/

static int do_ext_remove(int gi, struct layout *lay, struct ws *ws, int n, int symbol_set, int matching_cells)/*{{{*/
{
  int i;
  int NS = lay->ns;
  short *base;
  int did_anything = 0;
  base = lay->groups + (gi * NS);
#if 0
  fprintf(stderr, "Symbol set : ");
  show_symbols_in_set(NS, lay->symbols, symbol_set);
  fprintf(stderr, "\n");
  for (i=0; i<NS; i++) {
    int ic = base[i];
    fprintf(stderr, "  %c %s : ",
        ((1<<i) & matching_cells) ? '*' : ' ',
        lay->cells[ic].name);
    show_symbols_in_set(NS, lay->symbols, ws->poss[ic]);
    fprintf(stderr, "\n");
  }
#endif
  
  for (i=0; i<NS; i++) {
    int ic = base[i];
    int mask = (1 << i);
    if (mask & matching_cells) continue;
    if (ws->poss[ic] & symbol_set) {
      did_anything = 1;
      if (ws->options & OPT_VERBOSE) {
        int k, fk;
        fprintf(stderr, "(e%d) Removing <", n);
        show_symbols_in_set(NS, lay->symbols, symbol_set & ws->poss[ic]);
        fprintf(stderr, "> from <%s:", lay->cells[ic].name);
        show_symbols_in_set(NS, lay->symbols, ws->poss[ic]);
        fprintf(stderr, ">, because <");
        show_symbols_in_set(NS, lay->symbols, symbol_set);
        fprintf(stderr, "> must be in <");
        fk = 1;
        for (k=0; k<NS; k++) {
          if (matching_cells & (1<<k)) {
            int ck = base[k];
            if (!fk) {
              fprintf(stderr, ",");
            }
            fk = 0;
            fprintf(stderr, "%s(", lay->cells[ck].name);
            show_symbols_in_set(NS, lay->symbols, ws->poss[ck]);
            fprintf(stderr, ")");
          }
        }
        fprintf(stderr, "> in <%s>\n", lay->group_names[gi]);
      }
      ws->poss[ic] &= ~symbol_set;
      requeue_cell(ic, lay, ws);
      requeue_groups(lay, ws, ic);
    }
  }
  return did_anything;
}
/*}}}*/
static int do_int_remove(int gi, struct layout *lay, struct ws *ws, int n, int cell_set, int matching_symbols)/*{{{*/
{
  int i;
  int NS = lay->ns;
  short *base;
  int did_anything = 0;
  base = lay->groups + (gi * NS);

#if 0
  fprintf(stderr, "Matching symbols : ");
  show_symbols_in_set(NS, lay->symbols, matching_symbols);
  fprintf(stderr, "\n");
  for (i=0; i<NS; i++) {
    int ic = base[i];
    fprintf(stderr, "  %c %s : ",
        ((1<<i) & cell_set) ? '*' : ' ',
        lay->cells[ic].name);
    show_symbols_in_set(NS, lay->symbols, ws->poss[ic]);
    fprintf(stderr, "\n");
  }
#endif
  for (i=0; i<NS; i++) {
    if (cell_set & (1<<i)) {
      int ic = base[i];
      if (ws->poss[ic] & ~matching_symbols) {
        if (ws->options & OPT_VERBOSE) {
          fprintf(stderr, "(i%d) Removing <", n);
          show_symbols_in_set(NS, lay->symbols, ws->poss[ic] & ~matching_symbols);
          fprintf(stderr, "> from <%s>, must be one of <", lay->cells[ic].name);
          show_symbols_in_set(NS, lay->symbols, matching_symbols);
          fprintf(stderr, "> in <%s>\n", lay->group_names[gi]);
        }
        did_anything = 1;
        ws->poss[ic] &= matching_symbols;
        requeue_cell(ic, lay, ws);
        requeue_groups(lay, ws, ic);
      }
    }
  }
  return did_anything;
}
/*}}}*/
static int try_partition(int gi, struct layout *lay, struct ws *ws, int opt, struct score *score)/*{{{*/
{
  int N, NN;
  int cmap[64], icmap[64];
  int smap[64], ismap[64];
  int fposs[64], rposs[64];
  int i, j, k;
  short *base;
  
  base = lay->groups + (gi * lay->ns);
  NN = N = count_bits(ws->todo[gi]);
  N >>= 1; /* Only have to examine up to 1/2 that number */
  N++;
  if (N > 4) N = 4; /* Maximise for now. */

  memset(rposs, 0, sizeof(int) * lay->ns);
  /* Loop over symbols */
  j = 0;
  for (i=0; i<lay->ns; i++) {
    int mask = 1<<i;
    if (mask & ws->todo[gi]) {
      smap[j] = i;
      ismap[i] = j;
      j++;
    }
  }
  if (j != NN) {
    fprintf(stderr, "j != NN at %d\n", __LINE__);
    exit(1);
  }
  /* Loop over cells */
  j = 0;
  for (i=0; i<lay->ns; i++) {
    int ic = base[i];
    if (ws->state[ic] < 0) {
      cmap[j] = i;
      icmap[i] = j;
      fposs[j] = ws->poss[ic];
      for (k=0; k<lay->ns; k++) {
        int mask = 1<<k;
        int kk = ismap[k];
        if (mask & ws->poss[ic]) {
          /* Build map of which cells can take which symbols. */
          rposs[kk] |= (1<<i);
        }
      }
      j++;
    }
  }
  if (j != NN) {
    fprintf(stderr, "j != NN at %d\n", __LINE__);
    exit(1);
  }

  for (i=2; i<N; i++) {
    switch (i) {
      case 2:/*{{{*/
        {
          int a0, a1;
          for (a0 = 1; a0 < NN; a0++) {
            for (a1 = 0; a1 < a0; a1++) {
              int U;
              U = fposs[a0] | fposs[a1];
              if (count_bits(U) == 2) {
                if (!score) {
                  int matching_cells = (1 << cmap[a0]) | (1 << cmap[a1]);
                  if (do_ext_remove(gi, lay, ws, 2, U, matching_cells)) 
                    return 1;
                }
              }
              U = rposs[a0] | rposs[a1];
              if (count_bits(U) == 2) {
                if (!score) {
                  int matching_symbols = (1 << smap[a0]) | (1 << smap[a1]);
                  /* Hit : interior split */
                  if (do_int_remove(gi, lay, ws, 2, U, matching_symbols)) 
                    return 1;
                }
              }
            }
          }
        } 
        break;
/*}}}*/
      case 3:/*{{{*/
        {
          int a0, a1, a2;
          for (a0 = 2; a0 < NN; a0++) {
            for (a1 = 1; a1 < a0; a1++) {
              for (a2 = 0; a2 < a1; a2++) {
                int U;
                U = fposs[a0] | fposs[a1] | fposs[a2];
                if (count_bits(U) == 3) {
                  if (!score) {
                    int matching_cells = (1 << cmap[a0]) | (1 << cmap[a1]) | (1 << cmap[a2]);
                    if (do_ext_remove(gi, lay, ws, 3, U, matching_cells)) 
                      return 1;
                  }
                }
                U = rposs[a0] | rposs[a1] | rposs[a2];
                if (count_bits(U) == 3) {
                  if (!score) {
                    int matching_symbols = (1 << smap[a0]) | (1 << smap[a1]) | (1 << smap[a2]);
                    /* Hit : interior split */
                    if (do_int_remove(gi, lay, ws, 3, U, matching_symbols))
                      return 1;
                  }
                }
              }
            }
          }
        } 
        break;
/*}}}*/
      case 4:/*{{{*/
        {
          int a0, a1, a2, a3;
          for (a0 = 3; a0 < NN; a0++) {
            for (a1 = 2; a1 < a0; a1++) {
              for (a2 = 1; a2 < a1; a2++) {
                for (a3 = 0; a3 < a2; a3++) {
                  int U;
                  U = fposs[a0] | fposs[a1] | fposs[a2] | fposs[a3];
                  if (count_bits(U) == 4) {
                    if (!score) {
                      int matching_cells = (1 << cmap[a0]) | (1 << cmap[a1]) | (1 << cmap[a2]) | (1 << cmap[a3]);
                      if (do_ext_remove(gi, lay, ws, 4, U, matching_cells)) 
                        return 1;
                    }
                  }
                  U = rposs[a0] | rposs[a1] | rposs[a2] | rposs[a3];
                  if (count_bits(U) == 4) {
                    if (!score) {
                      int matching_symbols = (1 << smap[a0]) | (1 << smap[a1]) | (1 << smap[a2]) | (1 << smap[a3]);
                      /* Hit : interior split */
                      if (do_int_remove(gi, lay, ws, 4, U, matching_symbols)) {
                        return 1;
                      }
                    }
                  }
                }
              }
            }
          }
        } 
        break;
/*}}}*/
      default:
        fprintf(stderr, "Not handled yet\n");
    }
  }
  return 0;
}
/*}}}*/

static int try_split_internal(int gi, struct layout *lay, struct ws *ws, int opt, struct score *score)/*{{{*/
{
  /* 
   * Deal with this case: suppose the symbols 2,3,5,6 are unallocated within
   * a group.  Suppose there are two cells
   *   A that could be 2,3,5
   *   B that could be 2,3,6
   * and neither 2 nor 3 could go anywhere else within the group under
   * analysis.  Then clearly we can eliminate 5 as a possibility on A and 6
   * as a possibility on B.
   * */

  int NC, NS, NG;
  int did_anything = 0;
  int *intersect, *poss_map;
  int sym, cell, ci;
  int fill, mask;
  short *base;

  NS = lay->ns;
  NC = lay->nc;
  NG = lay->ng;
  intersect = new_array(int, NS);
  poss_map = new_array(int, NS);

  base = lay->groups + gi*NS;
  fill = (1 << NS) - 1;
  
  for (sym=0; sym<NS; sym++) {
    intersect[sym] = fill;
    poss_map[sym] = 0;
    mask = 1<<sym;
    for (cell=0; cell<NS; cell++) {
      ci = base[cell];
      if (ws->poss[ci] & mask) {
        intersect[sym] &= ws->poss[ci];
        poss_map[sym] |= (1<<cell);
      }
    }
  }

  /* Now analyse to look for candidates. */
  for (sym=0; sym<NS; sym++) {
    if (count_bits(intersect[sym]) == count_bits(poss_map[sym])) {
      /* that is a necessary condition... */
      int sym1;
      for (sym1=0; sym1<NS; sym1++) {
        int mask = 1<<sym1;
        if (sym1 == sym) continue;
        if (intersect[sym] & mask) {
          if ((intersect[sym] == intersect[sym1]) &&
              (poss_map[sym] == poss_map[sym1])) {

          } else {
            goto examine_next_symbol;
          }
        }
      }

      /* Good subset: */
      for (cell=0; cell<NS; cell++) {
        if (poss_map[sym] & (1<<cell)) {
          int ci = base[cell];
          if (ws->poss[ci] != intersect[sym]) {
            if (score) {
            } else {
              if (ws->options & OPT_VERBOSE) {
                fprintf(stderr, "(i) Removing <");
                show_symbols_in_set(NS, lay->symbols, ws->poss[ci] & ~intersect[sym]);
                fprintf(stderr, "> from <%s>, must be one of <", lay->cells[ci].name);
                show_symbols_in_set(NS, lay->symbols, intersect[sym]);
                fprintf(stderr, "> in <%s>\n", lay->group_names[gi]);
              }
              ws->poss[ci] = intersect[sym];
              requeue_cell(ci, lay, ws);
              requeue_groups(lay, ws, ci);
            }
            did_anything = 1;
          }
        }
      }
    }
examine_next_symbol:
    (void) 0;
  }

  free(intersect);
  free(poss_map);
  return did_anything ? 1 : 0;
}
/*}}}*/
static int subset_or_eq_p(int x, int y)/*{{{*/
{
  if (x & ~y) return 0;
  else return 1;
}
/*}}}*/
static int try_split_external(int gi, struct layout *lay, struct ws *ws, int opt, struct score *score)/*{{{*/
{
  /* 
   * Deal with this case: suppose the symbols 2,3,5 are unallocated within
   * a group.  Suppose there are three cells
   *   A that could be 2,3
   *   B that could be 2,3
   *   C that could be 3,5
   * and neither 2 nor 3 could go anywhere else within the group under
   * analysis.  Then we can eliminate 3 as an option on C.
   * */

  int NC, NS, NG;
  int did_anything = 0;
  int did_anything_this_iter;
  int i, ci, j, cj;
  short *base;
  char *flags;

  NS = lay->ns;
  NC = lay->nc;
  NG = lay->ng;

  base = lay->groups + gi*NS;
  flags = new_array(char, NS);

  do {
    did_anything_this_iter = 0;
    for (i=0; i<NS; i++) {
      int count, other_count;
      ci = base[i];
      if (!ws->poss[ci]) continue;
      count = 0;
      other_count = 0;
      memset(flags, 0, NS);
      for (j=0; j<NS; j++) {
        cj = base[j];
        if (opt ? (ws->poss[cj]
	            && subset_or_eq_p(ws->poss[cj], ws->poss[ci]))
	        : (ws->poss[ci] == ws->poss[cj])) {
          ++count;
          flags[j] = 1;
        } else if ((ws->poss[cj] & ws->poss[ci]) &&
	           (opt ? !subset_or_eq_p(ws->poss[ci], ws->poss[cj])
		        : (ws->poss[ci] != ws->poss[cj])
		   )) {
          other_count++; /* cells we'll be able to remove possibilities from */
        }
      }
      /* count==1 is a normal allocate done elsewhere! */
      if ((count > 1) && (other_count > 1) && (count == count_bits(ws->poss[ci]))) {
        /* got one. */
        for (j=0; j<NS; j++) {
          cj = base[j];
          if (!flags[j] && (ws->poss[cj] & ws->poss[ci])) {
            did_anything = 1;
            if (score) {
            } else {
              did_anything_this_iter = 1;
              if (ws->options & OPT_VERBOSE) {
                int k, fk;
                fprintf(stderr, "(%c) Removing <", opt ? 'E' : 'e');
                show_symbols_in_set(NS, lay->symbols, ws->poss[cj] & ws->poss[ci]);
                fprintf(stderr, "> from <%s:", lay->cells[cj].name);
                show_symbols_in_set(NS, lay->symbols, ws->poss[cj]);
                fprintf(stderr, ">, because <");
                show_symbols_in_set(NS, lay->symbols, ws->poss[ci]);
                fprintf(stderr, "> must be in <");
                fk = 1;
                for (k=0; k<NS; k++) {
                  int ck = base[k];
                  if (flags[k]) {
                    if (!fk) {
                      fprintf(stderr, ",");
                    }
                    fk = 0;
                    fprintf(stderr, "%s(", lay->cells[ck].name);
                    show_symbols_in_set(NS, lay->symbols,ws->poss[ck]);
                    fprintf(stderr, ")");
                  }
                }
                fprintf(stderr, "> in <%s>\n", lay->group_names[gi]);
              }
              ws->poss[cj] &= ~ws->poss[ci];
              requeue_cell(cj, lay, ws);
              requeue_groups(lay, ws, cj);
            }
          }
        }
      }
    }
  } while (did_anything_this_iter);

  free(flags);
  return did_anything ? 1 : 0;
}
/*}}}*/

static int try_onlyopt(int ic, struct layout *lay, struct ws *ws, int opt, struct score *score)/*{{{*/
{
  int NC;
  int nb;
  NC = lay->nc;
  if (ws->state[ic] < 0) {
    nb = count_bits(ws->poss[ic]);
    if (nb == 0) {
      if (!(ws->options & OPT_SPECULATE)) {
        fprintf(stderr, "Cell <%s> has no options left\n", lay->cells[ic].name);
      }
      return -1;
    } else if (nb == 1) {
      if (score) {
      } else {
        int sym = decode(ws->poss[ic]);
        if (ws->options & OPT_VERBOSE) {
          fprintf(stderr, "(o) Allocate <%c> to <%s> (only option)\n",
              lay->symbols[sym], lay->cells[ic].name);
        }
        --ws->n_todo;
        allocate(lay, ws, 0, ic, sym);
        if (ws->options & OPT_HINT) {
          free_ws(ws);
          free_layout(lay);
          exit(0);
        }
      }
      return 1;
    }
  }
  return 0;
}
/*}}}*/

static int select_minimal_cell(struct layout *lay, int *state, int *poss, int in_overlap)/*{{{*/
{
  int ic;
  int minbits;
  int i;
  minbits = lay->ns + 1;
  ic = -1;
  for (i=0; i<lay->nc; i++) {
    if (!in_overlap || lay->cells[i].is_overlap) {
      if (state[i] < 0) {
        int nb = count_bits(poss[i]);
        if (nb < minbits) {
          minbits = nb;
          ic = i;
        }
      }
    }
  }
  return ic;
}
/*}}}*/
static int speculate(struct layout *lay, struct ws *ws_in)/*{{{*/
{
  /* Called when all else fails and we have to guess a cell but be able to back
   * out the guess if it goes wrong. */
  int ic;
  int start_point;
  int i;
  int NS, NC;
  int *scratch, *solution;
  int n_sol, total_n_sol;
  int n_poss;

  ic = select_minimal_cell(lay, ws_in->state, ws_in->poss, 1);
  if (ic < 0) {
    ic = select_minimal_cell(lay, ws_in->state, ws_in->poss, 0);
  }
  if (ic < 0) {
    return 0;
  }

  NS = lay->ns;
  NC = lay->nc;
  scratch = new_array(int, NC);
  solution = new_array(int, NC);
  start_point = lrand48() % NS;
  total_n_sol = 0;
  n_poss = count_bits(ws_in->poss[ic]);
  for (i=0; i<NS; i++) {
    int ii = (i + start_point) % NS;
    int mask = 1<<ii;
    if (mask & ws_in->poss[ic]) {
      struct ws *ws;
      ws = clone_ws(ws_in);
      memcpy(scratch, ws_in->state, NC * sizeof(int));
      ws->state = scratch;
      --ws->n_todo;
      allocate(lay, ws, 0, ic, ii);
      n_sol = inner_infer(lay, ws);
      if (n_sol > 0) {
        memcpy(solution, scratch, NC * sizeof(int));
        total_n_sol += n_sol;
        ws_in->score += ws->score * (double) n_poss;
        if (ws_in->options & OPT_FIRST_ONLY) {
          free_cloned_ws(lay, ws);
          break;
        }
      }
      if ((ws_in->options & OPT_STOP_ON_2) && (total_n_sol >= 2)) {
        free_cloned_ws(lay, ws);
        break;
      }
      free_cloned_ws(lay, ws);
    }
  }
  free(scratch);
  memcpy(ws_in->state, solution, NC * sizeof(int));
  free(solution);
  return total_n_sol;
}
/*}}}*/

/* ============================================================================ */

static void do_scoring(struct layout *lay, struct ws *ws)/*{{{*/
{
  /* Determine an increment for the score based on how 'hard' the puzzle is to
   * advance at this stage. */

  int n_open_groups, n_open_cells;
  int n_live_resources;
  
  int n_live_alloc, n_live_subsets, n_live_onlyopt, n_live_remote, n_live_near;

  int i, nc, ng;
  struct score score;
  int status, status0;
  double this_score;

  n_live_alloc = 0;
  n_live_subsets = 0;
  n_live_onlyopt = 0;
  n_live_remote = 0;
  n_live_near = 0;
  n_live_resources = 0;

  nc = lay->nc;
  ng = lay->ng;
  n_open_cells = 0;
  n_open_groups = 0;

  score.foo = 0.0;

  this_score = 0.0;
  
  for (i=0; i<nc; i++) {
    if (ws->state[i] < 0) {
      ++n_open_cells;
      status = try_onlyopt(i, lay, ws, 0, &score);
      if (status) {
        ++n_live_onlyopt, ++n_live_resources;
        this_score += 1.0 / 5.0;
      }
    }
  }
  for (i=0; i<ng; i++) {
    if (ws->todo[i] > 0) {
      status = 0;
      ++n_open_groups;
      status |= status0 = try_group_allocate(i, lay, ws, 0, &score);
      if (status0) {
        ++n_live_alloc;
        if (lay->is_block[i]) {
          this_score += 1.0;
        } else {
          this_score += 1.0 / 3.0;
        }
      }
      status |= status0 = try_subsets(i, lay, ws, 0, &score);
      if (status0) {
        ++n_live_subsets;
        this_score += 1.0 / 5.0;
      }
      status |= status0 = try_split_external(i, lay, ws, 0, &score);
      if (status0) {
        ++n_live_remote;
        this_score += 1.0 / 10.0;
      }
      status |= status0 = try_split_internal(i, lay, ws, 0, &score);
      if (status0) {
        ++n_live_near;
        this_score += 1.0 / 10.0;
      }
      if (status) ++n_live_resources;
    }
  }

  if (n_live_resources > 0) {
    this_score = (double)(n_open_cells + n_open_groups) / this_score;
    ws->score += this_score;
  }

  if (ws->options & OPT_VERBOSE) {
    fprintf(stderr, "  OPEN_CELLS=%d, OPEN_GROUPS=%d  LIVE_RESOURCES=%d (a=%d,s=%d,o=%d,r=%d,n=%d), SCORE=%.2f\n",
        n_open_cells, n_open_groups, n_live_resources,
        n_live_alloc, n_live_subsets, n_live_onlyopt, n_live_remote, n_live_near,
        this_score);
  }
}
/*}}}*/

/* ============================================================================ */

static int inner_infer(struct layout *lay, struct ws *ws)/*{{{*/
{
  int NC, NG, NS;
  int result;
  struct queue *q;
  int do_rescore;

  NC = lay->nc;
  NG = lay->ng;
  NS = lay->ns;

  q = ws->base_q;
  result = 0;
  do_rescore = 1;

  while (q) { /* i.e. we still have a queue left to look at */
    struct link *lk;

    if ((ws->options & OPT_SCORE) && do_rescore) {
      /* Only score after some progress has been made. */
      do_scoring(lay, ws);
      do_rescore = 0;
    }

    lk = dequeue(q);
    if (lk) {
      int status;
#if 0
      if (lk->index == 12) {
        fprintf(stderr, "Running %s on %d\n", q->name, lk->index);
      }
#endif
      status = (q->worker)(lk->index, lay, ws, q->opt, NULL);
#if 0
      fprintf(stderr, "  status = %d\n", status);
#endif
      switch (status) {
        case -1:
          goto get_out;
          break;
        case 0:
          /* Try applying a harder rule on this group (if it doesn't get moved
           * back to the simplest queue first.) */
          if (q->next_to_push) {
            enqueue(lk, q->next_to_push);
          }
          break;
        case 1:
          /* If the worker made progress, start scanning from the easiest
           * queue again. */
          if (ws->n_marked_todo == 0) {
            /* If we're not solving a marked puzzle, this field will be -1 */
            result = 1;
            goto get_out;
          }
          q = ws->base_q;
          do_rescore = 1;
          break;
        default:
          fprintf(stderr, "Oh dear, result=%d\n", result);
          exit(2);
      }
    } else {
      /* queue became empty, look at the next one. */
      q = q->next_to_run;
    }
  }

  if (ws->n_todo == 0) {
    if ((ws->options & (OPT_SPECULATE | OPT_SHOW_ALL)) == (OPT_SPECULATE | OPT_SHOW_ALL)) {
      /* ugh, ought to be via an argument */
      static int sol_no = 1;
      printf("Solution %d:\n", sol_no++);
      display(stdout, lay, ws->state);
      printf("\n");
    }
    result = 1;
  } else if (ws->n_todo > 0) {
    /* Didn't get a solution - decide whether to speculate or not. */
    if (ws->options & OPT_SPECULATE) {
      result = speculate(lay, ws);
    } else {
      result = 0;
    }
  } else {
    fprintf(stderr, "n_todo became negative\n");
    exit(1);
  }

get_out:
  return result;
  
}
/*}}}*/
int infer(struct layout *lay, int *state, int *order, int *score, int options)/*{{{*/
{
  int nc, ng, ns;
  struct ws *ws;
  int i;
  int result;

  nc = lay->nc;
  ng = lay->ng;
  ns = lay->ns;

  if (score) {
    options |= OPT_SCORE;
  }

  ws = make_ws(nc, ng, ns);
  ws->solvepos = 0;
  ws->options = options;
  ws->state = state;
  ws->order = order;

  /* Set up work queues */
  {
    struct queue *next_run, *next_cell_push, *next_line_push, *next_block_push, *next_group_push;
    next_run = NULL;
    next_cell_push = NULL;
    next_group_push = NULL;

#if 0
    if (!(options & OPT_NO_SPLIT_INT)) {
      struct queue *our_q = mk_queue(try_split_internal, next_run, next_group_push, 0, "Interior");
      next_run = next_group_push = our_q;
    }
    if (!(options & OPT_NO_SPLIT_EXTX)) {
      struct queue *our_q = mk_queue(try_split_external, next_run, next_group_push, 1, "Exterior_X");
      next_run = next_group_push = our_q;
    }
    if (!(options & OPT_NO_SPLIT_EXT)) {
      struct queue *our_q = mk_queue(try_split_external, next_run, next_group_push, 0, "Exterior");
      next_run = next_group_push = our_q;
    }
#endif
    
    if (!(options & OPT_NO_SPLIT_EXT)) {
      struct queue *our_q = mk_queue(try_partition, next_run, next_group_push, 0, "Partition");
      next_run = next_group_push = our_q;
    }

    if (!(options & OPT_NO_SUBSETS)) {
      struct queue *our_q = mk_queue(try_subsets, next_run, next_group_push, 0, "Subsets");
      next_run = next_group_push = our_q;
    }
    if (!(options & OPT_ONLYOPT_FIRST)) {
      if (!(options & OPT_NO_ONLYOPT)) {
        struct queue *our_q = mk_queue(try_onlyopt, next_run, next_cell_push, 0, "Onlyopt");
        next_run = next_cell_push = our_q;
      }
    }

    /* TODO : eventually, the earlier queues may be split into separate block
     * and line variants. */
    next_block_push = next_group_push;
    next_line_push  = next_group_push;

    if (!(options & OPT_NO_LINES)) {
      struct queue *our_q = mk_queue(try_group_allocate, next_run, next_line_push, 0, "Lines");
      next_run = next_line_push = our_q;
    }
    if (1) { /* allocate in blocks. */
      struct queue *our_q = mk_queue(try_group_allocate, next_run, next_block_push, 0, "Blocks");
      next_run = next_block_push = our_q;
    }

    if (options & OPT_ONLYOPT_FIRST) {
      if (!(options & OPT_NO_ONLYOPT)) {
        struct queue *our_q = mk_queue(try_onlyopt, next_run, next_cell_push, 0, "Onlyopt");
        next_run = next_cell_push = our_q;
      }
    }

    ws->base_q = next_run;
    ws->base_block_q = next_block_push;
    ws->base_line_q = next_line_push;
    ws->base_cell_q = next_cell_push;
  }

  set_base_queues(lay, ws);

  if (options & OPT_SOLVE_MARKED) {
    int i;
    ws->n_marked_todo = 0;
    for (i=0; i<nc; i++) {
      if (ws->state[i] == CELL_MARKED) {
        ++ws->n_marked_todo;
      }
    }
  }
  
  for (i=0; i<nc; i++) {
    if (state[i] >= 0) {
      /* This will clear the poss bits on cells in the same groups, remove todo
       * bits, and enqueue groups that need scanning immediately at the start
       * of the search. */
      allocate(lay, ws, 1, i, state[i]);
    } else {
      ++ws->n_todo;
    }
  }

  result = inner_infer(lay, ws);
  if (score) {
    *score = (int)(0.5 + ws->score);
  }

  free_ws(ws);
  return result;
  
}
/*}}}*/

/* ============================================================================ */
