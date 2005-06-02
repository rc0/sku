#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Change this to do a higher-order puzzle!! */
#define N 3
#define NN 9
#define FILL ((1<<NN)-1)

/* Index [block-row][row-in-block][block-col][col-in-block].
 *
 * A zero represents an unassigned cell.
 * A +ve value represents a cell known to have that symbol in it.
 *
 * */
typedef int STATE[N][N][N][N];

static int verbose = 0;

static int count_bits(unsigned int a)
{
  a = (a & 0x55555555) + ((a>>1) & 0x55555555);
  a = (a & 0x33333333) + ((a>>2) & 0x33333333);
  a = (a & 0x0f0f0f0f) + ((a>>4) & 0x0f0f0f0f);
  a = (a & 0x00ff00ff) + ((a>>8) & 0x00ff00ff);
  a = (a & 0x0000ffff) + ((a>>16) & 0x0000ffff);
  return a;
}

static int decode(unsigned int a)
{
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

static int solve(STATE in, int iter)
{
  /* Given a state, try to solve it.
   * Return the number of solutions (possibly with some upper limit?)
   * */

  /* For each unassigned square, a bitmap of possible values. */
  int poss[N][N][N][N];
  int row_todo[N][N];
  int col_todo[N][N];
  int blk_todo[N][N];

  int i, j, m, n;
  int p, q;

  STATE scratch;

  int did_something;
  int any_not_solved;

  /* Pre-fill constraint tables. */
  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      row_todo[i][j] = FILL;
      col_todo[i][j] = FILL;
      blk_todo[i][j] = FILL;
      for (m=0; m<N; m++) {
        for (n=0; n<N; n++) {
          poss[i][j][m][n] = FILL;
        }
      }
    }
  }

back_here:
  iter++;

  any_not_solved = 0;

  /* Discover what's still possible to fit into each empty cell. */
  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      for (m=0; m<N; m++) {
        for (n=0; n<N; n++) {
          int val = in[i][j][m][n];
          if (val > 0) {
            int vm = 1<<(val-1);
#if 0
            fprintf(stderr, "Found %d in cell %d,%d,%d,%d\n", val, i, j, m, n);
#endif
            poss[i][j][m][n] = 0;
            row_todo[i][j] &= ~vm;
            col_todo[m][n] &= ~vm;
            blk_todo[i][m] &= ~vm;
            for (p=0; p<N; p++) {
              for (q=0; q<N; q++) {
                poss[i][j][p][q] &= ~vm; /* Eliminate in same column */ 
                poss[p][q][m][n] &= ~vm; /* Eliminate in same row */ 
                poss[i][p][m][q] &= ~vm; /* Eliminate in same block */ 
              }
            }
          } else {
            any_not_solved = 1;
          }
        }
      }
    }
  }

  if (!any_not_solved) {
    /* Reached a solution. */
    return 1;
  }

  /* Detect empty cells that can't be filled.  (If so, we're stuck, bail out).
   * */
  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      for (m=0; m<N; m++) {
        for (n=0; n<N; n++) {
          int val = in[i][j][m][n];
          if ((val == 0) &&
              (poss[i][j][m][n] == 0)) {
            if (verbose) {
              fprintf(stderr, "-- speculation failed\n");
            }
            return 0;
          }
        }
      }
    }
  }

  did_something = 0;

  /* Look for forced placements : blocks */
  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      int todo = blk_todo[i][j];
      if (todo > 0) {
        int v;
        for (v=0; v<NN; v++) {
          int mask;
          mask = 1<<v;
          if (todo & mask) {
            int count, ixm, ixn;
            count = 0;
            /* Scan column for cells that can take this value. */
            for (m=0; m<N; m++) {
              for (n=0; n<N; n++) {
                if (poss[i][m][j][n] & mask) {
                  ixm = m;
                  ixn = n;
                  count++;
                }
              }
            }
            if (count == 0) {
              /* It's gone pear-shaped. */
              if (verbose) {
                fprintf(stderr, "-- speculation failed\n");
              }
              return 0;
            } else if (count == 1) {
              /* Great - we've solved a square. */
              if (verbose) {
                fprintf(stderr, "Iter %d, setting [row %d][col %d] to %d (block)\n",
                    iter, 3*i+ixm, 3*j+ixn, v+1);
              }
              in[i][ixm][j][ixn] = v+1;
              did_something = 1;
            } else {
              /* No good - there are >=2 squares where this number could go. */
            }
          }
        }
      }
    }
  }

  /* Look for forced placements : rows */
  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      int todo = row_todo[i][j];
      if (todo > 0) {
        int v;
        for (v=0; v<NN; v++) {
          int mask;
          mask = 1<<v;
          if (todo & mask) {
            int count, ixm, ixn;
            /* Scan column for cells that can take this value. */
            count = 0;
            for (m=0; m<N; m++) {
              for (n=0; n<N; n++) {
                if (poss[i][j][m][n] & mask) {
                  ixm = m;
                  ixn = n;
                  count++;
                }
              }
            }
            if (count == 0) {
              /* It's gone pear-shaped. */
              if (verbose) {
                fprintf(stderr, "-- speculation failed\n");
              }
              return 0;
            } else if (count == 1) {
              /* Great - we've solved a square. */
              if (verbose) {
                fprintf(stderr, "Iter %d, setting [row %d][col %d] to %d (row)\n",
                    iter, 3*i+j, 3*ixm+ixn, v+1);
              }
              in[i][j][ixm][ixn] = v+1;
              did_something = 1;
            } else {
              /* No good - there are >=2 squares where this number could go. */
            }
          }
        }
      }
    }
  }

  /* Look for forced placements : columns */
  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      int todo = col_todo[i][j];
      if (todo > 0) {
        int v;
        for (v=0; v<NN; v++) {
          int mask;
          mask = 1<<v;
          if (todo & mask) {
            int count, ixm, ixn;
            count = 0;
            /* Scan column for cells that can take this value. */
            for (m=0; m<N; m++) {
              for (n=0; n<N; n++) {
                if (poss[m][n][i][j] & mask) {
                  ixm = m;
                  ixn = n;
                  count++;
                }
              }
            }
            if (count == 0) {
              /* It's gone pear-shaped. */
              if (verbose) {
                fprintf(stderr, "-- speculation failed\n");
              }
              return 0;
            } else if (count == 1) {
              /* Great - we've solved a square. */
              if (verbose) {
                fprintf(stderr, "Iter %d, setting [row %d][col %d] to %d (column)\n",
                    iter, 3*ixm+ixn, 3*i+j, v+1);
              }
              in[ixm][ixn][i][j] = v+1;
              did_something = 1;
            } else {
              /* No good - there are >=2 squares where this number could go. */
            }
          }
        }
      }
    }
  }

  if (!did_something) {
    /* Discover if there are any values that we can be sure must be in a
     * particular row or column within a block, even if their exact cell can't be
     * determined yet. */

    for (i=0; i<N; i++) {
      for (j=0; j<N; j++) {
        int v;
        /* i,j addresses the block. */
        for (v=0; v<NN; v++) {
          int vm = 1<<v;
          if (blk_todo[i][j] & vm) {
            int rows = 0;
            int cols = 0;
            for (m=0; m<N; m++) {
              for (n=0; n<N; n++) {
                if (poss[i][m][j][n] & vm) {
                  rows |= (1<<m);
                  cols |= (1<<n);
                }
              }
            }
            if (count_bits(rows) == 1) {
              int any = 0;
              m = decode(rows);
              for (p=0; p<N; p++) {
                if (p == j) continue;
                for (q=0; q<N; q++) {
                  if (poss[i][m][p][q] & vm) {
                    did_something = 1;
                    poss[i][m][p][q] &= ~vm;
                    any = 1;
                  }
                }
              }
              if (any) {
                if (verbose) {
                  fprintf(stderr, "Iter %d eliminating %d from row %d,%d (due to block %d,%d)\n",
                      iter, v+1, i, m, i, j);
                }
              }
            }
            if (count_bits(cols) == 1) {
              int any = 0;
              m = decode(cols);
              for (p=0; p<N; p++) {
                if (p == i) continue;
                for (q=0; q<N; q++) {
                  if (poss[p][q][j][m] & vm) {
                    did_something = 1;
                    poss[p][q][j][m] &= ~vm;
                    any = 1;
                  }
                }
              }
              if (any) {
                if (verbose) {
                  fprintf(stderr, "Iter %d eliminating %d from col %d,%d (due to block %d,%d)\n",
                      iter, v+1, j, m, i, j);
                }
              }
            }
          }
        }
      }
    }
  }

  if (did_something > 0) {
    goto back_here;
  } else {
    /* Couldn't work it out - have to take a guess and go around again. */
    for (i=0; i<N; i++) {
      for (j=0; j<N; j++) {
        for (m=0; m<N; m++) {
          for (n=0; n<N; n++) {
            int possible = poss[i][j][m][n];
            if (possible != 0) {
              int n_solutions = 0;
              STATE solution;
              int v, mask;
              for (v=0; v<NN; v++) {
                mask = 1<<v;
                if (possible & mask) {
                  int n_sol;
                  memcpy(scratch, in, sizeof(STATE));
                  if (verbose) {
                    fprintf(stderr, "Speculating %d,%d,%d,%d is %d\n", i, j, m, n, v+1);
                  }
                  scratch[i][j][m][n] = v+1;
                  n_sol = solve(scratch, iter);
                  if (n_sol > 0) {
                    memcpy(solution, scratch, sizeof(STATE));
                    n_solutions += n_sol;
                  }
                }
              }
              memcpy(in, solution, sizeof(STATE));
              return n_solutions;
            }
          }
        }
      }
    }
    if (verbose) {
      fprintf(stderr, "-- speculation failed\n");
    }
    return 0;
  }
}

int main (int argc, char **argv) {
  STATE state;
  int i, j, m, n;
  int n_solutions;

  if (argc > 1) {
    if (!strcmp(argv[1], "-v")) {
      verbose = 1;
    }
  }

  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      for (m=0; m<N; m++) {
        for (n=0; n<N; n++) {
          int c;
          int valid = 0;
          do {
            c = getchar();
            switch (c) {
              case '.': state[i][j][m][n] = 0; valid = 1; break;
              case '1': state[i][j][m][n] = 1; valid = 1; break;
              case '2': state[i][j][m][n] = 2; valid = 1; break;
              case '3': state[i][j][m][n] = 3; valid = 1; break;
              case '4': state[i][j][m][n] = 4; valid = 1; break;
              case '5': state[i][j][m][n] = 5; valid = 1; break;
              case '6': state[i][j][m][n] = 6; valid = 1; break;
              case '7': state[i][j][m][n] = 7; valid = 1; break;
              case '8': state[i][j][m][n] = 8; valid = 1; break;
              case '9': state[i][j][m][n] = 9; valid = 1; break;
              case EOF:
                fprintf(stderr, "Ran out of input data!\n");
                exit(1);
              default: valid = 0; break;
            }
          } while (!valid);
        }
      }
    }
  }


  n_solutions = solve(state, 0);
  if (n_solutions == 0) {
    fprintf(stderr, "The puzzle had no solutions\n");
    return 0;
  } else if (n_solutions == 1) {
    fprintf(stderr, "The puzzle had precisely 1 solution\n");

  } else {
    fprintf(stderr, "The puzzle had %d solutions (one is shown)\n", n_solutions);
  }

  for (i=0; i<N; i++) {
    for (j=0; j<N; j++) {
      for (m=0; m<N; m++) {
        for (n=0; n<N; n++) {
          fprintf(stderr, "%1d", state[i][j][m][n]);
        }
        fprintf(stderr, " ");
      }
      fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
  }

  return 0;
}


