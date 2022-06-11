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
  int fVerbose = 0, fSat = 0;
  int c;
  Extra_UtilGetoptReset();
  while ((c = Extra_UtilGetopt(argc, argv, "svh")) != EOF) {
    switch (c) {
    case 's':
      fSat ^= 1;
      break;
    case 'v':
      fVerbose ^= 1;
      break;
    case 'h':
      goto usage;
    default:
      goto usage;
    }
  }

  if (fVerbose) {
    Abc_NtkShow(pNtk, 0, 0, 0);

    Abc_Obj_t* pObj;
    int i;
    Abc_NtkForEachObj(pNtk, pObj, i) {
      Abc_ObjPrint(stderr, pObj);
    }
  }

  unsigned long addend[32];
  int ret;
  if (fSat) { // use SAT
    ret = ExtractAddendSat(pNtk, addend);
  } else { // use BDD
    Rev_NtkAigBuildBddToPi(pNtk);

    DdManager *dd = pNtk->pManFunc;

    if (fVerbose) {
      int i;
      Abc_Obj_t *po;
      Abc_NtkForEachPo(pNtk, po, i) {
        Abc_ObjPrint(stderr, po);

        debug("po id=%d comp=%d", Abc_ObjFanin0(po)->Id, Abc_ObjFaninC0(po));

        DdNode *pFunc = po->pData;

        DdGen *gen;
        int *cube;
        CUDD_VALUE_TYPE value;
        Cudd_ForeachCube(dd, pFunc, gen, cube, value) {
          for (int i = 0; i < dd->size; i++)
            fprintf(stdout, "%c",
                    cube[i] == 0   ? '0'
                    : cube[i] == 1 ? '1'
                                   : '-');

          fprintf(stdout, "\n");
        }

        Abc_NodeShowBdd(po, 0);
      }
      Abc_PrintErr(1, "\n");
    }
    
    ret = ExtractAddendBdd(pNtk, addend);
  }

  assert(ret);
  Abc_Print(1, "Extracted addend: (%lu) ", addend[0]);
  for (int i = 0; i < 32; i++) {
    Abc_Print(1, "%lx ", addend[i]);
  }
  Abc_Print(1, "\n");

  return 0;

usage:
  Abc_Print(-2, "usage: rev [-svh]\n");
  Abc_Print(-2, "\t         extract addend\n");
  Abc_Print(-2, "\t-s     : extract using SAT (default BDD) [default = %s]\n",
            fSat ? "yes" : "no");
  Abc_Print(-2, "\t-v     : print BDD of each PO [default = %s]\n",
            fVerbose ? "yes" : "no");
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
