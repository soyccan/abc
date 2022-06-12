/**CFile****************************************************************

  FileName    [rev.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Reverse Engineer Datapath]

  Synopsis    [External declarations.]

  Author      [Cheng-Kang Chen]

  Affiliation [National Taiwan University]

  Date        [Ver. 1.0. Started - June 2, 2022.]

  Revision    [$Id: vec.h,v 1.00 2022/06/02 00:00:00 ahurst Exp $]

***********************************************************************/

#include "rev.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aig/hop/hop.h"
#include "base/abc/abc.h"
#include "base/abci/abcSat.h"
#include "bdd/cudd/cuddInt.h"
#include "misc/vec/vecInt.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Allocates memory pieces of fixed size.]

  Description [The size of the chunk is computed as the minimum of
  1024 entries and 64K. Can only work with entry size at least 4 byte long.]

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_ObjPrintFaninCone(Abc_Obj_t *pObj) {
  if (!pObj)
    return;
  Abc_Obj_t *fi0 = NULL, *fi1 = NULL;
  if (Abc_ObjFaninNum(pObj) >= 1)
    fi0 = Abc_ObjChild0(pObj);
  if (Abc_ObjFaninNum(pObj) >= 2)
    fi1 = Abc_ObjChild1(pObj);
  // Abc_PrintErr( ABC_STANDARD, " { " );
  Abc_ObjPrintFaninCone(fi0);
  Abc_ObjPrintFaninCone(fi1);
  Abc_ObjPrint(stderr, pObj);
  // printf("%d\n", Abc_ObjFaninC0(pObj));
  // Abc_PrintErr( ABC_STANDARD, " } " );
}

// ref: abcFunc.c: Abc_NtkAigToBdd
int Rev_NtkAigBuildBddToPi(Abc_Ntk_t *pNtk) {

  assert(Abc_NtkHasAig(pNtk));

  // start the functionality manager
  int nPi = Abc_NtkPiNum(pNtk);
  DdManager *dd = Cudd_Init(nPi, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);

  // set the mapping of AIG primary inputs to the BDD variables
  Abc_AigConst1(pNtk)->pData = Cudd_ReadOne(dd);
  for (int i = 0; i < nPi; i++)
    Abc_NtkPi(pNtk, i)->pData = Cudd_bddIthVar(dd, i);

  // build BDD for each node
  Abc_Obj_t *pNode;
  int i;
  Abc_NtkForEachNode(pNtk, pNode, i) {
    if (!pNode->fMarkA) {
      // if bdd is not built
      Rev_AigNodeBuildBddToPi(dd, pNode);
    }

    DdNode *pFunc = pNode->pData;

    if (pFunc == NULL) {
      Abc_PrintErr(
          ABC_ERROR,
          "Rev_NtkAigBuildBddToPi: Error while converting AIG into BDD.\n");
      return 0;
    }
  }

  Abc_NtkForEachPo(pNtk, pNode, i) Rev_AigNodeBuildBddToPi(dd, pNode);

  Abc_NtkForEachObj(pNtk, pNode, i) pNode->fMarkA = 0;

  // replace manager
  Abc_AigFree((Abc_Aig_t *)pNtk->pManFunc);
  pNtk->pManFunc = dd;

  // update the network type
  pNtk->ntkFunc = ABC_FUNC_BDD;
  pNtk->ntkType = ABC_NTK_LOGIC;

  return 1;
}

void Rev_AigNodeBuildBddToPi(DdManager *dd, Abc_Obj_t *node) {

  // debug("node id=%2d type=%d comp=%d,%d%d built=%d", node->Id, node->Type,
  //       Hop_IsComplement(node->pData), Abc_ObjFaninC0(node),
  //       Abc_ObjFaninC1(node), bddBuilt[node->Id]);

  if (node->fMarkA)
    // BDD already built
    return;

  // record that bdd is built
  node->fMarkA = 1;

  // check the case of a constant
  if (Abc_AigNodeIsConst(node)) {
    node->pData = Cudd_NotCond(Cudd_ReadOne(dd), Abc_ObjIsComplement(node));
    return;
  }

  if (!Abc_ObjIsNode(node) && !Abc_ObjIsPo(node))
    return;

  if (Abc_ObjFaninNum(node) == 2) {
    Rev_AigNodeBuildBddToPi(dd, Abc_ObjFanin0(node));
    Rev_AigNodeBuildBddToPi(dd, Abc_ObjFanin1(node));
    node->pData = Cudd_bddAnd(
        dd, Cudd_NotCond(Abc_ObjFanin0(node)->pData, Abc_ObjFaninC0(node)),
        Cudd_NotCond(Abc_ObjFanin1(node)->pData, Abc_ObjFaninC1(node)));
  } else if (Abc_ObjFaninNum(node) == 1) {
    Rev_AigNodeBuildBddToPi(dd, Abc_ObjFanin0(node));
    node->pData =
        Cudd_NotCond(Abc_ObjFanin0(node)->pData, Abc_ObjFaninC0(node));
  } else {
    assert(0);
  }
  // TODO: correctly reference/dereference
  cuddRef((DdNode *)node->pData);
}

// determine PO-PI pair by BDD, and derive next carry
static int pairPoPiBdd(DdManager *dd, DdNode *po, DdNode *pi, DdNode *carry,
                       DdNode **nxt_carry) {
  DdNode *tmp = Cudd_bddXor(dd, pi, carry);
  cuddRef(tmp);
  DdNode *cond1 = Cudd_bddXnor(dd, po, tmp);
  cuddRef(cond1);
  DdNode *cond2 = Cudd_bddXnor(dd, po, Cudd_Not(tmp));
  cuddRef(cond2);
  int ret;
  if (cond1 == Cudd_ReadOne(dd)) {
    *nxt_carry = Cudd_bddAnd(dd, pi, carry);
    ret = 0;
  } else if (cond2 == Cudd_ReadOne(dd)) {
    *nxt_carry = Cudd_bddOr(dd, pi, carry);
    ret = 1;
  } else {
    *nxt_carry = NULL;
    ret = -1;
  }
  if (*nxt_carry)
    cuddRef(*nxt_carry);
  Cudd_RecursiveDeref(dd, tmp);
  Cudd_RecursiveDeref(dd, cond1);
  Cudd_RecursiveDeref(dd, cond2);
  return ret;
}

// determine PO-PI pair by SAT, and derive next carry
static int pairPoPiSat(sat_solver *pSat, Vec_Int_t *assumps, int po, int pi,
                       int carry, int *nxtCarry, int fVerbose) {

  // assert PO[i] ^ PI[i] ^ carry[i]
  // if UNSAT => PO[i] == PI[i] ^ carry[i]
  //
  // assert !(PO[i] ^ PI[i] ^ carry[i])
  // if UNSAT => PO[i] == PI[i] ~^ carry[i]

  int tmp = sat_solver_nvars(pSat);
  sat_solver_add_xor(pSat, tmp, pi, carry, 0);
  int tmp2 = sat_solver_nvars(pSat);
  sat_solver_add_xor(pSat, tmp2, tmp, po, 0);

  // sat_solver_add_const(pSat, tmpVar2, 0);
  // printf("#clause=%d\n", sat_solver_nclauses(pSat));

  abctime clk;
  lbool status;
  int sat_cnt = 0;
  for (int neg = 0; neg <= 1; neg++) {
    Vec_IntClear(assumps);
    Vec_IntPush(assumps, toLitCond(tmp2, neg));
    // Vec_IntPush(vVars,
    // toLitCond((int)(ABC_PTRINT_T)Abc_ObjRegular(pofi)->pCopy,
    //                              Abc_ObjIsComplement(pofi)));
    // ret =
    //     sat_solver_addclause(pSat, vVars->pArray, vVars->pArray +
    //     vVars->nSize);
    // assert(ret);

    // solve the miter
    clk = Abc_Clock();
    status = sat_solver_solve(pSat, Vec_IntArray(assumps),
                              Vec_IntLimit(assumps), 0, 0, 0, 0);
    if (status == l_Undef) {
      Abc_PrintErr(ABC_ERROR, "The problem timed out.\n");
      return -1;
    } else if (status == l_True) {
      // SAT
      sat_cnt++;
    } else if (status == l_False) {
      // UNSAT
      if (!neg) {
        // PO[i] == PI[i] ^ carry[i]
        // carry[i+1] = PI[i] & carry[i]
        int var = sat_solver_nvars(pSat);
        sat_solver_add_and(pSat, var, pi, carry, 0, 0, 0);
        *nxtCarry = var;
        return 0;
      } else {
        // PO[i] == PI[i] ~^ carry[i]
        // carry[i+1] = PI[i] | carry[i]
        int var = sat_solver_nvars(pSat);
        sat_solver_add_and(pSat, var, pi, carry, 1, 1, 1);
        *nxtCarry = var;
        return 1;
      }
    } else {
      assert(0);
    }
    if (fVerbose) {
      ABC_PRT("solver time", Abc_Clock() - clk);
      printf("The number of conflicts = %d.\n", (int)pSat->stats.conflicts);
    }
  }
  // ASat_SolverWriteDimacs( pSat, "temp_sat.cnf", NULL, NULL, 1 );
  return -1;
}

// for i = 0..n-1
//   if PO[i] == PI[i] ^ c[i]
//     res[i] = 0
//     c[i+1] = PI[i] & c[i]
//   else if PO[i] == PI[i] ~^ c[i]
//     res[i] = 1
//     c[i+1] = PI[i] | c[i]
int Rev_ExtractAddendBdd(Abc_Ntk_t *ntk, unsigned long *addend) {
  DdManager *dd = ntk->pManFunc;

  int adder_size = Abc_NtkPiNum(ntk);
  int arr[MAX_ADDER_SIZE];
  memset(arr, -1, sizeof(arr));
  if (adder_size > MAX_ADDER_SIZE) {
    Abc_PrintErr(ABC_ERROR, "Adder size too large\n");
    return 0;
  }

  DdNode *carry = Cudd_ReadLogicZero(dd);
  for (int i = 0, j, k; i < adder_size; i++) {
    // pair PO-PIs
    Abc_Obj_t *po = NULL, *pi = NULL;
    Abc_NtkForEachPo(ntk, po, j) {
      if (po->fMarkA)
        continue;
      int bit = -1;
      Abc_NtkForEachPi(ntk, pi, k) {
        if (pi->fMarkA)
          continue;
        DdNode *nxt_carry = NULL;
        bit = pairPoPiBdd(dd, (DdNode *)po->pData, (DdNode *)pi->pData, carry,
                          &nxt_carry);
        if (bit != -1) {
          debug("i=%d po=%d pi=%d match %d", i, j, k, bit);
          arr[i] = bit;
          carry = nxt_carry;
          po->fMarkA = 1;
          pi->fMarkA = 1;
          break;
        }
      }
      if (bit != -1)
        break;
    }
  }
  Cudd_RecursiveDeref(dd, carry);

  // # of PO-PI pairs left unpaired
  int remain = 0;
  {
    int i;
    Abc_Obj_t *node;
    Abc_NtkForEachPi(ntk, node, i) {
      if (!node->fMarkA)
        remain++;
      node->fMarkA = 0;
    }
    Abc_NtkForEachPo(ntk, node, i) {
      if (!node->fMarkA)
        assert(Cudd_Regular((DdNode *)node->pData) ==
               Cudd_Regular((DdNode *)Abc_ObjFanin0(node)->pData));
      node->fMarkA = 0;
    }
  }

  for (int j = 0; j < MAX_ADDER_SIZE / 64; j++) {
    unsigned long ret = 0;
    for (int i = 64 * (j + 1) - 1; i >= 64 * j; i--) {
      ret = (ret << 1) | (i < adder_size && i >= remain ? arr[i - remain] : 0);
    }
    addend[j] = ret;
  }

  return 1;
}

// ref: abcSat.c: Abc_NtkMiterSat
int Rev_ExtractAddendSat(Abc_Ntk_t *pNtk, unsigned long *addend, int fVerbose) {
  int ret;

  sat_solver *pSat = sat_solver_new();
  assert(pSat);
  if (fVerbose) {
    pSat->fVerbose = 1;
    pSat->verbosity = 1;
    pSat->fPrintClause = 1;
  }

  ret = Abc_NtkMiterSatCreateInt(pSat, pNtk);
  assert(ret);
  // debug("solver #var=%d", sat_solver_nvars(pSat));
  {
    // clear mark set by Abc_NtkMiterSatCreateInt
    Abc_Obj_t *pNode;
    int i;
    Abc_NtkForEachObj(pNtk, pNode, i) pNode->fMarkA = 0;
  }

  int addend_vec[MAX_ADDER_SIZE];
  memset(addend_vec, -1, sizeof(addend_vec));
  int adder_size = Abc_NtkPiNum(pNtk);
  if (adder_size > MAX_ADDER_SIZE) {
    Abc_PrintErr(ABC_ERROR, "Adder size too large\n");
    return 0;
  }

  // assumptions for incremental SAT
  Vec_Int_t *assumps = Vec_IntAlloc(100);

  // new var carry
  int carryVar = sat_solver_nvars(pSat);
  // let carry = CONST_0 = !CONST_1 = !(var 0)
  sat_solver_add_buffer(pSat, 0, carryVar, 1);

  for (int i = 0, j, k; i < adder_size; i++) {
    Abc_Obj_t *po = NULL, *pi = NULL;
    // Abc_NtkForEachCo(pNtk, po, j) {
    Abc_NtkForEachPo(pNtk, po, j) {
      if (po->fMarkA)
        continue;
      int bit = -1;
      Abc_NtkForEachPi(pNtk, pi, k) {
        if (pi->fMarkA)
          continue;
        // clk = Abc_Clock();
        // status = sat_solver_simplify(pSat);
        // printf("Simplified the problem to %d variables and %d clauses. ",
        //        sat_solver_nvars(pSat), sat_solver_nclauses(pSat));
        // ABC_PRT("Time", Abc_Clock() - clk);
        // if (status == 0) {
        //   sat_solver_delete(pSat);
        //   printf("The problem is UNSATISFIABLE after simplification.\n");
        //   return 1;
        // }
        int nxtCarryVar = -1;
        bit = pairPoPiSat(pSat, assumps, (int)(ABC_PTRINT_T)po->pCopy,
                          (int)(ABC_PTRINT_T)pi->pCopy, carryVar, &nxtCarryVar,
                          fVerbose);
        if (bit != -1) {
          debug("i=%d po=%d pi=%d match %d", i, j, k, bit);
          addend_vec[i] = bit;
          carryVar = nxtCarryVar;
          po->fMarkA = 1;
          pi->fMarkA = 1;
          break;
        }
      }
      if (bit != -1)
        break;
    }
  }

  // # of PO-PI pairs left unpaired
  int remain = 0;
  {
    int i;
    Abc_Obj_t *node;
    Abc_NtkForEachPi(pNtk, node, i) {
      if (!node->fMarkA)
        remain++;
      node->fMarkA = 0;
    }
    Abc_NtkForEachPo(pNtk, node, i) {
      if (!node->fMarkA)
        assert(Cudd_Regular((DdNode *)node->pData) ==
               Cudd_Regular((DdNode *)Abc_ObjFanin0(node)->pData));
      node->fMarkA = 0;
    }
  }

  for (int j = 0; j < MAX_ADDER_SIZE / 64; j++) {
    unsigned long ret = 0;
    for (int i = 64 * (j + 1) - 1; i >= 64 * j; i--) {
      ret = (ret << 1) |
            (i < adder_size && i >= remain ? addend_vec[i - remain] : 0);
    }
    addend[j] = ret;
  }

  sat_solver_delete(pSat);
  return 1;

error:
  sat_solver_delete(pSat);
  return 0;
}

Abc_Obj_t *Abc_NodeRecognizeExor(Abc_Obj_t *pNode, Abc_Obj_t **ppNodeT,
                                Abc_Obj_t **ppNodeE) {
  ;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
ABC_NAMESPACE_IMPL_END
