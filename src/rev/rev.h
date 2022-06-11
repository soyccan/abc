/**CFile****************************************************************

  FileName    [rev.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Reverse Engineer Datapath]

  Synopsis    [External declarations.]

  Author      [Cheng-Kang Chen]

  Affiliation [National Taiwan University]

  Date        [Ver. 1.0. Started - June 2, 2022.]

  Revision    [$Id: vec.h,v 1.00 2022/06/02 00:00:00 ahurst Exp $]

***********************************************************************/

#ifndef ABC__rev__rev_h
#define ABC__rev__rev_h

#ifdef _WIN32
#define inline __inline // compatible with MS VS 6.0
#endif
////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "base/main/mainInt.h"
#include "bdd/cudd/cuddInt.h"

ABC_NAMESPACE_HEADER_START

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

#define _debug(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define debug(fmt, ...) _debug(fmt "\n", ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////
void Abc_ObjPrintFaninCone(Abc_Obj_t *pObj);
int Rev_NtkAigBuildBddToPi(Abc_Ntk_t *pNtk);
void Rev_AigNodeBuildBddToPi(DdManager *dd, int *bddBuilt, Abc_Obj_t *node);
int ExtractAddendBdd(Abc_Ntk_t *ntk, unsigned long *addend);
int ExtractAddendSat(Abc_Ntk_t *ntk, unsigned long *addend, int fVerbose);

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DEFINITIONS                          ///
////////////////////////////////////////////////////////////////////////

// debug function
static int printBddCubes(DdManager *dd, DdNode *func) {
  DdGen *gen;
  int *cube;
  CUDD_VALUE_TYPE value;
  Cudd_ForeachCube(dd, func, gen, cube, value) {
    for (int i = 0; i < dd->size; i++)
      fprintf(stdout, "%c", cube[i] == 0 ? '0' : cube[i] == 1 ? '1' : '-');

    fprintf(stdout, "\n");
  }
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

ABC_NAMESPACE_HEADER_END

#endif
