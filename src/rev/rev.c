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
#define MAX_ADDER_SIZE 2048

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
  // Hop_Man_t *pMan = pNtk->pManFunc;
  Abc_NtkObj(pNtk, 0)->pData = Cudd_ReadOne(dd);
  for (int i = 0; i < nPi; i++)
    Abc_NtkPi(pNtk, i)->pData = Cudd_bddIthVar(dd, i);

  // record if bdd of a node is built
  // TODO: use pNode->fMarkA instead
  int *bddBuilt = ABC_CALLOC(int, Abc_NtkObjNumMax(pNtk));

  // build BDD for each node
  Abc_Obj_t *pNode;
  int i;
  Abc_NtkForEachNode(pNtk, pNode, i) {
    if (!bddBuilt[pNode->Id]) {
      // if bdd is not built
      Rev_AigNodeBuildBddToPi(dd, bddBuilt, pNode);
    }

    DdNode *pFunc = pNode->pData;

    if (pFunc == NULL) {
      Abc_PrintErr(
          ABC_ERROR,
          "Rev_NtkAigBuildBddToPi: Error while converting AIG into BDD.\n");
      return 0;
    }
  }

  Abc_NtkForEachPo(pNtk, pNode, i) {
    pNode->pData =
        Cudd_NotCond(Abc_ObjFanin0(pNode)->pData, Abc_ObjFaninC0(pNode));
  }

  Cudd_PrintInfo(dd, stdout);
  /*Reports the number of live nodes in BDDs and ADDs*/
  printf("DdManager nodes: %ld | ", Cudd_ReadNodeCount(dd));
  /*Returns the number of BDD variables in existance*/
  printf("DdManager vars: %d | ", Cudd_ReadSize(dd));
  /*Reports the number of nodes in the BDD*/
  // printf("DdNode nodes: %d | ", Cudd_DagSize(dd));
  /*Returns the number of variables in the BDD*/
  // printf("DdNode vars: %d | ", Cudd_SupportSize(gbm, dd));
  /*Returns the number of times reordering has occurred*/
  printf("DdManager reorderings: %d | ", Cudd_ReadReorderings(dd));
  /*Returns the memory in use by the manager measured in bytes*/
  printf("DdManager memory: %lfM |\n\n", Cudd_ReadMemoryInUse(dd) / 1048576.);
  // Prints to the standard output a DD and its statistics: number of nodes,
  // number of leaves, number of minterms.
  // Cudd_PrintDebug(gbm, dd, n, pr);

  // printf( "Reorderings performed = %d.\n", Cudd_ReadReorderings(ddTemp) );

  // replace manager
  // TODO: free the old manager
  // Hop_ManStop((Hop_Man_t *)pNtk->pManFunc);
  // Aig_ManStop(pNtk->pManFunc);
  pNtk->pManFunc = dd;

  // update the network type
  pNtk->ntkFunc = ABC_FUNC_BDD;
  pNtk->ntkType = ABC_NTK_LOGIC;

  ABC_FREE(bddBuilt);

  return 1;
}

void Rev_AigNodeBuildBddToPi(DdManager *dd, int *bddBuilt, Abc_Obj_t *node) {

  // debug("node id=%2d type=%d comp=%d,%d%d built=%d", node->Id, node->Type,
  //       Hop_IsComplement(node->pData), Abc_ObjFaninC0(node),
  //       Abc_ObjFaninC1(node), bddBuilt[node->Id]);

  if (bddBuilt[node->Id])
    // BDD already built
    return;

  bddBuilt[node->Id] = 1;

  // Hop_Obj_t *hNode = Hop_Regular(node->pData);
  // int comp = Hop_IsComplement(node->pData);

  // check the case of a constant
  // TODO
  // if (Hop_ObjIsConst1(hNode)) {
  //   node->pData = Cudd_NotCond(Cudd_ReadOne(dd), comp);
  //   return;
  // }

  if (!Abc_ObjIsNode(node))
    return;

  if (Abc_ObjFaninNum(node) == 2) {
    Rev_AigNodeBuildBddToPi(dd, bddBuilt, Abc_ObjFanin0(node));
    Rev_AigNodeBuildBddToPi(dd, bddBuilt, Abc_ObjFanin1(node));
    node->pData = Cudd_bddAnd(
        dd, Cudd_NotCond(Abc_ObjFanin0(node)->pData, Abc_ObjFaninC0(node)),
        Cudd_NotCond(Abc_ObjFanin1(node)->pData, Abc_ObjFaninC1(node)));
  } else if (Abc_ObjFaninNum(node) == 1) {
    Rev_AigNodeBuildBddToPi(dd, bddBuilt, Abc_ObjFanin0(node));
    node->pData =
        Cudd_NotCond(Abc_ObjFanin0(node)->pData, Abc_ObjFaninC0(node));
  } else {
    assert(0);
  }
  // TODO: correctly reference/dereference
  Cudd_Ref((DdNode *)node->pData);
}

static int match_sum(DdManager *dd, DdNode *podd, DdNode *pidd, DdNode *carry,
                     DdNode **nxt_carry) {

  DdNode *tmp = Cudd_bddXor(dd, pidd, carry);

  if (Cudd_bddXnor(dd, podd, tmp) == Cudd_ReadOne(dd)) {
    *nxt_carry = Cudd_bddAnd(dd, pidd, carry);
    return 0;
  } else if (Cudd_bddXnor(dd, podd, Cudd_Not(tmp)) == Cudd_ReadOne(dd)) {
    *nxt_carry = Cudd_bddOr(dd, pidd, carry);
    return 1;
  } else {
    *nxt_carry = NULL;
    return -1;
  }
}

// for i = 0..n-1
//   if PO[i] == PI[i] ^ c[i]
//     res[i] = 0
//     c[i+1] = PI[i] & c[i]
//   else if PO[i] == PI[i] ~^ c[i]
//     res[i] = 1
//     c[i+1] = PI[i] | c[i]
int ExtractAddendBdd(Abc_Ntk_t *ntk, unsigned long *addend) {
  DdManager *dd = ntk->pManFunc;

  int adder_size = Abc_NtkPiNum(ntk);
  int arr[MAX_ADDER_SIZE] = {0};
  assert(adder_size <= MAX_ADDER_SIZE);

  DdNode *carry = Cudd_ReadLogicZero(dd);
  for (int i = 0, j, k; i < adder_size; i++) {
    Abc_Obj_t *po = NULL, *pi = NULL;
    Abc_NtkForEachPo(ntk, po, j) {
      if (po->fMarkA)
        continue;
      int bit = -1;
      Abc_NtkForEachPi(ntk, pi, k) {
        if (pi->fMarkA)
          continue;
        DdNode *podd = (DdNode *)po->pData;
        DdNode *pidd = (DdNode *)pi->pData;
        DdNode *nxt_carry = NULL;
        bit = match_sum(dd, podd, pidd, carry, &nxt_carry);
        // debug("po %d pi %d match %d", j, k, bit);
        if (bit != -1) {
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

  {
    int i;
    Abc_Obj_t *node;
    Abc_NtkForEachPo(ntk, node, i) node->fMarkA = 0;
    Abc_NtkForEachPi(ntk, node, i) node->fMarkA = 0;
  }

  for (int j = 0; j < MAX_ADDER_SIZE / 64; j++) {
    unsigned long ret = 0;
    for (int i = 64 * (j + 1) - 1; i >= 64 * j; i--) {
      ret = (ret << 1) | arr[i];
    }
    addend[j] = ret;
  }

  return 1;
}

// ref: abcSat.c: Abc_NtkMiterSat
int ExtractAddendSat(Abc_Ntk_t *pNtk, unsigned long *addend, int fVerbose) {
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

  Abc_Obj_t *pNode;
  int i;
  Abc_NtkForEachObj(pNtk, pNode, i) pNode->fMarkA = 0;

  // tmp array for adding clause
  // TODO: size?
  Vec_Int_t *vVars = Vec_IntAlloc(100);
  // assumptions for incremental SAT
  Vec_Int_t *assumps = Vec_IntAlloc(100);

  int addend_vec[MAX_ADDER_SIZE] = {0};
  assert(Abc_NtkPiNum(pNtk) <= MAX_ADDER_SIZE);

  // new var carry
  int carryVar = sat_solver_nvars(pSat);
  // let carry = CONST_0 = !CONST_1 = !(var 0)
  sat_solver_add_buffer(pSat, 0, carryVar, 1);

  Abc_Obj_t *pi;
  abctime clk;
  lbool status;
  Abc_NtkForEachPi(pNtk, pi, i) {
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

    // assert PO[i] ^ PI[i] ^ carry[i]
    // if UNSAT => PO[i] == PI[i] ^ carry[i]
    // assert !(PO[i] ^ PI[i] ^ carry[i])
    // if UNSAT => PO[i] == PI[i] ~^ carry[i]
    Abc_Obj_t *po = Abc_NtkCo(pNtk, i);
    int poVar = (int)(ABC_PTRINT_T)po->pCopy;
    int piVar = (int)(ABC_PTRINT_T)pi->pCopy;

    int tmpVar = sat_solver_nvars(pSat);
    sat_solver_add_xor(pSat, tmpVar, piVar, carryVar, 0);
    int tmpVar2 = sat_solver_nvars(pSat);
    sat_solver_add_xor(pSat, tmpVar2, tmpVar, poVar, 0);

    // sat_solver_add_const(pSat, tmpVar2, 0);
    // printf("#clause=%d\n", sat_solver_nclauses(pSat));

    int sat_cnt = 0;
    for (int neg = 0; neg <= 1; neg++) {
      Vec_IntClear(assumps);
      Vec_IntPush(assumps, toLitCond(tmpVar2, neg));
      // Vec_IntPush(vVars,
      // toLitCond((int)(ABC_PTRINT_T)Abc_ObjRegular(pofi)->pCopy,
      //                              Abc_ObjIsComplement(pofi)));
      // ret =
      //     sat_solver_addclause(pSat, vVars->pArray, vVars->pArray +
      //     vVars->nSize);
      // assert(ret);

      // solve the miter
      ABC_INT64_T nConfLimit = 100000;
      ABC_INT64_T nInsLimit = 100000;
      ABC_INT64_T numConfs = 0;
      ABC_INT64_T numInspects = 0;
      clk = Abc_Clock();
      status =
          sat_solver_solve(pSat, Vec_IntArray(assumps), Vec_IntLimit(assumps),
                           nConfLimit, nInsLimit, 0, 0);
      if (status == l_Undef) {
        Abc_PrintErr(ABC_ERROR, "The problem timed out.\n");
        goto error;
      } else if (status == l_True) {
        // SAT
        sat_cnt++;
      } else if (status == l_False) {
        // UNSAT
        if (!neg) {
          // PO[i] == PI[i] ^ carry[i]
          // carry[i+1] = PI[i] & carry[i]
          addend_vec[i] = 0;
          int var = sat_solver_nvars(pSat);
          sat_solver_add_and(pSat, var, piVar, carryVar, 0, 0, 0);
          carryVar = var;
        } else {
          // PO[i] == PI[i] ~^ carry[i]
          // carry[i+1] = PI[i] | carry[i]
          addend_vec[i] = 1;
          int var = sat_solver_nvars(pSat);
          sat_solver_add_and(pSat, var, piVar, carryVar, 1, 1, 1);
          carryVar = var;
        }
      } else {
        assert(0);
      }
      if (fVerbose) {
        ABC_PRT("solver time", Abc_Clock() - clk);
        printf("The number of conflicts = %d.\n", (int)pSat->stats.conflicts);
      }

      if (sat_cnt >= 2) {
        // invalid constant adder circuit
        assert(0);
        return 0;
      }
    }

    //    ASat_SolverWriteDimacs( pSat, "temp_sat.cnf", NULL, NULL, 1 );
  }

  for (int j = 0; j < MAX_ADDER_SIZE / 64; j++) {
    unsigned long ret = 0;
    for (int i = 64 * (j + 1) - 1; i >= 64 * j; i--) {
      ret = (ret << 1) | addend_vec[i];
    }
    addend[j] = ret;
  }

  sat_solver_delete(pSat);
  return 1;

error:
  sat_solver_delete(pSat);
  return 0;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
ABC_NAMESPACE_IMPL_END
