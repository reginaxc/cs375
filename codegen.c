/* codgen.c       Generate Assembly Code for x86         11 Oct 17   */

/* Copyright (c) 2017 Gordon S. Novak Jr. and The University of Texas at Austin
    */

/* Starter file for CS 375 Code Generation assignment.           */
/* Written by Gordon S. Novak Jr.                  */

/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License (file gpl.text) for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "token.h"
#include "symtab.h"
#include "lexan.h"
#include "genasm.h"
#include "codegen.h"

void genc(TOKEN code);

/* Set DEBUGGEN to 1 for debug printouts of code generation */
#define DEBUGGEN 0
#define REGISTERS 32

int nextlabel;    /* Next available label number */
int stkframesize;   /* total stack frame size */
int registertable[REGISTERS]; /* Register Table */

int op_to_inst_int[50];
int op_to_inst_real[50];
int op_to_inst_point[50];

int ifc_to_jinst[50];

/* Top-level entry for code generator.
   pcode    = pointer to code:  (program foo (output) (progn ...))
   varsize  = size of local storage in bytes
   maxlabel = maximum label number used so far

Add this line to the end of your main program:
    gencode(parseresult, blockoffs[blocknumber], labelnumber);
The generated code is printed out; use a text editor to extract it for
your .s file.
         */

void gencode(TOKEN pcode, int varsize, int maxlabel) {
     initalisetables();
     TOKEN name, code;
     name = pcode->operands;
     code = name->link->link;
     nextlabel = maxlabel + 1;
     stkframesize = asmentry(name->stringval,varsize);
     genc(code);
     asmexit(name->stringval);
  }

int getreg(int kind)
  {
    int chosen_register;

    if (kind == 1) {  
      for (int i = RBASE; i <= RMAX; i ++) {
        if (registertable[i] == 0) {
          used(i);
          chosen_register =  i; 
          break;
        }
      }
    } else {
      for (int i = FBASE; i <= FMAX; i ++) {
        if (registertable[i] == 0) {
          used(i);
          chosen_register = i;
          break;
        }
      }
    }

    if (DEBUGGEN) {
      printf("getreg\n");
      printf("chosen %d\n", chosen_register);
    }
    return chosen_register;
  }

/* Generate code for arithmetic expression, return a register number */
int genarith(TOKEN code)
  { 
    int num, reg, offs, reg1 = -1, reg2 = -1;
    SYMBOL sym;
    float value;
    TOKEN lhs, rhs;

    if (DEBUGGEN) { 
      printf("genarith\n");
	    dbugprinttok(code);
    };
    //Call genaref somewhere
    switch ( code->tokentype ) { 
      case NUMBERTOK:   switch (code->datatype) { 
                            case INTEGER: num = code->intval;
                                          reg = getreg(1);
                                     		  if ( num >= MINIMMEDIATE && num <= MAXIMMEDIATE )
                                   		    asmimmed(MOVL, num, reg);
                                     		  break;
    	                      case REAL:    value = code->realval;
                                          reg = getreg(2);
                                          int label = nextlabel ++;
                                          makeflit(value, label);
                                          asmldflit(MOVSD, label, reg);
                                          break;
              	        }
                        break;
      case STRINGTOK: num = nextlabel++;
                      makeblit(code->stringval, num);


      case IDENTIFIERTOK: //If not a function
                          sym =  code->symentry;
                          offs = sym->offset - stkframesize;
                          switch (code->datatype) {
                            case INTEGER: reg = getreg(1);
                                          asmld(MOVL, offs, reg, code->stringval);
                                          break;
                            case REAL:    reg = getreg(2);
                                          asmld(MOVSD, offs, reg, code->stringval); 
                                          break;
                            case POINTER: break; //TODO (MOVQ)
                          }
                          break;

      case OPERATOR:  if (code->whichval == AREFOP) {
                        //reg = genaref(code);
                      } else if (code->whichval == FLOATOP) {
                          lhs = code->operands;
                          reg1 = genarith(lhs);
                          reg = getreg(2);
                          asmfloat(reg1, reg);
                          unused(reg1);
                      } else if (code->whichval == FLOATOP) {
                          lhs = code->operands;
                          reg1 = genarith(lhs);
                          reg = getreg(1);
                          asmfloat(reg1, reg);
                          unused(reg1);
                      } else if (code->datatype == INTEGER) {
                          lhs = code->operands;
                          rhs = lhs->link;
                          reg = genarith(lhs);     
                          reg1 = genarith(rhs);     
                          asmrr(op_to_inst_int[code->whichval], reg1, reg); break;
                          unused(reg1);
                      } else if (code->datatype == REAL) {
                         lhs = code->operands;
                          rhs = lhs->link;
                          reg = genarith(lhs);     
                          reg1 = genarith(rhs);     
                          asmrr(op_to_inst_real[code->whichval], reg1, reg); break;
                          unused(reg1);
                      } else {
                        printf("Dont know what to do");
                        return -1;
                      }
                      break;
    };

     return reg;
  }


/* Generate code for a Statement from an intermediate-code form */
void genc(TOKEN code)
  {  TOKEN tok, lhs, rhs;
     int reg, offs;
     SYMBOL sym;
     clearreg();

     if (DEBUGGEN) {
      printf("genc\n");
	    dbugprinttok(code);
     };

     if ( code->tokentype != OPERATOR ) { 
          printf("Bad code token");
      	  dbugprinttok(code);
      };

     switch ( code->whichval ) {
      case PROGNOP:  tok = code->operands;
                	   while ( tok != NULL ) {
                       genc(tok);
                		   tok = tok->link;
                	   }; 
                	   break;

      case ASSIGNOP: lhs = code->operands;      /* Trivial version: handles I := e */
                	   rhs = lhs->link;
                	   reg = genarith(rhs);              /* generate rhs into a register */
                	   sym = lhs->symentry;              /* assumes lhs is a simple var  */
                	   offs = sym->offset - stkframesize; /* net offset of the var   */
                     switch (code->datatype) {          /* store value into lhs  */
                       case INTEGER: asmst(MOVL, reg, offs, lhs->stringval);
                                     break;
                                 /* ...  */
                     };
                     break; 

      case GOTOOP: asmjump(JMP, code->operands->intval);
                   break;

      case LABELOP: asmlabel(code->operands->intval);
                    break;

      case IFOP:  tok = code->operands;
                  reg = genarith(tok);        
                  lhs = tok->link; //THEN PART
                  rhs = lhs->link; //ELSE PART
                  int curr = nextlabel++;
                  asmjump(ifc_to_jinst[tok->whichval], curr);
                  if (rhs) 
                    genc(rhs);
                  int end = nextlabel++;
                  asmjump(JMP, end);
                  asmlabel(curr);
                  genc(lhs);
                  asmlabel(end);
                  break;

      case FUNCALLOP: tok = code->operands; //FUNCTION
                      lhs = tok->link;  //FIRST ARGUMENT
                      while (lhs) {
                        genarith(lhs);
                        lhs = lhs->link;
                      }
                      break;
	   };
  }

/* Generate code for array references and pointers */
/* In Pascal, a (^ ...) can only occur as first argument of an aref. */
/* If storereg < 0, generates a load and returns register number;
   else, generates a store from storereg. */
int genaref(TOKEN code, int storereg) {
  return -1;
}

/* Set up a literal address argument for subroutine call, e.g. writeln('*') */
/* Example:  asmlitarg(8, EDI);   addr of literal 8 --> %edi */
void asmlitarg(int labeln, int dstreg) {

}


/* Generate code for a function call */
int genfun(TOKEN code);

/* find the correct MOV op depending on type of code */
int moveop(TOKEN code);

  void clearreg() {
    for (int i = 0; i < REGISTERS; i ++) {
      unused(i);
    }
  }

  void unused(int reg) {
    registertable[reg] = 0;
  }

  void used(int reg){
    registertable[reg] = 1; 
  }

  void initalisetables() {
    op_to_inst_int[PLUSOP] = ADDL;
    op_to_inst_int[MINUSOP] = SUBL;
    op_to_inst_int[TIMESOP] = IMULL;
    op_to_inst_int[DIVIDEOP] = DIVL;
    op_to_inst_int[ANDOP] = ANDL;
    op_to_inst_int[OROP] = ORL;
    op_to_inst_int[EQOP] = op_to_inst_int[LEOP] = op_to_inst_int[LTOP] = op_to_inst_int[GEOP] = op_to_inst_int[GTOP] = op_to_inst_int[NEOP] = CMPL;


    op_to_inst_real[PLUSOP] = ADDSD;
    op_to_inst_real[MINUSOP] = SUBSD;
    op_to_inst_real[TIMESOP] = MULSD;
    op_to_inst_real[DIVIDEOP] = DIVSD;
    op_to_inst_real[NOTOP] = NEGSD;
    op_to_inst_real[EQOP] = op_to_inst_real[LEOP] = op_to_inst_real[LTOP] = op_to_inst_real[GEOP] = op_to_inst_real[GTOP] = op_to_inst_real[NEOP] = CMPSD;


    op_to_inst_point[PLUSOP] = ADDQ;
    op_to_inst_point[MINUSOP] = SUBQ;
    op_to_inst_point[TIMESOP] = IMULQ;
    op_to_inst_point[ANDOP] = ANDQ;
    op_to_inst_point[NOTOP] = NOTQ;
    op_to_inst_point[OROP] = ORQ;
    op_to_inst_point[EQOP] = op_to_inst_point[LEOP] = op_to_inst_point[LTOP] = op_to_inst_point[GEOP] = op_to_inst_point[GTOP] = op_to_inst_point[NEOP] = CMPQ;

    ifc_to_jinst[EQOP] = JE;
    ifc_to_jinst[NEOP] = JNE;
    ifc_to_jinst[LTOP] = JL;
    ifc_to_jinst[LEOP] = JLE;
    ifc_to_jinst[GEOP] = JGE;
    ifc_to_jinst[GTOP] = JG;
  }
