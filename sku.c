
/* TODO:
 * symmetrical removal of givens in pose mode
 * tidy up verbose output
 * hint mode : advance to the next allocate in a part-solved puzzle
 * 5-gattai puzzles and generalisations thereof
 * analyze where pose mode spends its time.
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

/* ============================================================================ */

int main (int argc, char **argv)/*{{{*/
{
  int options;
  int reduce_req_n;
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
    OP_FORMAT
  } operation;
  char *layout_name = NULL;
  
  operation = OP_SOLVE;

  options = 0;
  reduce_req_n = 0;

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
        options |= OPT_MAKE_EASIER;
      } else {
        const char *p = 2 + *argv;
        while (*p) {
          switch (*p) {
            case 'e': options |= OPT_NO_SPLIT_EXT;  break;
            case 'i': options |= OPT_NO_SPLIT_INT;    break;
            case 'l': options |= OPT_NO_LINES;   break;
            case 'o': options |= OPT_NO_ONLYOPT; break;
            case 's': options |= OPT_NO_SUBSETS; break;
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
    } else if (!strncmp(*argv, "-L", 2)) {
      apply_level(*argv + 2, &options, &reduce_req_n);
    } else if (!strncmp(*argv, "-m", 2)) {
      iters_for_min = atoi(*argv + 2);
    } else if (!strcmp(*argv, "-o")) {
      options |= OPT_ONLYOPT_FIRST;
    } else if (!strcmp(*argv, "-r")) {
      operation = OP_REDUCE;
    } else if (!strncmp(*argv, "-R", 2)) {
      if ((*argv)[2] == 0) {
        reduce_req_n = OPT_MAKE_EASIER;
      } else {
        const char *p = 2 + *argv;
        while (*p) {
          switch (*p) {
            case 'e': reduce_req_n |= OPT_NO_SPLIT_EXT;  break;
            case 'i': reduce_req_n |= OPT_NO_SPLIT_INT;    break;
            case 'l': reduce_req_n |= OPT_NO_LINES;   break;
            case 'o': reduce_req_n |= OPT_NO_ONLYOPT; break;
            case 's': reduce_req_n |= OPT_NO_SUBSETS; break;
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

  if (options & reduce_req_n) {
    fprintf(stderr, "You cannot exclude methods and require them too!\n");
    exit(1);
  }
  
  seed = time(NULL) ^ getpid();
  if (options & OPT_VERBOSE) {
    fprintf(stderr, "Seed=%d\n", seed);
  }
  srand48(seed);
  switch (operation) {
    case OP_SOLVE:
      solve(options);
      break;
    case OP_HINT:
      solve(options | OPT_HINT | OPT_VERBOSE);
      break;
    case OP_ANY:
      solve_any(options);
      break;
    case OP_REDUCE:
      reduce(iters_for_min, options, reduce_req_n);
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
      mark_cells(grey_cells, options);
      break;
    case OP_FORMAT:
      format_output(options);
      break;
  }
  return 0;
}
/*}}}*/

/* ============================================================================ */

