#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "absin.h"
#include "types.h"
#include "errormsg.h"
#include "temp.h"
#include "tree.h"
#include "parse.h"
#include "frame.h"
#include "semant.h"

extern int yyparse(void);
extern A_exp absin_root;

A_exp parse(string fname) 
{EM_reset(fname);
 if (yyparse() == 0)
   return absin_root;
 else return NULL;
}

