#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ulas.h"
#include <unistd.h>
#include <getopt.h>

#define ULAS_NAME "ulas"
#define ULAS_VER "0.0.1"

// args without value
#define ULAS_OPTS "hvV"

// args with value
#define ULAS_OPTS_ARG "o:"

#define ULAS_HELP(a, desc) printf("\t-%s\t%s\n", (a), desc);

void ulas_help(void) {
  printf("%s\n", ULAS_NAME);
  printf("Usage %s [-%s] [-o=path] [input]\n\n", ULAS_NAME, ULAS_OPTS);
  ULAS_HELP("h", "display this help and exit");
  ULAS_HELP("V", "display version info and exit");
  ULAS_HELP("v", "verbose output");
  ULAS_HELP("o=path", "Output file");
}

void ulas_version(void) { printf("%s version %s\n", ULAS_NAME, ULAS_VER); }

void ulas_getopt(int argc, char **argv, struct ulas_config *cfg) {
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
      cfg->verbose = true;
      break;
    case 'o':
      cfg->output_path = ulas_strndup(optarg, ULAS_PATHMAX);
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

  int res = ulas_main(cfg);

  return res;
}
