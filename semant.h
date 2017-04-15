#ifndef SEMANT_H
#define SEMANT_H

typedef void *Tr_exp; // avoid IR

struct expty {Tr_exp exp; Ty_ty ty;};

struct expty expTy(Tr_exp exp, Ty_ty ty);

struct expty transVar(S_table venv, S_table tenv, A_var v);

struct expty transExp(S_table venv, S_table tenv, A_exp e);

void         transDec(S_table venv, S_table tenv, A_dec d);

       Ty_ty transTy(               S_table tenv, A_ty a);

       Ty_ty actual_ty(Ty_ty ty);

void SEM_transProg(A_exp exp);

#endif
