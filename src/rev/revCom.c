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
#include "base/abci/abcSat.h"
#include "base/main/mainInt.h"
#include "bdd/bbr/bbr.h"
#include "bdd/cudd/cuddInt.h"
#include "misc/util/abc_global.h"
#include "misc/vec/vecInt.h"

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

  if (pNtk == NULL) {
    Abc_Print(-1, "Empty network.\n");
    return 1;
  }

  if (!Abc_NtkIsStrash(pNtk)) {
    Abc_PrintErr(ABC_ERROR, "Network is not strash\n");
    return 1;
  }

  // set defaults
  int fVerbose = 0, fSat = 0, fPlot = 0;
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "svph")) != EOF) {
    switch (c) {
    case 's':
      fSat ^= 1;
      break;
    case 'v':
      fVerbose ^= 1;
      break;
    case 'p':
      fPlot ^= 1;
      break;
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }

  if (fPlot) {
    Abc_NtkShow(pNtk, 0, 0, 0);
  }

  if (fVerbose) {
    Abc_Obj_t *pObj;
    int i;
    Abc_NtkForEachObj(pNtk, pObj, i) { Abc_ObjPrint(stderr, pObj); }
  }

  unsigned long addend[32];
  int ret;
  if (fSat) { // use SAT
    ret = ExtractAddendSat(pNtk, addend, fVerbose);
  } else { // use BDD
    Rev_NtkAigBuildBddToPi(pNtk);

    ret = ExtractAddendBdd(pNtk, addend);

    DdManager *dd = pNtk->pManFunc;

    if (fVerbose) {
      int i;
      Abc_Obj_t *po;
      Abc_NtkForEachPo(pNtk, po, i) {
        Abc_ObjPrint(stderr, po);

        debug("po id=%d comp=%d", Abc_ObjFanin0(po)->Id, Abc_ObjFaninC0(po));

        DdNode *pFunc = po->pData;
        printBddCubes(dd, pFunc);
        Abc_NodeShowBdd(po, 0);
      }
      Abc_PrintErr(1, "\n");
    }

    {
      int i;
      Abc_Obj_t *node;
      Abc_NtkForEachObj(pNtk, node, i) {
        if (node->pData) {
          Cudd_RecursiveDeref(dd, node->pData);
        }
      }
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
  }

  Abc_Print(1, "Extracted addend: (%lu) ", addend[0]);
  for (int i = 0; i < MAX_ADDER_SIZE / 64; i++) {
    Abc_Print(1, "%lx ", addend[i]);
  }
  Abc_Print(1, "\n");

  return ret;

usage:
  Abc_Print(-2, "usage: rev [-svh]\n");
  Abc_Print(-2, "\t         extract addend\n");
  Abc_Print(-2, "\t-s     : extract using SAT (default BDD) [default = %s]\n",
            fSat ? "yes" : "no");
  Abc_Print(-2, "\t-v     : verbose [default = %s]\n", fVerbose ? "yes" : "no");
  Abc_Print(-2, "\t-p     : plot AIG & BDD [default = %s]\n",
            fPlot ? "yes" : "no");
  Abc_Print(-2, "\t-h     : print the command usage\n");
  return 1;
}

void Rev_Init(Abc_Frame_t *pAbc) {
  Cmd_CommandAdd(pAbc, "Various", "rev", Abc_CommandRev, 0);
}

void Rev_End(Abc_Frame_t *pAbc) { ; }

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
ABC_NAMESPACE_IMPL_END
