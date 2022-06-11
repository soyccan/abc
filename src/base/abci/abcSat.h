/**CFile****************************************************************

  FileName    [abcSat.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Network and node package.]

  Synopsis    [Procedures to solve the miter using the internal SAT sat_solver.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: abcSat.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "base/cmd/cmd.h"
#include "sat/bsat/satSolver.h"

#ifdef ABC_USE_CUDD
#include "bdd/extrab/extraBdd.h"
#endif

ABC_NAMESPACE_HEADER_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

Vec_Int_t * Abc_NtkGetCiSatVarNums( Abc_Ntk_t * pNtk );
 
int Abc_NtkClauseTriv( sat_solver * pSat, Abc_Obj_t * pNode, Vec_Int_t * vVars );int Abc_NtkClauseTop( sat_solver * pSat, Vec_Ptr_t * vNodes, Vec_Int_t * vVars );
int Abc_NtkClauseAnd( sat_solver * pSat, Abc_Obj_t * pNode, Vec_Ptr_t * vSuper, Vec_Int_t * vVars );
int Abc_NtkClauseMux( sat_solver * pSat, Abc_Obj_t * pNode, Abc_Obj_t * pNodeC, Abc_Obj_t * pNodeT, Abc_Obj_t * pNodeE, Vec_Int_t * vVars );
int Abc_NtkCollectSupergate_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vSuper, int fFirst, int fStopAtMux );
void Abc_NtkCollectSupergate( Abc_Obj_t * pNode, int fStopAtMux, Vec_Ptr_t * vNodes );
int Abc_NtkNodeFactor( Abc_Obj_t * pObj, int nLevelMax );
int Abc_NtkMiterSatCreateInt( sat_solver * pSat, Abc_Ntk_t * pNtk );

int Abc_NodeAddClauses( sat_solver * pSat, char * pSop0, char * pSop1, Abc_Obj_t * pNode, Vec_Int_t * vVars );
int Abc_NodeAddClausesTop( sat_solver * pSat, Abc_Obj_t * pNode, Vec_Int_t * vVars );
void Abc_NtkWriteSorterCnf( char * pFileName, int nVars, int nQueens );


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_HEADER_END

