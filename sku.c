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

#include <time.h>

#include "sku.h"

/* ============================================================================ */


static void usage(void)
{
  fprintf(stderr,
      "General options:\n"
      "  -v          : verbose\n"
      "\n"
      "With no option, solve a puzzle\n"
      "  -f          : if puzzle has >1 solution, only find the first\n"
      "  -o          : apply 'only option' rule first\n"
      "  -s          : use speculation if logic fails to complete the grid\n"
      "  -A          : show all solutions if using speculation and the puzzle is ambiguous\n"
      "  -M          : find a minimal solution (for puzzles with marked cells)\n"
      "\n"
      "-b<layout>    : create a blank grid with named <layout>\n"
      "\n"
      "-H            : provide a hint (show the next step in the solution of a partial grid)\n"
      "\n"
      "-a            : complete a grid\n"
      "\n"
      "-g            : grade the difficulty of a puzzle\n"
      "\n"
      "-r            : reduce to minimum no. of givens\n"
      "  -E          : generate an 'easy' puzzle (only needs allocate within blocks)\n"
      "  -Ee         : don't look for splits (exterior removals)\n"
      "  -Ei         : don't look for splits (interior removals)\n"
      "  -El         : don't do allocation along lines (only within blocks)\n"
      "  -Eo         : don't look for squares with only one option left\n"
      "  -Es         : don't do subset analysis\n"
      "  -m<number>  : try <number> times to find a puzzle with a smallest number of givens\n"
      "  -s          : allow solutions that require speculation to solve\n"
      "  -t          : allow puzzles with < 2 unknowns in a group\n"
      "  -y          : require 180 degree rotational symmetry\n"
      "  -yy         : require 90,180,270 degree rotational symmetry\n"
      "  -yh         : require horizontal reflective symmetry\n"
      "  -yv         : require vertical reflective symmetry\n"
      "\n"
      "-k<number>    : mark <number> empty squares in grey\n"
      "\n"
      "-F            : format output as SVG\n"
      );
}

/* ============================================================================ */

#if 0
static void apply_level(const char *level, int *options, int *reduce_req_n)/*{{{*/
{
  int lo, hi;
  const int tab_a[6] = {
    OPT_MAKE_EASIER,
    OPT_NO_SUBSETS | OPT_NO_ONLYOPT | OPT_NO_SPLIT_EXT | OPT_NO_SPLIT_INT,
    OPT_NO_ONLYOPT | OPT_NO_SPLIT_EXT | OPT_NO_SPLIT_INT,
    OPT_NO_SPLIT_EXT | OPT_NO_SPLIT_INT,
    OPT_NO_SPLIT_INT,
    0
  };
  const int tab_b[6] = {
    OPT_MAKE_EASIER,
    OPT_NO_LINES | OPT_NO_SUBSETS | OPT_NO_SPLIT_EXT | OPT_NO_SPLIT_INT,
    OPT_NO_SUBSETS | OPT_NO_SPLIT_EXT | OPT_NO_SPLIT_INT,
    OPT_NO_SPLIT_EXT | OPT_NO_SPLIT_INT,
    OPT_NO_SPLIT_INT,
    0
  };

  if (level[0]) {
    lo = level[0] - '0';
    if (level[1]) {
      hi = level[1] - '0';
      if (lo > hi) {
        int tmp = hi;
        hi = lo;
        lo = tmp;
      }
    } else {
      hi = lo;
    }
    if (lo < 0 || lo > 5 || hi < 0 || hi > 5) {
      fprintf(stderr, "Arguments to -L out of range\n");
      exit(1);
    }
  } else {
    fprintf(stderr, "No argument given to -L\n");
    exit(1);
  }

  *options |= ((*options & OPT_ONLYOPT_FIRST) ? tab_b : tab_a)[lo];
  *reduce_req_n |= ((*options & OPT_ONLYOPT_FIRST) ? tab_b : tab_a)[hi] ^ OPT_MAKE_EASIER;

}
/*}}}*/
#endif

/* ============================================================================ */

int main (int argc, char **argv)/*{{{*/
{
  int options;
  int seed;
  int iters_for_min = 0;
  int grey_cells = 0;
  enum operation {
    OP_BLANK,     /* Generate a blank grid */
    OP_ANY,       /* Generate any solution to a partial grid */
    OP_REDUCE,    /* Remove givens until it's no longer possible without
                     leaving an ambiguous puzzle. */
    OP_SOLVE,
    OP_MARK,
    OP_GRADE,
    OP_HINT,
    OP_FORMAT,
    OP_TIDY
  } operation;
  char *layout_name = NULL;
  struct constraint simplify_cons, required_cons;
  
  operation = OP_SOLVE;

  options = 0;
  required_cons = cons_none;
  simplify_cons = cons_all;

  while (++argv, --argc) {
    if (!strcmp(*argv, "-h") || !strcmp(*argv, "-help") || !strcmp(*argv, "--help")) {
      usage();
      exit(0);
    } else if (!strcmp(*argv, "-a")) {
      operation = OP_ANY;
    } else if (!strcmp(*argv, "-A")) {
      options |= OPT_SHOW_ALL;
    } else if (!strncmp(*argv, "-b", 2)) {
      operation = OP_BLANK;
      layout_name = *argv + 2;
    } else if (!strncmp(*argv, "-E", 2)) {
      if ((*argv)[2] == 0) {
        simplify_cons = cons_none;
        simplify_cons.is_default = 0;
      } else {
        const char *p = 2 + *argv;
        simplify_cons.is_default = 0;
        while (*p) {
          switch (*p) {
            case 'l': simplify_cons.do_lines = 0; break;
            case 'o': simplify_cons.do_onlyopt = 0; break;
            case 's': simplify_cons.do_subsets = 0; break;
            case '2': simplify_cons.max_partition_size = 0; break;
            case '3': simplify_cons.max_partition_size = 2; break;
            case '4': simplify_cons.max_partition_size = 3; break;
            case '5': simplify_cons.max_partition_size = 4; break;
            default: fprintf(stderr, "Can't use %c with -E\n", *p);
              break;
          }
          p++;
        }
      }
    } else if (!strcmp(*argv, "-f")) {
      options |= OPT_FIRST_ONLY;
    } else if (!strcmp(*argv, "-g")) {
      operation = OP_GRADE;
    } else if (!strcmp(*argv, "-F")) {
      operation = OP_FORMAT;
    } else if (!strcmp(*argv, "-H")) {
      operation = OP_HINT;
    } else if (!strncmp(*argv, "-k", 2)) {
      operation = OP_MARK;
      if ((*argv)[2] == 0) {
        grey_cells = 4;
      } else {
        grey_cells = atoi(*argv + 2);
      }
#if 0
    } else if (!strncmp(*argv, "-L", 2)) {
      apply_level(*argv + 2, &options, &reduce_req_n);
#endif
    } else if (!strncmp(*argv, "-m", 2)) {
      iters_for_min = atoi(*argv + 2);
    } else if (!strcmp(*argv, "-M")) {
      options |= OPT_SOLVE_MINIMAL;
    } else if (!strcmp(*argv, "-o")) {
      options |= OPT_ONLYOPT_FIRST;
    } else if (!strcmp(*argv, "-r")) {
      operation = OP_REDUCE;
    } else if (!strncmp(*argv, "-R", 2)) {
      if ((*argv)[2] == 0) {
        required_cons = cons_all;
        required_cons.is_default = 0;
      } else {
        const char *p = 2 + *argv;
        required_cons.is_default = 0;
        while (*p) {
          switch (*p) {
            case 'l': required_cons.do_lines = 1; break;
            case 'o': required_cons.do_onlyopt = 1; break;
            case 's': required_cons.do_subsets = 1; break;
            case '2': required_cons.max_partition_size = 2; break;
            case '3': required_cons.max_partition_size = 3; break;
            case '4': required_cons.max_partition_size = 4; break;
            case '5': required_cons.max_partition_size = 5; break;
            default: fprintf(stderr, "Can't use %c with -R\n", *p);
              break;
          }
          p++;
        }
      }
    } else if (!strcmp(*argv, "-s")) {
      options |= OPT_SPECULATE;
    } else if (!strcmp(*argv, "-t")) {
      options |= OPT_ALLOW_TRIVIAL;
    } else if (!strcmp(*argv, "-T")) {
      operation = OP_TIDY;
    } else if (!strcmp(*argv, "-v")) {
      options |= OPT_VERBOSE;
    } else if (!strcmp(*argv, "-y")) {
      options |= OPT_SYM_180;
    } else if (!strcmp(*argv, "-yy")) {
      options |= OPT_SYM_180 | OPT_SYM_90;
    } else if (!strcmp(*argv, "-yh")) {
      options |= OPT_SYM_HORIZ;
    } else if (!strcmp(*argv, "-yv")) {
      options |= OPT_SYM_VERT;
    } else {
      fprintf(stderr, "Unrecognized argument <%s>\n", *argv);
      exit(1);
    }
  }
  
  seed = time(NULL) ^ getpid();
  if (options & OPT_VERBOSE) {
    fprintf(stderr, "Seed=%d\n", seed);
  }
  srand48(seed);
  switch (operation) {
    case OP_SOLVE:
      solve(&simplify_cons, options);
      break;
    case OP_HINT:
      solve(&simplify_cons, options | OPT_HINT | OPT_VERBOSE);
      break;
    case OP_ANY:
      solve_any(options);
      break;
    case OP_REDUCE:
      reduce(iters_for_min, &simplify_cons, &required_cons, options);
      break;
    case OP_BLANK:
      {
        struct layout *lay;
        lay = genlayout(*layout_name ? layout_name : "3", options);
        blank(lay);
        free_layout(lay);
        break;
      }
    case OP_GRADE:
      grade(options);
      break;
    case OP_MARK:
      mark_cells(grey_cells, &simplify_cons, options);
      break;
    case OP_FORMAT:
      format_output(options);
      break;
    case OP_TIDY:
      tidy(options);
      break;
  }
  return 0;
}
/*}}}*/

/* ============================================================================ */

