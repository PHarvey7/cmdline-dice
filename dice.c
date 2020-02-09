/** dice.c
 *  Simple command-line dice roller
 *  (c) 2020 Patrick Harvey [see LICENSE.txt] 
 */

#ifdef _WIN32
#define _CRT_RAND_S
#include <malloc.h>
#endif
#include "dice.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void print_error(char* message) {
  printf("ERROR: %s\n", message);
}

/** Initializes the random generator. Should be called once per program invocation.
 *  Currently using the random device to seed, freeing it from macro-scale time
 *  dependencies from the previous approach. */
void init_random() {
#ifndef _WIN32
  srandomdev();
#endif
}

/** Gets a random number from the generator. Using BSD random() currently to go with
 *  srandomdev in init_random. Windows uses rand_s which has better performance than rand.*/
int get_next_random() {
#ifdef _WIN32
  unsigned int num;
  errno_t e = rand_s(&num);
  if (e) {
    print_error("Failed to get number from PRNG");
    exit(1);
  }
  // rand_s can return negative numbers when cast to int; so convert after casting
  int snum = (int) num;
  return snum < 0 ? snum*-1 : snum;
#else
  return (int) random();
#endif
}

/** Frees a modifier node in the parse tree. Safe to call on null references. */
void free_modifier_node(RollModifier* mod) {
  if (mod != NULL) {
    free(mod);
  }
}

/** Frees a roll node in the parse tree, including any descendants. Safe to call on null references. */
void free_roll_node(RollNode* roll) {
  if (roll != NULL) {
    free_modifier_node(roll->rollMod);
    free(roll);
  }
}

/** Frees an object node in the parse tree, including any descendants. Safe to call on null references. */
void free_obj_node(ObjNode* obj) {
  if (obj != NULL) {
    free_roll_node(obj->roll);
    free_expr_node(obj->subList);
    free(obj);
  }
}

/** Frees an expression node in the parse tree, including any descendants. Safe to call on null references. */
void free_expr_node(ExprList* list) {
  if (list != NULL) {
    free_obj_node(list->obj);
    free_expr_node(list->lhList);
    free_expr_node(list->rhList);
    free(list);
  }
}

/** Locates the first operator not nested within a deeper expression in the selected substring of input. */
char* find_first_free_opt(char* inp, int len, char* opchars) {
  int parensDepth = 0;
  char* scan = strpbrk(inp, opchars);
  while (scan && ((scan - inp) < len)) {
    if (*scan == '(') {
      parensDepth++;
    } else if (*scan == ')') {
      parensDepth--;
      if (parensDepth < 0) {
	print_error("Mismatched parentheses.");
	return "?";
      }
    } else {
      if (parensDepth == 0) {
	return scan;
      }
    }
    scan = strpbrk(scan+1, opchars);
  }
  if (parensDepth != 0) {
    print_error("Mismatched parentheses.");
    return "?";
  }
  return NULL;
}

/* Parse an expr_list, recursively. inp should be the string starting at the
   beginning of the expr_list, not including any leading open parentheses. 
   Len should be the length of the string the expr_list should be concerned 
   with, i.e. either strlen(inp) or (strchr(inp, ')') - inp). */
ExprList* parse_expr(char* inp, int len, OpType type) {
  if (len <= 0) {
    print_error("Missing Object.");
    return NULL;
  }

  char* opchars = "()+-";
  if (type == M_OP) {
    opchars = "()*/";
  }

  //Is there an operator? If so, form is obj opt expr_list, if not just obj
  char* first_opt = find_first_free_opt(inp, len, opchars);

  if (first_opt) {
    ObjNode* obj = NULL;
    ExprList* lhExpr = NULL;
    if (type == M_OP) {
      obj = parse_obj(inp, first_opt - inp);
    } else if (type == A_OP) {
      lhExpr = parse_expr(inp, first_opt - inp, M_OP); 
    }
    Operation opt = parse_operator(first_opt);
    ExprList* subExpr = parse_expr(first_opt + 1, len - ((first_opt - inp) + 1), type);
    //Successful parse?
    if ((obj == NULL && lhExpr == NULL) || opt == NOOP || subExpr == NULL) {
      free_obj_node(obj);
      free_expr_node(lhExpr);
      free_expr_node(subExpr);
      return NULL;
    }
    //Make the node
    ExprList* this_expr = malloc(sizeof(ExprList));
    if (this_expr == NULL) {
      free_obj_node(obj);
      free_expr_node(lhExpr);
      free_expr_node(subExpr);
      print_error("Out of memory.");
      return NULL;
    }
    this_expr->lhList = lhExpr;
    this_expr->obj = obj;
    this_expr->opt = opt;
    this_expr->rhList = subExpr;
    return this_expr;
  } else {
    ObjNode* obj = NULL;
    ExprList* lhNode = NULL;
    if (type == M_OP) {
      obj = parse_obj(inp, len);
    } else {
      lhNode = parse_expr(inp, len, M_OP);
    }
    //Successful parse?
    if (obj == NULL && lhNode == NULL) {
      return NULL;
    }
    //Make the node
    ExprList* this_expr = malloc(sizeof(ExprList));
    if (this_expr == NULL) {
      free_obj_node(obj);
      free_expr_node(lhNode);
      print_error("Out of memory.");
      return NULL;
    }
    this_expr->lhList = lhNode;
    this_expr->obj = obj;
    this_expr->opt = NOOP;
    this_expr->rhList = NULL;
    return this_expr;
  }
}

ObjNode* parse_obj(char* inp, int len) {
  if (len <= 0) {
    print_error("Missing Object.");
    return NULL;
  }

  //Determine what type of object this is.
  if (*inp == '(') {
    //It is ( expr_list ).
    if (inp[len-1] != ')') {
      print_error("Mismatched parentheses.");
      return NULL;
    }
    ExprList* subExpr = parse_expr(inp+1, len-2, A_OP);
    if (subExpr == NULL) {
      return NULL;
    }
    ObjNode* this_obj = malloc(sizeof(ObjNode));
    if (this_obj == NULL) {
      free_expr_node(subExpr);
      print_error("Out of memory.");
      return NULL;
    }
    this_obj->roll = NULL;
    this_obj->constant = 0;
    this_obj->subList = subExpr;
    return this_obj;
  } else if (strspn(inp, "0123456789") == len) {
    //It is a constant.
    ObjNode* this_obj = malloc(sizeof(ObjNode));
    if (this_obj == NULL) {
      print_error("Out of memory.");
      return NULL;
    }
    this_obj->roll = NULL;
    this_obj->subList = NULL;
    STACK_ALLOC(char, numbuf, len+1);
    strncpy(numbuf, inp, len);
    numbuf[len] = '\0';
    this_obj->constant = atoi(numbuf);
    return this_obj;
  } else {
    //It must be a roll (if it's badly formatted, will be caught further down the line).
    RollNode* roll = parse_roll(inp, len);
    if (roll == NULL) {
      return NULL;
    }
    ObjNode* this_obj = malloc(sizeof(ObjNode));
    if (this_obj == NULL) {
      free_roll_node(roll);
      print_error("Out of memory.");
      return NULL;
    }
    this_obj->roll = roll;
    this_obj->constant = 0;
    this_obj->subList = NULL;
    return this_obj;
  }
}

/** Parses a single 'roll' (i.e. XdY[(modifier)Z]), reporting error and returning null if something is wrong. */
RollNode* parse_roll(char* inp, int len) {
  if (len <= 0) {
    print_error("Missing Roll.");
    return NULL;
  }
  char* d_loc = strchr(inp, 'd');
  if ((!d_loc) || ((d_loc - inp) >= len)) {
    print_error("Garbled roll (no 'd' delimiter).");
    return NULL;
  }

  char* mod_loc = strpbrk(inp, "cbvw");
  int mod_chars = 0;
  RollModifier* mod = NULL;
  if (mod_loc && ((mod_loc - inp) < len)) {
    mod_chars = len - (mod_loc - inp);
    mod = parse_modifier(mod_loc, mod_chars);
    if (mod == NULL) {
      return NULL;
    }
  }
  
  int a_chars = d_loc - inp;
  int b_chars = len - ((d_loc - inp) + 1 + mod_chars);

  STACK_ALLOC(char, constA, a_chars+1);
  STACK_ALLOC(char, constB, b_chars+1);

  strncpy(constA, inp, a_chars);
  strncpy(constB, d_loc+1, b_chars);
  constA[a_chars] = '\0';
  constB[b_chars] = '\0';

  int a_char_len = strlen(constA);
  int b_char_len = strlen(constB);

  if (a_char_len == 0 ||
      b_char_len == 0) {
    print_error("Missing constant.");
    free_modifier_node(mod);
    return NULL;
  }

  int a_nums_len = strspn(constA, "0123456789");
  int b_nums_len = strspn(constB, "0123456789");
  
  if ((a_nums_len != a_char_len) ||
      (b_nums_len != b_char_len)) {
    print_error("Invalid constant.");
    free_modifier_node(mod);
    return NULL;
  }

  RollNode* this_roll = malloc(sizeof(RollNode));
  if (this_roll == NULL) {
    free_modifier_node(mod);
    print_error("Out of memory.");
    return NULL;
  }

  this_roll->dieCount = atoi(constA);
  this_roll->dieSides = atoi(constB);
  this_roll->rollMod = mod;
  return this_roll;
}

/** Parses a roll modifier, reporting an error and returning null if something is wrong.  */
RollModifier* parse_modifier(char* inp, int len) {
  if (len <= 0) {
    print_error("Missing Modifier.");
    return NULL;
  }

  ModifierType type = NONE;

  switch(inp[0]) {
  case 'c':
    type = CHOOSE_HIGH;
    break;
  case 'b':
    type = REROLL_BELOW;
    break;
  case 'v':
    type = KEEP_AND_REROLL_ABOVE;
    break;
  case 'w':
    type = CHOOSE_LOW;
    break;
  default:
    print_error("Invalid Modifier Character.");
    return NULL;
  }

  if (len < 2) {
    print_error("Missing Modifier Constant.");
    return NULL;
  }

  STACK_ALLOC(char, constant, len);
  strncpy(constant, inp+1, len-1);
  constant[len-1] = '\0';
  int value = atoi(constant);

  RollModifier* mod = malloc(sizeof(RollModifier));
  if (mod == NULL) {
    print_error("Out of memory.");
    return NULL;
  }
  mod->type = type;
  mod->constant = value;
  return mod;
}

/** Parses a single arithmetic operator character. */
Operation parse_operator(char* inp) {
  switch(*inp) {
  case '+':
    return PLUS;
  case '-':
    return MINUS;
  case '*':
    return TIMES;
  case '/':
    return DIVIDE;
  default:
    return NOOP;
  }
}

/** Returns true iff the expression does not contain further lists or operators. */
bool expr_is_singlet(ExprList* expr) {
  return (expr->rhList == NULL && expr->opt == NOOP); 
}

/** Execute an expression, including rolling contained die rolls as appropriate. 
 *  To be called on an ExprList after building the parse tree. */
int execute_expr(ExprList* expr, bool verbose) {
  int lhResult;
  if (expr->lhList != NULL) {
    lhResult = execute_expr(expr->lhList, verbose);
  } else {
    lhResult = execute_obj(expr->obj, verbose);
  }
  
  if (expr_is_singlet(expr)) {
    return lhResult;
  } else {
    int rhResult = execute_expr(expr->rhList, verbose);
    switch(expr->opt) {
    case PLUS:
      return lhResult + rhResult;
    case MINUS:
      return lhResult - rhResult;
    case TIMES:
      return lhResult * rhResult;
    case DIVIDE:
      return lhResult / rhResult;
    default:
      print_error("Unrecognized operation.");
      exit(1);
    }
  }
}

/** Execute an object, performing all descendant rolls as appropriate. To be called
 *  on an object node in a complete parse tree. */
int execute_obj(ObjNode* obj, bool verbose) {
  if (obj->roll != NULL) {
    return execute_roll(obj->roll, verbose);
  } else if (obj->subList != NULL) {
    return execute_expr(obj->subList, verbose);
  } else {
    return obj->constant;
  }
}

/** Sums up a series of rolls into a total. */
int sum_up(int* rolls, int count) {
  int sum = 0;
  for (int i = 0; i < count; i++) {
    sum += rolls[i];
  }
  return sum;
}

/** Performs a basic (unmodified) roll. */
int execute_basic_roll(int dieCount, int dieSides, bool verbose) {
  STACK_ALLOC(int, rolls, dieCount);
  if (verbose) {
    printf("%dd%d:\n", dieCount, dieSides);
  }
  for (int i = 0; i < dieCount; i++) {
    rolls[i] = (get_next_random() % dieSides)+1;
    if (verbose) {
      printf("  %d\n", rolls[i]);
    }
  }
  return sum_up(rolls, dieCount);
}

/** A comparison function that returns >0 if r2 > r1, <0 if r2 < r1, and 0 if r2==r1. */
int compare_roll_high(const void* r1, const void* r2) {
  return *(int*)r2 - *(int*)r1;
}

/** A comparison function that returns >0 if r2 < r1, <0 if r2 > r1, and 0 if r2==r1. */
int compare_roll_low(const void* r1, const void* r2) {
  return *(int*)r1 - *(int*)r2;
}

/** Performs a roll where some subset of the rolled dice are to be accounted for in the total, with dice chosen based
 *  on the provided compison function, which will be used to sort the rolls before the top N are selected. */
int execute_choose_n_roll(int dieCount, int dieSides, int nChoose, int(*comp)(const void*, const void*), bool verbose) {
  STACK_ALLOC(int, rolls, nChoose+1);
  int chosen = 0;
  int typechar = 'c';
  int test = 1;
  if (comp(&chosen, &test) < 0) {
    typechar = 'w';
  }
  if (verbose) {
    printf("%dd%d%c%d:\n", dieCount, dieSides, typechar, nChoose);
  }
  for (int i = 0; i < dieCount; i++) {
    int roll = (get_next_random() % dieSides) + 1;
    if (verbose) {
      printf("  %d\n", roll);
    }
    if (chosen < nChoose) {
      rolls[chosen] = roll;
      chosen++;
    } else {
      rolls[nChoose] = roll;
      qsort(rolls, nChoose+1, sizeof(int), comp);
    }
  }
  if (verbose) {
    printf("Chosen:");
    for (int i = 0; i < nChoose; i++) {
      printf(" %d", rolls[i]); 
    }
    printf("\n");
  }
  return sum_up(rolls, nChoose);
}

/** Execute a roll where all rolls below a threshold are rerolled until they are above it. */
int execute_reroll_below_roll(int dieCount, int dieSides, int rerollThresh, bool verbose) {
  STACK_ALLOC(int, rolls, dieCount);
  if (verbose) {
    printf("%dd%d%c%d:\n", dieCount, dieSides, 'b', rerollThresh);
  }
  for (int i = 0; i < dieCount; i++) {
    int roll = (get_next_random() % dieSides) + 1;
    if (verbose) {
      printf("  %d", roll);
    }
    while (roll <= rerollThresh) {
      if (verbose) {
        printf(" * Rerolled\n");
      }
      roll = (get_next_random() % dieSides) + 1;
      if (verbose) {
        printf("  %d", roll);
      }
    }
    if (verbose) {
      printf("\n");
    }
    rolls[i] = roll;
  }
  return sum_up(rolls, dieCount);
}

/** Execute a roll where rolls above a certain value 'explode' into an extra (also included in total) roll. */
int execute_exploding_roll(int dieCount, int dieSides, int explodeThresh, bool verbose) {
  STACK_ALLOC(int, rolls, dieCount);
  if (verbose) {
    printf("%dd%d%c%d:\n", dieCount, dieSides, 'v', explodeThresh);
  }
  for (int i = 0; i < dieCount; i++) {
    int rollTotal = 0;
    int roll = (get_next_random() % dieSides) + 1;
    rollTotal += roll;
    if (verbose) {
      printf("  %d", roll);
    }
    while (roll >= explodeThresh) {
      if (verbose) {
        printf(" * Exploded:\n");
      }
      roll = (get_next_random() % dieSides) + 1;
      rollTotal += roll;
      if (verbose) {
        printf("    %d", roll);
      }
    }
    if (verbose) {
      printf("\n");
    }
    rolls[i] = rollTotal;
  }
  return sum_up(rolls, dieCount);
}

/** Execute a die roll. Based on modifiers to the roll type, calls the appropriate roll execution function. */
int execute_roll(RollNode* roll, bool verbose) {
  if (roll->rollMod != NULL) {
    switch(roll->rollMod->type) {
    case CHOOSE_HIGH:
      return execute_choose_n_roll(roll->dieCount, roll->dieSides, roll->rollMod->constant, compare_roll_high, verbose);
    case CHOOSE_LOW:
      return execute_choose_n_roll(roll->dieCount, roll->dieSides, roll->rollMod->constant, compare_roll_low, verbose);
    case REROLL_BELOW:
      return execute_reroll_below_roll(roll->dieCount, roll->dieSides, roll->rollMod->constant, verbose);
    case KEEP_AND_REROLL_ABOVE:
      return execute_exploding_roll(roll->dieCount, roll->dieSides, roll->rollMod->constant, verbose);
    case NONE:
      return 0; //Should not happen
    }
  }
  return execute_basic_roll(roll->dieCount, roll->dieSides, verbose);
}

/** Prints a usage message and exits the program. */
void print_usage() {
  printf("Usage: dice <flags> <expression>\n See header for expression grammar.\n");
  printf("Use dice -help for a short explanation.\n");
  exit(1);
}

/** Prints a help message explaining some of program use. */
void print_help() {
  printf("General die rolls take the form of XdY.\nX is the number of dice to roll and Y is the number of sides of the die for those rolls.\nDie rolls can be composed with infix arithmetic operators (+, -, *, /) and can include constant values (ex. 1d4+4).\n\n-v flag: Enables verbose printing (each individual die rolled will be displayed). Default is to print numbered roll results for overall rolls only.\n\n-q flag: Only print the total value of each roll, newline-delimited, and nothing else (quiet mode). Useful for using the tool as input to other programs.\n\nDie modifiers (appended to end of die rolls):\n    c (Usage XdYcZ): Take only the Z highest results from the X dice rolled.\n    v (Usage XdYvZ): Roll 'exploding' dice, wherein if a value at or above Z is rolled on a given die an extra die (of the same Y many sides) is rolled and also added to the total. Such extra dice can also explode given the same threshold.\n    b (Usage XdYbZ): Reroll individual dice that fall below the threshold Z in value until they result in a value greater than Z.\n    w (Usage XdYwZ): Take only the Z lowest results from the X dice rolled.\n\n");
}

/** Parses option flags, etc out of the start of the input string. */
ConfigOptions parse_options(int argc, char** argv) {
  ConfigOptions opts = { VER_DEFAULT, MODE_CMDLINE, 0 };
  bool verbose = false;
  bool quiet = false;
  int i = 1;
  for (; (i < argc) && (argv[i][0] == '-'); i++) {
    if (strcmp(argv[i], "-help") == 0) {
      if (i == 1) {
        opts.mode = MODE_HELP;
      } else {
        print_usage();
      }
      return opts;
    }
    if (strcmp(argv[i], "-v") == 0) {
      verbose = true;
    }
    if (strcmp(argv[i], "-q") == 0) {
      quiet = true;
    }
    if (strcmp(argv[i], "-i") == 0) {
      opts.mode = MODE_INTERACTIVE;
    }
  }
  if (verbose && quiet) {
    verbose = false;
    quiet = false;
  }
  if (verbose) {
    opts.verbosity = VER_VERBOSE;
  }
  if (quiet) {
    opts.verbosity = VER_QUIET;
  }
  opts.option_count = i-1;
  return opts;
}

/** Handles the overall operation of the program in command-line invocational mode. */
void parse_and_exec_cmdline(int argc, char** argv, ConfigOptions options) {
  if (argc == 0) {
    print_usage();
  }
  bool verbose = (options.verbosity == VER_VERBOSE);
  bool quiet = (options.verbosity == VER_QUIET);

  init_random();
  if (verbose) {
    printf("----------------------------\n");
  }
  for (int i = 0; i < argc; i++) {
    if (!quiet) {
      printf("Roll %d:", i + 1);
    }
    if (verbose) {
      printf("\n----------------------------\n");
    } else if (!quiet) {
      printf(" ");
    }
    ExprList* tree = parse_expr(argv[i], strlen(argv[i]), A_OP);
    if (tree != NULL) {
      int result = execute_expr(tree, verbose);
      if (verbose) {
        printf("Total: ");
      }
      printf("%d\n", result);
      free_expr_node(tree);
    }
    if (verbose) {
      printf("----------------------------\n");
    }
  }
}

void parse_and_exec_set_command(char* cmd, ConfigOptions* options) {
  if ((strlen(cmd) >= 10) && (strncmp(cmd, "verbosity ", 10) == 0)) {
    cmd += 10;
    if (strcmp(cmd, "verbose\n") == 0 || strcmp(cmd, "v\n") == 0 || strcmp(cmd, "-v\n") == 0) {
      options->verbosity = VER_VERBOSE;
      printf("Verbosity set to verbose (-v)\n");
    } else if (strcmp(cmd, "normal\n") == 0 || strcmp(cmd, "default\n") == 0) {
      options->verbosity = VER_DEFAULT;
      printf("Verbosity set to default (normal)\n");
    } else if (strcmp(cmd, "quiet\n") == 0 || strcmp(cmd, "q\n") == 0 || strcmp(cmd, "-q\n") == 0) {
      options->verbosity = VER_QUIET;
      printf("Verbosity set to quiet (-q)\n");
    } else {
      print_error("Unrecognized verbosity setting.");
    }
  } else {
    print_error("Unrecognized setting.");
  }
}

void parse_and_exec_interactive_input(char* input, ConfigOptions* options) {
  if ((strlen(input) >= 4) && (strncmp(input, "set ", 4) == 0)) {
    //If some program state is to be updated
    parse_and_exec_set_command(input+4, options);
  } else {
    //If it is a set of rolls
    bool verbose = (options->verbosity == VER_VERBOSE);
    bool quiet = (options->verbosity == VER_QUIET);

    if (verbose) {
      printf("----------------------------\n");
    }

    char* current_location = input;
    
    for (int i = 1; *current_location; i++) {
      if (!quiet) {
        printf("Roll %d:", i);
      }
      if (verbose) {
        printf("\n----------------------------\n");
      } else if (!quiet) {
        printf(" ");
      }
      int n_chars_this_roll = strcspn(current_location, " \n");
      ExprList* tree = parse_expr(current_location, n_chars_this_roll, A_OP);
      if (tree != NULL) {
        int result = execute_expr(tree, verbose);
        if (verbose) {
          printf("Total: ");
        }
        printf("%d\n", result);
        free_expr_node(tree);
      }
      if (verbose) {
        printf("----------------------------\n");
      }
      current_location += n_chars_this_roll;
      if (strlen(current_location)) {
        current_location++;
      }
    }
  }
}

/** Handles interactive mode. */
void interactive_loop(ConfigOptions options) {
  
  init_random();

  char inpBuf[MAX_CMDLEN];

  printf("dice, interactive mode:\n>>> ");
  while (fgets(inpBuf, MAX_CMDLEN, stdin)) {
    if (inpBuf[0] == 0x1b || inpBuf[0] == 'q' || strcmp(inpBuf, "exit\n") == 0) {
      break;
    }
    parse_and_exec_interactive_input(inpBuf, &options);
    printf(">>> ");
  }
}

/** Handles command-line argument parsing and dispatch. */
int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
  }

  ConfigOptions options = parse_options(argc, argv);

  if (options.mode == MODE_HELP) {
    print_help();
    return 0;
  }

  int i = options.option_count + 1;

  switch(options.mode) {
  case MODE_CMDLINE:
    parse_and_exec_cmdline(argc - i, argv + i, options);
    break;
  case MODE_INTERACTIVE:
    interactive_loop(options);
    break;
  case MODE_TUI:
    break;
  default:
    break;
  }

  return 0;
}
