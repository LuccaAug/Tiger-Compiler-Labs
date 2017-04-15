/* This file is not complete.  You should fill it in with your
   solution to the programming exercise. */
#include <stdio.h>
#include "prog1.h"
#include "slp.h"
#include <string.h>

struct IntAndTable interpExp(A_exp exp, Table_ t);
Table_ interpStm(A_stm stm, Table_ t);

int maxargs(A_stm stm) {
	int r = 0;
	switch (stm->kind){
		case A_compoundStm: {
			A_stm stm1 = stm->u.compound.stm1;
			A_stm stm2 = stm->u.compound.stm2;
			int x = maxargs(stm1);
			int y = maxargs(stm2);
			r = (x > y) ? x : y;
		}
			break;
		case A_printStm: {
			A_expList expList = stm->u.print.exps;
			r = 1;
			while (expList->kind != A_lastExpList) {
				expList = expList->u.pair.tail;
				r++;
			}
		}
			break;
		case A_assignStm: {
			A_exp exp = stm->u.assign.exp;
			if (exp->kind == A_eseqExp) {
				A_stm stm1 = exp->u.eseq.stm;
				r = maxargs(stm1);
			}
		}
			break;
	}
	return r;
}

int look_up(string id, Table_ t) {
	Table_ i = t;
	while (i) {
		if (!strcmp(id, i->id))
			return i->value;
		i = i->tail;
	}
	return 0;
}

void update(string id, int value, Table_ *t) {
	Table_ new_entry = Table(id, value, *t);
	*t = new_entry;	
}

struct IntAndTable interpExp(A_exp exp, Table_ t) {
	struct IntAndTable r;
	switch (exp->kind) {
		case A_idExp: {
			r.i = look_up(exp->u.id, t);
			r.t = t;
		}
			break;
		case A_numExp: {
			r.i = exp->u.num;
			r.t = t;
		}
			break;
		case A_opExp: {
			struct IntAndTable left = 
				interpExp(exp->u.op.left, t);
			struct IntAndTable right = 
				interpExp(exp->u.op.right, t);			
			switch (exp->u.op.oper) {
				case A_plus:
					r.i = left.i + right.i;
					break;
				case A_minus:
					r.i = left.i - right.i;
					break;
				case A_times:
					r.i = left.i * right.i;
					break;
				case A_div:
					r.i = left.i / right.i;
					break;
			}
			r.t = t;
		}
			break;
		case A_eseqExp: {
			r.t = interpStm(exp->u.eseq.stm, t);
			r = interpExp(exp->u.eseq.exp, r.t);
		}
			break;
	}
	return r;
}

Table_ interpStm(A_stm stm, Table_ t) {
	switch (stm->kind){
		case A_compoundStm: {
			A_stm stm1 = stm->u.compound.stm1;
			A_stm stm2 = stm->u.compound.stm2;
			t = interpStm(stm1, t);
			t = interpStm(stm2, t);
		}
			break;
		case A_printStm: {
			A_expList expList = stm->u.print.exps;
			struct IntAndTable it;
			while (expList->kind != A_lastExpList) {
				it = interpExp(expList->u.pair.head, t);
				printf("%d ", it.i);
				t = it.t;
				expList = expList->u.pair.tail;
			}
			it = interpExp(expList->u.last, t);
			printf("%d\n", it.i);
			t = it.t;
		}
			break;
		case A_assignStm: {
			A_exp exp = stm->u.assign.exp;
			struct IntAndTable it = 
				interpExp(exp, t);
			t = it.t;
			update(stm->u.assign.id, it.i, &t);
		}
			break;
	}	
	return t;
}

void interp(A_stm stm) {
	interpStm(stm, NULL);
}

/*
 *Please don't modify the main() function
 */
int main()
{
	int args;

	printf("prog\n");
	args = maxargs(prog());
	printf("args: %d\n",args);
	interp(prog());

	printf("prog_prog\n");
	args = maxargs(prog_prog());
	printf("args: %d\n",args);
	interp(prog_prog());

	printf("right_prog\n");
	args = maxargs(right_prog());
	printf("args: %d\n",args);
	interp(right_prog());

	return 0;
}
