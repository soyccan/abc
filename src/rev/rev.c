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
#include "bdd/cudd/cuddInt.h"
#include "misc/vec/vec.h"

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
  // Hop_Man_t *pMan = pNtk->pManFunc;
  Abc_NtkObj(pNtk, 0)->pData = Cudd_ReadOne(dd);
  for (int i = 0; i < nPi; i++)
    Abc_NtkPi(pNtk, i)->pData = Cudd_bddIthVar(dd, i);

  // record if bdd of a node is built
  int *bddBuilt = ABC_CALLOC(int, Abc_NtkObjNumMax(pNtk));

  // build BDD for each node
  Abc_Obj_t *pNode;
  int i;
  // Abc_NtkForEachObj(pNtk, pNode, i) {
  Abc_NtkForEachNode(pNtk, pNode, i) {
    // Abc_NtkForEachPo(pNtk, pNode, i) {
    // if (Abc_ObjIsPo(pNode))
    //   pNode = Abc_ObjFanin0(pNode);

    // Hop_Obj_t *hNode = Hop_Regular(pNode->pData);
    // int comp = Hop_IsComplement(pNode->pData);
    // assert(hNode);

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

  debug("node id=%2d type=%d comp=%d,%d%d built=%d", node->Id, node->Type,
        Hop_IsComplement(node->pData), Abc_ObjFaninC0(node),
        Abc_ObjFaninC1(node), bddBuilt[node->Id]);

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

  // DdNode *func = node->pData;

  // if (Abc_ObjIsPi(node)) {
  //   return;
  // }

  // DdNode *fi0_func = DD_ONE(dd), *fi1_func = DD_ONE(dd);

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

// for i = 0..n-1
//   if PO[i] == PI[i] ^ c[i]
//     res[i] = 0
//     c[i+1] = PI[i] & c[i]
//   else if PO[i] == PI[i] ~^ c[i]
//     res[i] = 1
//     c[i+1] = PI[i] | c[i]
unsigned long ExtractAddend(Abc_Ntk_t *ntk) {
  DdManager *dd = ntk->pManFunc;

  const int MAX_ADDER_SIZE = 64;
  int arr[MAX_ADDER_SIZE];
  memset(arr, 0, sizeof(arr));
  assert(Abc_NtkPiNum(ntk) < MAX_ADDER_SIZE);

  DdNode *carry = Cudd_ReadLogicZero(dd);

  Abc_Obj_t *pi;
  int i;
  Abc_NtkForEachPi(ntk, pi, i) {
    Abc_Obj_t *po = Abc_NtkPo(ntk, i);
    DdNode *pidd = (DdNode *)pi->pData;
    DdNode *podd = (DdNode *)po->pData;

    DdNode *tmp = Cudd_bddXor(dd, pidd, carry);

    if (Cudd_bddXnor(dd, podd, tmp) == Cudd_ReadOne(dd)) {
      arr[i] = 0;
      carry = Cudd_bddAnd(dd, pidd, carry);
    } else if (Cudd_bddXnor(dd, podd, Cudd_Not(tmp)) == Cudd_ReadOne(dd)) {
      arr[i] = 1;
      carry = Cudd_bddOr(dd, pidd, carry);
    } else {
      // invalid constant adder circuit
      assert(0);
    }
  }

  unsigned long ret = 0;
  for (int i = MAX_ADDER_SIZE - 1; i >= 0; i--) {
    ret = (ret << 1) | arr[i];
  }

  return ret;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
ABC_NAMESPACE_IMPL_END
