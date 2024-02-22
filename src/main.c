#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ulas.h"
#include <unistd.h>
#include <getopt.h>

/**
 * TODO: Implement -d flag to disassemble input file
 * TODO: Add symbol file output
 * TODO: Write documentation
 * TODO: process defined variables inside defines correctly
 * TODO: Implement struct, union and enum syntax
 * TODO: Add warning levels such as: warning when literal is too large for
 * register with -w syntax
 */

#define ULAS_NAME "ulas"
#define ULAS_VER "0.0.1"

// args without value
#define ULAS_OPTS "hvVpd"

// args with value
#define ULAS_OPTS_ARG "o:l:s:i:w:a:"

#define ULAS_HELP(a, desc) printf("\t-%s\t%s\n", (a), desc);

#define ULAS_INCPATHSMAX 256

char *incpaths[ULAS_INCPATHSMAX];
unsigned long incpathslen = 0;

void ulas_help(void) {
  printf("%s\n", ULAS_NAME);
  printf("Usage %s [-%s] [-o=path] [-i=path] [-l=path] [-a=initial-address] "
         "[input]\n\n",
         ULAS_NAME, ULAS_OPTS);
  ULAS_HELP("h", "display this help and exit");
  ULAS_HELP("V", "display version info and exit");
  ULAS_HELP("v", "verbose output");
  ULAS_HELP("p", "Stop after preprocessor");
  ULAS_HELP("o=path", "Output file");
  ULAS_HELP("l=path", "Listing file");
  ULAS_HELP("s=path", "Symbols file");
  ULAS_HELP("i=path", "Add include search path");
  ULAS_HELP("a=initial-address", "Initial starting address");
  ULAS_HELP("d", "Disassemble a file");
  ULAS_HELP("w=warning", "Toggle warnings: a=all, o=overflow");
}

void ulas_version(void) { printf("%s version %s\n", ULAS_NAME, ULAS_VER); }

void ulas_getopt(int argc, char **argv, struct ulas_config *cfg) {
  int warnings[255];
  memset(warnings, 0, 255 * sizeof(int));
  warnings['a'] = ULAS_WARN_ALL;
  warnings['o'] = ULAS_WARN_OVERFLOW;

  int c = 0;
  while ((c = getopt(argc, argv, ULAS_OPTS ULAS_OPTS_ARG)) != -1) {
    switch (c) {
    case 'h':
      ulas_help();
      exit(0);
      break;
    case 'V':
      ulas_version();
      exit(0);
      break;
    case 'v':
      cfg->verbose = 1;
      break;
    case 'o':
      cfg->output_path = strndup(optarg, ULAS_PATHMAX);
      break;
    case 'p':
      cfg->preproc_only = 1;
      break;
    case 's':
      cfg->sym_path = strndup(optarg, ULAS_PATHMAX);
      break;
    case 'l':
      cfg->lst_path = strndup(optarg, ULAS_PATHMAX);
      break;
    case 'i':
      assert(incpathslen < ULAS_INCPATHSMAX);
      incpaths[incpathslen++] = strndup(optarg, ULAS_PATHMAX);
      break;
    case 'a':
      cfg->org = strtol(optarg, NULL, 0);
      break;
    case 'w':
      cfg->warn_level ^= warnings[(int)optarg[0]];
      break;
    case 'd':
      cfg->disas = 1;
      break;
    case '?':
      break;
    default:
      printf("%s: invalid option '%c'\nTry '%s -h' for more information.\n",
             ULAS_NAME, c, ULAS_NAME);
      exit(-1);
      break;
    }
  }

  cfg->argc = argc - optind;
  cfg->argv = argv + optind;
}

int main(int argc, char **argv) {
  // map args to cfg here
  struct ulas_config cfg = ulas_cfg_from_env();

  ulas_getopt(argc, argv, &cfg);
  cfg.incpaths = incpaths;
  cfg.incpathslen = incpathslen;

  int res = ulas_main(cfg);

  if (cfg.output_path) {
    free(cfg.output_path);
  }

  if (cfg.sym_path) {
    free(cfg.sym_path);
  }

  if (cfg.lst_path) {
    free(cfg.lst_path);
  }

  for (int i = 0; i < incpathslen; i++) {
    free(incpaths[i]);
  }

  return res;
}
