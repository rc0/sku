
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
      "  -v                 : verbose\n"
      "\n"
      "With no option, solve a puzzle\n"
      "  -f                 : if puzzle has >1 solution, only find the first\n"
      "  -s                 : use speculation if logic fails to complete the grid\n"
      "\n"
      "-b<layout>           : create a blank grid with named <layout>\n"
      "\n"
      "-a                   : complete a grid\n"
      "\n"
      "-r                   : reduce to minimum no. of givens\n"
      "  -Eb                : only do allocation on rectangles (not rows & columns)\n"
      "  -Es                : don't do subsetting analysis to remove possibilities\n"
      "  -Eu                : don't handle squares with a unique symbol left\n"
      "  -y                 : require 180 degree rotational symmetry\n"
      "  -yy                : require 90,180,270 degree rotational symmetry\n"
      "  -s                 : allow solutions that require speculation to solve\n"
      "  -m<number>         : try <number> times to find a puzzle with a smallest number of givens\n"
      "\n"
      "-k<number>           : mark <number> empty squares in grey\n"
      "\n"
      "-F                   : format output as SVG\n"
      );
}

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
    OP_FORMAT
  } operation;
  char *layout_name = NULL;
  
  operation = OP_SOLVE;

  options = 0;
  while (++argv, --argc) {
    if (!strcmp(*argv, "-h") || !strcmp(*argv, "-help") || !strcmp(*argv, "--help")) {
      usage();
      exit(0);
    } else if (!strcmp(*argv, "-a")) {
      operation = OP_ANY;
    } else if (!strncmp(*argv, "-b", 2)) {
      operation = OP_BLANK;
      layout_name = *argv + 2;
    } else if (!strcmp(*argv, "-Eb")) {
      options |= OPT_NO_ROWCOL_ALLOC;
    } else if (!strcmp(*argv, "-Es")) {
      options |= OPT_NO_SUBSETTING;
    } else if (!strcmp(*argv, "-Eu")) {
      options |= OPT_NO_UNIQUES;
    } else if (!strcmp(*argv, "-f")) {
      options |= OPT_FIRST_ONLY;
    } else if (!strcmp(*argv, "-F")) {
      operation = OP_FORMAT;
    } else if (!strncmp(*argv, "-k", 2)) {
      operation = OP_MARK;
      if ((*argv)[2] == 0) {
        grey_cells = 4;
      } else {
        grey_cells = atoi(*argv + 2);
      }
    } else if (!strncmp(*argv, "-m", 2)) {
      iters_for_min = atoi(*argv + 2);
    } else if (!strcmp(*argv, "-r")) {
      operation = OP_REDUCE;
    } else if (!strcmp(*argv, "-s")) {
      options |= OPT_SPECULATE;
    } else if (!strcmp(*argv, "-v")) {
      options |= OPT_VERBOSE;
    } else if (!strcmp(*argv, "-y")) {
      options |= OPT_SYM_180;
    } else if (!strcmp(*argv, "-yy")) {
      options |= OPT_SYM_180 | OPT_SYM_90;
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
      solve(options);
      break;
    case OP_ANY:
      solve_any(options);
      break;
    case OP_REDUCE:
      reduce(iters_for_min, options);
      break;
    case OP_BLANK:
      {
        struct layout *lay;
        lay = genlayout(*layout_name ? layout_name : "3");
        blank(lay);
        break;
      }
#if 0
    case OP_DISCOVER:
      discover(options);
      break;
#endif
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

