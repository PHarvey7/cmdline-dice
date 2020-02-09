/** dice.h
 *  Simple command-line dice roller
 * (c) 2020 Patrick Harvey [see LICENSE.txt] */

/* A header file for the dice program. The utility parses
   an input line that describes a set of die rolls, possibly
   with modifiers, into a tree of roll and arithmetic operations,
   and then executes it for a result. */

/* The grammar for dice expressions is as follows:


   a_expr:    m_expr a_opt a_expr
            | m_expr

   m_expr:    obj m_opt m_expr
            | obj

   obj:       roll
            | constant
	    | '(' a_expr ')'

   roll:      constant 'd' constant roll_mod

   roll_mod:  'c' constant
            | 'b' constant
            | 'v' constant
            | 'w' constant
            |

   a_opt:     '+'
            | '-'
   
   m_opt:     '*'
	    | '/'
*/

#include <stdbool.h>

// The maximum length of a command in interactive mode, in chars.
#define MAX_CMDLEN 1024

typedef enum OpType {
  A_OP,
  M_OP
} OpType;

typedef enum Operation {
  NOOP,
  PLUS,
  MINUS,
  TIMES,
  DIVIDE
} Operation;

typedef enum ModifierType {
  NONE,
  CHOOSE_HIGH,
  CHOOSE_LOW,
  REROLL_BELOW,
  KEEP_AND_REROLL_ABOVE
} ModifierType;

typedef enum Verbosity {
  VER_QUIET,
  VER_DEFAULT,
  VER_VERBOSE
} Verbosity;

typedef enum Mode {
  MODE_CMDLINE,
  MODE_HELP,
  MODE_INTERACTIVE,
  MODE_TUI
} Mode;

struct objNode;
struct exprList;

typedef struct {
  enum ModifierType type;
  int constant;
} RollModifier;

typedef struct {
  int dieCount;
  int dieSides;
  RollModifier* rollMod;
} RollNode;

typedef struct exprList {
  struct exprList* lhList;
  struct objNode* obj;
  Operation opt;
  struct exprList* rhList;
} ExprList;

typedef struct objNode {
  RollNode* roll;
  int constant;
  ExprList* subList;
} ObjNode;

typedef struct configOptions {
  Verbosity verbosity;
  Mode mode;
  int option_count;
} ConfigOptions;

ExprList* parse_a_expr(char* inp, int len);
ExprList* parse_m_expr(char* inp, int len);
ObjNode* parse_obj(char* inp, int len);
RollNode* parse_roll(char* inp, int len);
RollModifier* parse_modifier(char* inp, int len);
Operation parse_operator(char* inp);
void free_expr_node(ExprList* node);
void free_obj_node(ObjNode* node);
void free_roll_node(RollNode* node);
int execute_obj(ObjNode* node, bool verbose);
int execute_expr(ExprList* expr, bool verbose);
int execute_roll(RollNode* node, bool verbose);
