/**CFile****************************************************************

  FileName    [revCom.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Reverse Engineer Datapath Command]

  Synopsis    [External declarations.]

  Author      [Cheng-Kang Chen]

  Affiliation [National Taiwan University]

  Date        [Ver. 1.0. Started - June 2, 2022.]

  Revision    [$Id: vec.h,v 1.00 2022/06/02 00:00:00 ahurst Exp $]

***********************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rev.h"

#include "aig/aig/aig.h"
#include "base/main/mainInt.h"
#include "bdd/bbr/bbr.h"
#include "bdd/cudd/cuddInt.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static int Abc_CommandRev(Abc_Frame_t *pAbc, int argc, char **argv) {
  Abc_Ntk_t *pNtk = Abc_FrameReadNtk(pAbc);
  assert(pNtk);

  if (!Abc_NtkIsStrash(pNtk)) {
    Abc_PrintErr(ABC_ERROR, "Network is not strash\n");
    return 1;
  }

  Aig_Man_t *pAig = Abc_NtkToDar(pNtk, 0, 1);
  Aig_ManShow(pAig, 0, NULL);

  // if (!Abc_NtkHasBdd(pNtk)) {
  //     // check if Abc_NtkAigToBdd was called
  //     Abc_PrintErr(ABC_ERROR, "Network is not BDD\n");
  //     return 1;
  // }

  Rev_NtkAigBuildBddToPi(pNtk);

  DdManager *dd = pNtk->pManFunc;
  // int arr[dd->size];

  // DdNode** node2func = ABC_ALLOC(DdNode*, Abc_NtkObjNumMax(pNtk));

  int i;
  Abc_Obj_t *po;
  Abc_NtkForEachPo(pNtk, po, i) {
    Abc_ObjPrint(stderr, po);

    debug("po id=%d comp=%d", Abc_ObjFanin0(po)->Id, Abc_ObjFaninC0(po));

    // Abc_Obj_t *node = Abc_ObjFanin0(po);
    // assert(!Abc_ObjIsBarBuf(node));
    // Abc_ObjPrint(stderr, node);

    DdNode *pFunc = po->pData;
    // DdNode* pFunc = Rev_BuildBddToPi(dd, node2func, pPo);

    // int nMints = 1;
    // DdNode** pbMints = Cudd_bddPickArbitraryMinterms(
    //     dd, pFunc, dd->vars, dd->size, nMints);

    // for (int k = 0; k < nMints; k++) {
    DdGen *gen;
    int *cube;
    CUDD_VALUE_TYPE value;
    Cudd_ForeachCube(dd, pFunc, gen, cube, value) {
      // Cudd_BddToCubeArray(dd, pbMints[k], arr);

      // for ( i = 0; i < Abc_NtkCiNum(pNtk); i++ )
      for (int i = 0; i < dd->size; i++)
        fprintf(stdout, "%c", cube[i] == 0 ? '0' : cube[i] == 1 ? '1' : '-');

      fprintf(stdout, "\n");
    }

    Abc_NodeShowBdd(po, 0);

    // Abc_ObjPrint( stderr, Abc_ObjFanin0(po) );
    // Abc_ObjPrint( stderr, Abc_ObjFanin0(Abc_ObjFanin0( po)) );
    // Abc_ObjPrintFaninCone( po );
    Abc_PrintErr(1, "\n");
  }

  return 0;
}

void Rev_Init(Abc_Frame_t *pAbc) {
  Cmd_CommandAdd(pAbc, "Various", "rev", Abc_CommandRev, 0);
}

void Rev_End(Abc_Frame_t *pAbc) { ; }

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
ABC_NAMESPACE_IMPL_END
