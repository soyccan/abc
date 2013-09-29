/**CFile****************************************************************

  FileName    [giaBalance.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Scalable AIG package.]

  Synopsis    [AIG balancing.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: giaBalance.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "gia.h"
#include "misc/vec/vecHash.h"
#include "misc/vec/vecQue.h"

ABC_NAMESPACE_IMPL_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

// operation manager
typedef struct Dam_Man_t_ Dam_Man_t;
struct Dam_Man_t_
{
    Gia_Man_t *      pGia;      // user's AIG
    Vec_Int_t *      vNod2Set;  // node ID into fanin set
    Vec_Int_t *      vDiv2Nod;  // div ID into fanin set
    Vec_Int_t *      vSetStore; // stored multisets
    Vec_Int_t *      vNodStore; // stored divisors
    Vec_Flt_t *      vCounts;   // occur counts
    Vec_Que_t *      vQue;      // pairs by count
    Hash_IntMan_t *  vHash;     // pair hash table
    abctime          clkStart;  // starting the clock
    int              nDivs;     // extracted divisor count
    int              nAnds;     // total AND node count
    int              nGain;     // total gain in AND nodes
    int              nGainX;    // gain from XOR nodes
};

static inline int    Dam_ObjHand( Dam_Man_t * p, int i )     { return i < Vec_IntSize(p->vNod2Set) ? Vec_IntEntry(p->vNod2Set, i) : 0;                      }
static inline int *  Dam_ObjSet( Dam_Man_t * p, int i )      { int h = Dam_ObjHand(p, i); if ( h == 0 ) return NULL; return Vec_IntEntryP(p->vSetStore, h); }

static inline int    Dam_DivHand( Dam_Man_t * p, int d )     { return d < Vec_IntSize(p->vDiv2Nod) ? Vec_IntEntry(p->vDiv2Nod, d) : 0;                      }
static inline int *  Dam_DivSet( Dam_Man_t * p, int d )      { int h = Dam_DivHand(p, d); if ( h == 0 ) return NULL; return Vec_IntEntryP(p->vNodStore, h); }


////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Simplify multi-input AND/XOR.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimplifyXor( Vec_Int_t * vSuper )
{
    int i, k = 0, Prev = -1, This, fCompl = 0;
    Vec_IntForEachEntry( vSuper, This, i )
    {
        if ( This == 0 )
            continue;
        if ( This == 1 )
            fCompl ^= 1; 
        else if ( Prev != This )
            Vec_IntWriteEntry( vSuper, k++, This ), Prev = This;
        else
            Prev = -1, k--;
    }
    Vec_IntShrink( vSuper, k );
    if ( Vec_IntSize( vSuper ) == 0 )
        Vec_IntPush( vSuper, fCompl );
    else if ( fCompl )
        Vec_IntWriteEntry( vSuper, 0, Abc_LitNot(Vec_IntEntry(vSuper, 0)) );
}
void Gia_ManSimplifyAnd( Vec_Int_t * vSuper )
{
    int i, k = 0, Prev = -1, This;
    Vec_IntForEachEntry( vSuper, This, i )
    {
        if ( This == 0 )
            { Vec_IntFill(vSuper, 1, 0); return; }
        if ( This == 1 )
            continue;
        if ( Prev == -1 || Abc_Lit2Var(Prev) != Abc_Lit2Var(This) )
            Vec_IntWriteEntry( vSuper, k++, This ), Prev = This;
        else if ( Prev != This )
            { Vec_IntFill(vSuper, 1, 0); return; }
    }
    Vec_IntShrink( vSuper, k );
    if ( Vec_IntSize( vSuper ) == 0 )
        Vec_IntPush( vSuper, 1 );
}

/**Function*************************************************************

  Synopsis    [Collect multi-input AND/XOR.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSuperCollectXor_rec( Gia_Man_t * p, Gia_Obj_t * pObj )
{
    assert( !Gia_IsComplement(pObj) );
    if ( !Gia_ObjIsXor(pObj) || Gia_ObjRefNum(p, pObj) > 1 || Vec_IntSize(p->vSuper) > 100 )
    {
        Vec_IntPush( p->vSuper, Gia_ObjToLit(p, pObj) );
        return;
    }
    assert( !Gia_ObjFaninC0(pObj) && !Gia_ObjFaninC1(pObj) );
    Gia_ManSuperCollectXor_rec( p, Gia_ObjFanin0(pObj) );
    Gia_ManSuperCollectXor_rec( p, Gia_ObjFanin1(pObj) );
}
void Gia_ManSuperCollectAnd_rec( Gia_Man_t * p, Gia_Obj_t * pObj )
{
    if ( Gia_IsComplement(pObj) || !Gia_ObjIsAndReal(p, pObj) || Gia_ObjRefNum(p, pObj) > 1 || Vec_IntSize(p->vSuper) > 100 )
    {
        Vec_IntPush( p->vSuper, Gia_ObjToLit(p, pObj) );
        return;
    }
    Gia_ManSuperCollectAnd_rec( p, Gia_ObjChild0(pObj) );
    Gia_ManSuperCollectAnd_rec( p, Gia_ObjChild1(pObj) );
}
void Gia_ManSuperCollect( Gia_Man_t * p, Gia_Obj_t * pObj )
{
//    int nSize;
    if ( p->vSuper == NULL )
        p->vSuper = Vec_IntAlloc( 1000 );
    else
        Vec_IntClear( p->vSuper );
    if ( Gia_ObjIsXor(pObj) )
    {
        assert( !Gia_ObjFaninC0(pObj) && !Gia_ObjFaninC1(pObj) );
        Gia_ManSuperCollectXor_rec( p, Gia_ObjFanin0(pObj) );
        Gia_ManSuperCollectXor_rec( p, Gia_ObjFanin1(pObj) );
//        nSize = Vec_IntSize(vSuper);
        Vec_IntSort( p->vSuper, 0 );
        Gia_ManSimplifyXor( p->vSuper );
//        if ( nSize != Vec_IntSize(vSuper) )
//            printf( "X %d->%d  ", nSize, Vec_IntSize(vSuper) );
    }
    else if ( Gia_ObjIsAndReal(p, pObj) )
    {
        Gia_ManSuperCollectAnd_rec( p, Gia_ObjChild0(pObj) );
        Gia_ManSuperCollectAnd_rec( p, Gia_ObjChild1(pObj) );
//        nSize = Vec_IntSize(vSuper);
        Vec_IntSort( p->vSuper, 0 );
        Gia_ManSimplifyAnd( p->vSuper );
//        if ( nSize != Vec_IntSize(vSuper) )
//            printf( "A %d->%d  ", nSize, Vec_IntSize(vSuper) );
    }
    else assert( 0 );
//    if ( nSize > 10 )
//        printf( "%d ", nSize );
    assert( Vec_IntSize(p->vSuper) > 0 );
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManCreateGate( Gia_Man_t * pNew, Gia_Obj_t * pObj, Vec_Int_t * vSuper )
{
    int iLit0 = Vec_IntPop(vSuper);
    int iLit1 = Vec_IntPop(vSuper);
    int iLit, i;
    if ( !Gia_ObjIsXor(pObj) )
        iLit = Gia_ManHashAnd( pNew, iLit0, iLit1 );
    else if ( pNew->pMuxes )
        iLit = Gia_ManHashXorReal( pNew, iLit0, iLit1 );
    else 
        iLit = Gia_ManHashXor( pNew, iLit0, iLit1 );
    Vec_IntPush( vSuper, iLit );
    Gia_ObjSetGateLevel( pNew, Gia_ManObj(pNew, Abc_Lit2Var(iLit)) );
    // shift to the corrent location
    for ( i = Vec_IntSize(vSuper)-1; i > 0; i-- )
    {
        int iLit1 = Vec_IntEntry(vSuper, i);
        int iLit2 = Vec_IntEntry(vSuper, i-1);
        if ( Gia_ObjLevelId(pNew, Abc_Lit2Var(iLit1)) <= Gia_ObjLevelId(pNew, Abc_Lit2Var(iLit2)) )
            break;
        Vec_IntWriteEntry( vSuper, i,   iLit2 );
        Vec_IntWriteEntry( vSuper, i-1, iLit1 );
    }
}
int Gia_ManBalanceGate( Gia_Man_t * pNew, Gia_Obj_t * pObj, Vec_Int_t * vSuper, int * pLits, int nLits )
{
    Vec_IntClear( vSuper );
    if ( nLits == 1 )
        Vec_IntPush( vSuper, pLits[0] );
    else if ( nLits == 2 )
    {
        Vec_IntPush( vSuper, pLits[0] );
        Vec_IntPush( vSuper, pLits[1] );
        Gia_ManCreateGate( pNew, pObj, vSuper );
    }
    else if ( nLits > 2 )
    {
        // collect levels
        int i, * pArray, * pPerm;
        for ( i = 0; i < nLits; i++ )
            Vec_IntPush( vSuper, Gia_ObjLevelId(pNew, Abc_Lit2Var(pLits[i])) );
        // sort by level
        Vec_IntGrow( vSuper, 4 * nLits );        
        pArray = Vec_IntArray( vSuper );
        pPerm = pArray + nLits;
        Abc_QuickSortCostData( pArray, nLits, 1, (word *)(pArray + 2 * nLits), pPerm );
        // collect in the increasing order of level
        for ( i = 0; i < nLits; i++ )
            Vec_IntWriteEntry( vSuper, i, pLits[pPerm[i]] );
        Vec_IntShrink( vSuper, nLits );
//        Vec_IntForEachEntry( vSuper, iLit, i )
//            printf( "%d ", Gia_ObjLevel(pNew, Gia_ManObj( pNew, Abc_Lit2Var(iLit) )) );
//        printf( "\n" );
        // perform incremental extraction
        while ( Vec_IntSize(vSuper) > 1 )
            Gia_ManCreateGate( pNew, pObj, vSuper );
    }
    // consider trivial case
    assert( Vec_IntSize(vSuper) == 1 );
    return Vec_IntEntry(vSuper, 0);
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManBalance_rec( Gia_Man_t * pNew, Gia_Man_t * p, Gia_Obj_t * pObj )
{
    int i, iLit, iBeg, iEnd;
    if ( ~pObj->Value )
        return;
    assert( Gia_ObjIsAnd(pObj) );
    // handle MUX
    if ( Gia_ObjIsMux(p, pObj) )
    {
        Gia_ManBalance_rec( pNew, p, Gia_ObjFanin0(pObj) );
        Gia_ManBalance_rec( pNew, p, Gia_ObjFanin1(pObj) );
        Gia_ManBalance_rec( pNew, p, Gia_ObjFanin2(p, pObj) );
        pObj->Value = Gia_ManHashMuxReal( pNew, Gia_ObjFanin2Copy(p, pObj), Gia_ObjFanin1Copy(pObj), Gia_ObjFanin0Copy(pObj) );
        Gia_ObjSetGateLevel( pNew, Gia_ManObj(pNew, Abc_Lit2Var(pObj->Value)) );
        return;
    }
    // find supergate
    Gia_ManSuperCollect( p, pObj );
    // save entries
    if ( p->vStore == NULL )
        p->vStore = Vec_IntAlloc( 1000 );
    iBeg = Vec_IntSize( p->vStore );
    Vec_IntAppend( p->vStore, p->vSuper );
    iEnd = Vec_IntSize( p->vStore );
    // call recursively
    Vec_IntForEachEntryStartStop( p->vStore, iLit, i, iBeg, iEnd )
    {
        Gia_Obj_t * pTemp = Gia_ManObj( p, Abc_Lit2Var(iLit) );
        Gia_ManBalance_rec( pNew, p, pTemp );
        Vec_IntWriteEntry( p->vStore, i, Abc_LitNotCond(pTemp->Value, Abc_LitIsCompl(iLit)) );
    }
    assert( Vec_IntSize(p->vStore) == iEnd );
    // consider general case
    pObj->Value = Gia_ManBalanceGate( pNew, pObj, p->vSuper, Vec_IntEntryP(p->vStore, iBeg), iEnd-iBeg );
    Vec_IntShrink( p->vStore, iBeg );
}
Gia_Man_t * Gia_ManBalanceInt( Gia_Man_t * p )
{
    Gia_Man_t * pNew, * pTemp;
    Gia_Obj_t * pObj;
    int i;
    Gia_ManFillValue( p );
    Gia_ManCreateRefs( p ); 
    // start the new manager
    pNew = Gia_ManStart( Gia_ManObjNum(p) );
    pNew->pName = Abc_UtilStrsav( p->pName );
    pNew->pSpec = Abc_UtilStrsav( p->pSpec );
    pNew->pMuxes = ABC_CALLOC( unsigned, pNew->nObjsAlloc );
    pNew->vLevels = Vec_IntStart( pNew->nObjsAlloc );
    // create constant and inputs
    Gia_ManConst0(p)->Value = 0;
    Gia_ManForEachCi( p, pObj, i )
        pObj->Value = Gia_ManAppendCi( pNew );
    // create internal nodes
    Gia_ManHashStart( pNew );
    Gia_ManForEachCo( p, pObj, i )
    {
        Gia_ManBalance_rec( pNew, p, Gia_ObjFanin0(pObj) );
        pObj->Value = Gia_ManAppendCo( pNew, Gia_ObjFanin0Copy(pObj) );
    }
    assert( Gia_ManObjNum(pNew) <= Gia_ManObjNum(p) );
    Gia_ManHashStop( pNew );
    Gia_ManSetRegNum( pNew, Gia_ManRegNum(p) );
    // perform cleanup
    pNew = Gia_ManCleanup( pTemp = pNew );
    Gia_ManStop( pTemp );
    return pNew;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_Man_t * Gia_ManBalance( Gia_Man_t * p, int fSimpleAnd, int fVerbose )
{
    Gia_Man_t * pNew, * pNew1, * pNew2;
    if ( fVerbose )      Gia_ManPrintStats( p, NULL );
    pNew = fSimpleAnd ? Gia_ManDup( p ) : Gia_ManDupMuxes( p );
    if ( fVerbose )      Gia_ManPrintStats( pNew, NULL );
    pNew1 = Gia_ManBalanceInt( pNew );
    if ( fVerbose )      Gia_ManPrintStats( pNew1, NULL );
    Gia_ManStop( pNew );
    pNew2 = Gia_ManDupNoMuxes( pNew1 );
    if ( fVerbose )      Gia_ManPrintStats( pNew2, NULL );
    Gia_ManStop( pNew1 );
    return pNew2;
}





/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Dam_Man_t * Dam_ManAlloc( Gia_Man_t * pGia )
{
    Dam_Man_t * p;
    p = ABC_CALLOC( Dam_Man_t, 1 );
    p->clkStart = Abc_Clock();
    p->pGia = pGia;
    return p;
}
void Dam_ManFree( Dam_Man_t * p )
{   
    Vec_IntFreeP( &p->vNod2Set );
    Vec_IntFreeP( &p->vDiv2Nod );
    Vec_IntFreeP( &p->vSetStore );
    Vec_IntFreeP( &p->vNodStore );
    Vec_FltFreeP( &p->vCounts );
    Vec_QueFreeP( &p->vQue );
    Hash_IntManStop( p->vHash );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Collect initial multi-input gates.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dam_ManCollectSets_rec( Dam_Man_t * p, int Id )
{
    Gia_Obj_t * pObj;
    int i, iBeg, iEnd, iLit;
    if ( Dam_ObjHand(p, Id) || Id == 0 )
        return;
    pObj = Gia_ManObj(p->pGia, Id);
    if ( Gia_ObjIsCi(pObj) )
        return;
    if ( Gia_ObjIsMux(p->pGia, pObj) )
    {
        Dam_ManCollectSets_rec( p, Gia_ObjFaninId0(pObj, Id) );
        Dam_ManCollectSets_rec( p, Gia_ObjFaninId1(pObj, Id) );
        Dam_ManCollectSets_rec( p, Gia_ObjFaninId2(p->pGia, Id) );
        p->nAnds += 3;
        return;
    }
    Gia_ManSuperCollect( p->pGia, pObj );
    Vec_IntWriteEntry( p->vNod2Set, Id, Vec_IntSize(p->vSetStore) );
    Vec_IntPush( p->vSetStore, Vec_IntSize(p->pGia->vSuper) );
    p->nAnds += (1 + 2 * Gia_ObjIsXor(pObj)) * (Vec_IntSize(p->pGia->vSuper) - 1);
    // save entries
    iBeg = Vec_IntSize( p->vSetStore );
    Vec_IntAppend( p->vSetStore, p->pGia->vSuper );
    iEnd = Vec_IntSize( p->vSetStore );
    // call recursively
    Vec_IntForEachEntryStartStop( p->vSetStore, iLit, i, iBeg, iEnd )
        Dam_ManCollectSets_rec( p, Abc_Lit2Var(iLit) );
}
void Dam_ManCollectSets( Dam_Man_t * p )
{
    Gia_Obj_t * pObj;
    int i;
    Gia_ManCreateRefs( p->pGia ); 
    p->vNod2Set  = Vec_IntStart( Gia_ManObjNum(p->pGia) );
    p->vSetStore = Vec_IntAlloc( Gia_ManObjNum(p->pGia) );
    Vec_IntPush( p->vSetStore, -1 );
    Gia_ManForEachCo( p->pGia, pObj, i )
        Dam_ManCollectSets_rec( p, Gia_ObjFaninId0p(p->pGia, pObj) );
    ABC_FREE( p->pGia->pRefs );
}

/**Function*************************************************************

  Synopsis    [Create divisors.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dam_ManCreateMultiRefs( Dam_Man_t * p, Vec_Int_t ** pvRefsAnd, Vec_Int_t ** pvRefsXor )  
{
    Vec_Int_t * vRefsAnd, * vRefsXor;
    Gia_Obj_t * pObj;
    int i, k, * pSet;
    vRefsAnd = Vec_IntStart( 2 * Gia_ManObjNum(p->pGia) );
    vRefsXor = Vec_IntStart( Gia_ManObjNum(p->pGia) );
    Gia_ManForEachAnd( p->pGia, pObj, i )
    {
        if ( !Dam_ObjHand(p, i) )
            continue;
        pSet = Dam_ObjSet(p, i);
        if ( Gia_ObjIsXor(pObj) )
            for ( k = 1; k <= pSet[0]; k++ )
            {
                assert( !Abc_LitIsCompl(pSet[k]) );
                Vec_IntAddToEntry( vRefsXor, Abc_Lit2Var(pSet[k]), 1 );
            }
        else if ( Gia_ObjIsAndReal(p->pGia, pObj) )
            for ( k = 1; k <= pSet[0]; k++ )
                Vec_IntAddToEntry( vRefsAnd, pSet[k], 1 );
        else assert( 0 );
    }
    *pvRefsAnd = vRefsAnd;
    *pvRefsXor = vRefsXor;
}
void Dam_ManCreatePairs( Dam_Man_t * p, int fVerbose )
{
    Gia_Obj_t * pObj;
    Hash_IntMan_t * vHash;
    Vec_Int_t * vRefsAnd, * vRefsXor, * vSuper, * vDivs, * vRemap;
    int i, j, k, Num, FanK, FanJ, nRefs, iNode, iDiv, * pSet;
    int nPairsAll = 0, nPairsTried = 0, nPairsUsed = 0, nPairsXor = 0;
    int nDivsAll = 0, nDivsUsed = 0, nDivsXor = 0;
    Dam_ManCollectSets( p );
    vSuper = p->pGia->vSuper;
    vDivs  = Vec_IntAlloc( Gia_ManObjNum(p->pGia) );
    vHash  = Hash_IntManStart( Gia_ManObjNum(p->pGia) );
    Dam_ManCreateMultiRefs( p, &vRefsAnd, &vRefsXor );
    Gia_ManForEachAnd( p->pGia, pObj, i )
    {
        if ( !Dam_ObjHand(p, i) )
            continue;
        pSet = Dam_ObjSet(p, i);
        nPairsAll += pSet[0] * (pSet[0] - 1) / 2;
        Vec_IntClear(vSuper);
        if ( Gia_ObjIsXor(pObj) )
        {
//            printf( "%d -> ", pSet[0] );
            for ( k = 1; k <= pSet[0]; k++ )
                if ( Vec_IntEntry(vRefsXor, Abc_Lit2Var(pSet[k])) > 1 )
                    Vec_IntPush( vSuper, pSet[k] );
//            printf( "%d    ", Vec_IntSize(vSuper) );
        }
        else if ( Gia_ObjIsAndReal(p->pGia, pObj) )
        {
            for ( k = 1; k <= pSet[0]; k++ )
                if ( Vec_IntEntry(vRefsAnd, pSet[k]) > 1 )
                    Vec_IntPush( vSuper, pSet[k] );
        }
        else assert( 0 );
        if ( Vec_IntSize(vSuper) < 2 )
            continue;
        // enumerate pairs
        nPairsTried += Vec_IntSize(vSuper) * (Vec_IntSize(vSuper) - 1) / 2;
        Vec_IntPush( vDivs, -i ); // remember node
        Vec_IntForEachEntry( vSuper, FanK, k )
        Vec_IntForEachEntryStart( vSuper, FanJ, j, k+1 )
        {
            if ( (FanK > FanJ) ^ Gia_ObjIsXor(pObj) )
                Num = Hash_Int2ManInsert( vHash, FanJ, FanK, 0 );
            else
                Num = Hash_Int2ManInsert( vHash, FanK, FanJ, 0 );
            if ( Hash_Int2ObjInc( vHash, Num ) == 1 )
            {
                nDivsUsed++;
                nDivsXor += Gia_ObjIsXor(pObj);
            }
            Vec_IntPush( vDivs, Num ); // remember devisor
        }
    }
    Vec_IntFree( vRefsAnd );
    Vec_IntFree( vRefsXor );
    // remove entries that appear only once
    p->vHash     = Hash_IntManStart( 2 * nDivsUsed );
    p->vCounts   = Vec_FltAlloc( 2 * nDivsUsed );           Vec_FltPush( p->vCounts, ABC_INFINITY );
    p->vQue      = Vec_QueAlloc( Vec_FltCap(p->vCounts) );
    Vec_QueSetCosts( p->vQue, Vec_FltArrayP(p->vCounts) );
    // mapping div to node
    p->vDiv2Nod  = Vec_IntAlloc( 2 * nDivsUsed );           Vec_IntPush( p->vDiv2Nod, ABC_INFINITY );
    p->vNodStore = Vec_IntAlloc( Gia_ManObjNum(p->pGia) );  Vec_IntPush( p->vNodStore, -1 );
    nDivsAll     = Hash_IntManEntryNum(vHash);
    vRemap       = Vec_IntStartFull( nDivsAll+1 );
    for ( i = 1; i <= nDivsAll; i++ )
    {
        nRefs = Hash_IntObjData2(vHash, i);
        if ( nRefs < 2 )
            continue;
        nPairsUsed += nRefs;
        if ( Hash_IntObjData0(vHash, i) > Hash_IntObjData1(vHash, i) )
            nPairsXor += nRefs; 
        Num = Hash_Int2ManInsert( p->vHash, Hash_IntObjData0(vHash, i), Hash_IntObjData1(vHash, i), 0 );
        assert( Num == Hash_IntManEntryNum(p->vHash) );
        assert( Num == Vec_FltSize(p->vCounts) );
        Vec_FltPush( p->vCounts, nRefs-1 );
        Vec_QuePush( p->vQue, Num );
        // remember divisors
        assert( Num == Vec_IntSize(p->vDiv2Nod) );
        Vec_IntPush( p->vDiv2Nod, Vec_IntSize(p->vNodStore) );
        Vec_IntPush( p->vNodStore, 0 );
        Vec_IntFillExtra( p->vNodStore, Vec_IntSize(p->vNodStore) + nRefs, -1 );
        // remember entry
        Vec_IntWriteEntry( vRemap, i, Num );
    }
    assert( Vec_FltSize(p->vCounts) == Hash_IntManEntryNum(p->vHash)+1 );
    assert( Vec_IntSize(p->vDiv2Nod) == nDivsUsed+1 );
    Hash_IntManStop( vHash );
    // fill in the divisors
    iNode = -1;
    Vec_IntForEachEntry( vDivs, iDiv, i )
    {
        if ( iDiv < 0 )
        {
            iNode = -iDiv;
            continue;
        }
        Num = Vec_IntEntry( vRemap, iDiv );
        if ( Num == -1 )
            continue;
        pSet = Dam_DivSet( p, Num );
        assert( pSet[0] <= Vec_FltEntry(p->vCounts, Num) );
        pSet[++pSet[0]] = iNode;
    }
    Vec_IntFree( vRemap );
    Vec_IntFree( vDivs );
    // make sure divisors are added correctly
    for ( i = 1; i <= nDivsUsed; i++ )
        assert( Dam_DivSet(p, i)[0] == Vec_FltEntry(p->vCounts, i)+1 );
    if ( !fVerbose )
        return;
    // print stats
    printf( "Pairs:" );
    printf( "  Total =%9d (%6.2f %%)", nPairsAll,   100.0 * nPairsAll   / Abc_MaxInt(nPairsAll, 1) );
    printf( "  Tried =%9d (%6.2f %%)", nPairsTried, 100.0 * nPairsTried / Abc_MaxInt(nPairsAll, 1) );
    printf( "  Used =%9d (%6.2f %%)",  nPairsUsed,  100.0 * nPairsUsed  / Abc_MaxInt(nPairsAll, 1) );
    printf( "  Xor =%9d (%6.2f %%)",   nPairsXor,   100.0 * nPairsXor   / Abc_MaxInt(nPairsAll, 1) );
    printf( "\n" );
    printf( "Div:  " );
    printf( "  Total =%9d (%6.2f %%)", nDivsAll,    100.0 * nDivsAll    / Abc_MaxInt(nDivsAll, 1) );
    printf( "  Tried =%9d (%6.2f %%)", nDivsAll,    100.0 * nDivsAll    / Abc_MaxInt(nDivsAll, 1) );
    printf( "  Used =%9d (%6.2f %%)",  nDivsUsed,   100.0 * nDivsUsed   / Abc_MaxInt(nDivsAll, 1) );
    printf( "  Xor =%9d (%6.2f %%)",   nDivsXor,    100.0 * nDivsXor    / Abc_MaxInt(nDivsAll, 1) );
    printf( "\n" );
}

/**Function*************************************************************

  Synopsis    [Derives new AIG.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dam_ManMultiAig_rec( Dam_Man_t * pMan, Gia_Man_t * pNew, Gia_Man_t * p, Gia_Obj_t * pObj )
{
    int i, * pSet;
    if ( ~pObj->Value )
        return;
    assert( Gia_ObjIsAnd(pObj) );
    pSet = Dam_ObjSet(pMan, Gia_ObjId(p, pObj));
    if ( pSet == NULL )
    {
        Dam_ManMultiAig_rec( pMan, pNew, p, Gia_ObjFanin0(pObj) );
        Dam_ManMultiAig_rec( pMan, pNew, p, Gia_ObjFanin1(pObj) );
        if ( Gia_ObjIsMux(p, pObj) )
        {
            Dam_ManMultiAig_rec( pMan, pNew, p, Gia_ObjFanin2(p, pObj) );
            pObj->Value = Gia_ManHashMuxReal( pNew, Gia_ObjFanin2Copy(p, pObj), Gia_ObjFanin1Copy(pObj), Gia_ObjFanin0Copy(pObj) );
        }
        else if ( Gia_ObjIsXor(pObj) )
            pObj->Value = Gia_ManHashXorReal( pNew, Gia_ObjFanin0Copy(pObj), Gia_ObjFanin1Copy(pObj) );
        else 
            pObj->Value = Gia_ManHashAnd( pNew, Gia_ObjFanin0Copy(pObj), Gia_ObjFanin1Copy(pObj) );
        Gia_ObjSetGateLevel( pNew, Gia_ManObj(pNew, Abc_Lit2Var(pObj->Value)) );
        return;
    }
    assert( Gia_ObjIsXor(pObj) || Gia_ObjIsAndReal(p, pObj) );
    // call recursively
    for ( i = 1; i <= pSet[0]; i++ )
    {
        Gia_Obj_t * pTemp = Gia_ManObj( p, Abc_Lit2Var(pSet[i]) );
        Dam_ManMultiAig_rec( pMan, pNew, p, pTemp );
        pSet[i] = Abc_LitNotCond( pTemp->Value, Abc_LitIsCompl(pSet[i]) );
    }
    // create balanced gate
    pObj->Value = Gia_ManBalanceGate( pNew, pObj, p->vSuper, pSet + 1, pSet[0] );
}
Gia_Man_t * Dam_ManMultiAig( Dam_Man_t * pMan )
{
    Gia_Man_t * p = pMan->pGia;
    Gia_Man_t * pNew, * pTemp;
    Gia_Obj_t * pObj;
    int i;
    // start the new manager
    pNew = Gia_ManStart( Gia_ManObjNum(p) );
    pNew->pName = Abc_UtilStrsav( p->pName );
    pNew->pSpec = Abc_UtilStrsav( p->pSpec );
    pNew->pMuxes = ABC_CALLOC( unsigned, pNew->nObjsAlloc );
    pNew->vLevels = Vec_IntStart( pNew->nObjsAlloc );
    // create constant and inputs
    Gia_ManFillValue( p );
    Gia_ManConst0(p)->Value = 0;
    Gia_ManForEachCi( p, pObj, i )
        pObj->Value = Gia_ManAppendCi( pNew );
    // create internal nodes
    Gia_ManHashStart( pNew );
    Gia_ManForEachCo( p, pObj, i )
    {
        Dam_ManMultiAig_rec( pMan, pNew, p, Gia_ObjFanin0(pObj) );
        pObj->Value = Gia_ManAppendCo( pNew, Gia_ObjFanin0Copy(pObj) );
    }
    assert( Gia_ManObjNum(pNew) <= Gia_ManObjNum(p) );
    Gia_ManHashStop( pNew );
    Gia_ManSetRegNum( pNew, Gia_ManRegNum(p) );
    // perform cleanup
    pNew = Gia_ManCleanup( pTemp = pNew );
    Gia_ManStop( pTemp );
    return pNew;
}

/**Function*************************************************************

  Synopsis    [Updates the data-structure after extracting one divisor.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Dam_PrintDiv( Dam_Man_t * p, int iDiv )
{
    if ( iDiv == 0 )
        printf( "Final statistics after extracting %6d divisors:          ", p->nDivs );
    else
    {
        char Buffer[100];
        int iData0 = Hash_IntObjData0(p->vHash, iDiv);
        int iData1 = Hash_IntObjData1(p->vHash, iDiv);
        printf( "Div%5d : ",  p->nDivs+1 );
        printf( "D%-8d = ",   iDiv );
        sprintf( Buffer, "%c%d", Abc_LitIsCompl(iData0)? '!':' ', Abc_Lit2Var(iData0) );
        printf( "%8s ",   Buffer );
        printf( "%c  ",     (iData0 < iData1) ? '*' : '+' );
        sprintf( Buffer, "%c%d", Abc_LitIsCompl(iData1)? '!':' ', Abc_Lit2Var(iData1) );
        printf( "%8s   ", Buffer );
        printf( "Weight %5d  ", (int)Vec_FltEntry(p->vCounts, iDiv) );
    }
    printf( "Divs =%8d  ",  Hash_IntManEntryNum(p->vHash) );
    printf( "Ands =%8d  ",  p->nAnds - p->nGain );
    Abc_PrintTime( 1, "Time", Abc_Clock() - p->clkStart );
}
void Dam_PrintQue( Dam_Man_t * p )
{
    int i;
    printf( "Divisor queue: \n" );
    for ( i = 1; i <= Hash_IntManEntryNum(p->vHash); i++ )
    {
        int iLit0 = Hash_IntObjData0(p->vHash, i);
        int iLit1 = Hash_IntObjData1(p->vHash, i);
        printf( "Div %7d : ",   i );
        printf( "Weight %5d  ", (int)Vec_FltEntry(p->vCounts, i) );
        printf( "F = %c%c ",    Abc_LitIsCompl(iLit0) ? '!': ' ', 'a' + Abc_Lit2Var(iLit0)-1 );
        printf( "%c ",          (Hash_IntObjData0(p->vHash, i) < Hash_IntObjData1(p->vHash, i)) ? '*':'+' );
        printf( "%c%c   ",      Abc_LitIsCompl(iLit1) ? '!': ' ', 'a' + Abc_Lit2Var(iLit1)-1 );
        printf( "\n" );
    }
}
int Dam_ManUpdateNode( Dam_Man_t * p, int iObj, int iLit0, int iLit1, int iLitNew )
{
    int i, k, c, Num, iLit, iLit2;
    int * pSet = Dam_ObjSet( p, iObj );
    // check if literal can be found
    for ( i = 1; i <= pSet[0]; i++ )
        if ( pSet[i] == iLit0 )
            break;
    if ( i > pSet[0] )
        return 0;
    // check if literal can be found
    for ( i = 1; i <= pSet[0]; i++ )
        if ( pSet[i] == iLit1 )
            break;
    if ( i > pSet[0] )
        return 0;
    // compact literals
    for ( k = i = 1; i <= pSet[0]; i++ )
    {
        if ( iLit0 == pSet[i] || iLit1 == pSet[i] )
            continue;
        pSet[k++] = iLit = pSet[i];
        // reduce weights of the divisors
        for ( c = 0; c < 2; c++ )
        {
            iLit2 = c ? iLit1 : iLit0;
            if ( (iLit > iLit2) ^ (iLit0 > iLit1) )
                Num = *Hash_Int2ManLookup( p->vHash, iLit2, iLit );
            else
                Num = *Hash_Int2ManLookup( p->vHash, iLit, iLit2 );
            if ( Num > 0 )
            {
                Vec_FltAddToEntry( p->vCounts, Num, -1 );
                Vec_QueUpdate( p->vQue, Num );
            }
        }
    }
    pSet[k] = iLitNew;
    pSet[0] = k;
    // add new divisors
//    for ( i = 1; i < pSet[0]; i++ )
//    {
//    }
    return 1;
}
void Dam_ManUpdate( Dam_Man_t * p, int iDiv )
{
    int iLit0 = Hash_IntObjData0(p->vHash, iDiv);
    int iLit1 = Hash_IntObjData1(p->vHash, iDiv);
    int i, iLitNew, * pNods = Dam_DivSet( p, iDiv );
    int fThisIsXor = (iLit0 > iLit1);
    int nPresent = 0;
//    Dam_PrintQue( p );
    if ( fThisIsXor )
        iLitNew = Gia_ManAppendXorReal( p->pGia, iLit0, iLit1 );
    else
        iLitNew = Gia_ManAppendAnd( p->pGia, iLit0, iLit1 );
    // replace entries
    assert( pNods[0] >= 2 );
    for ( i = 1; i <= pNods[0]; i++ )
        nPresent += Dam_ManUpdateNode( p, pNods[i], iLit0, iLit1, iLitNew );
    // update reference counters of AND/XOR nodes here!
    // update costs
    Vec_FltWriteEntry( p->vCounts, iDiv, 0 );
    p->nGain += (1 + 2 * fThisIsXor) * (nPresent - 1);
    p->nGainX += 3 * fThisIsXor * (nPresent - 1);
    p->nDivs++;
}

/**Function*************************************************************

  Synopsis    [Perform extraction for multi-input AND/XOR.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_Man_t * Dam_ManMultiExtractInt( Gia_Man_t * pGia, int nNewNodesMax, int fVerbose, int fVeryVerbose )
{
    Gia_Man_t * pNew;
    Dam_Man_t * p;
    int i, iDiv;
    p = Dam_ManAlloc( pGia );
    Dam_ManCreatePairs( p, fVerbose );
    for ( i = 0; i < nNewNodesMax && Vec_QueTopCost(p->vQue) > 0; i++ )
    {
        iDiv = Vec_QuePop(p->vQue);
        if ( fVeryVerbose )
            Dam_PrintDiv( p, iDiv );
        Dam_ManUpdate( p, iDiv );
    }
    if ( fVeryVerbose )
        Dam_PrintDiv( p, 0 );
    pNew = Dam_ManMultiAig( p );
    if ( fVerbose )
    {
        int nDivsAll = Hash_IntManEntryNum(p->vHash);
        int nDivsUsed = p->nDivs;
        printf( "Div:  " );
        printf( "  Total =%9d (%6.2f %%) ",   nDivsAll,   100.0 * nDivsAll    / Abc_MaxInt(nDivsAll, 1) );
        printf( "  Used =%9d (%6.2f %%)",     nDivsUsed,  100.0 * nDivsUsed   / Abc_MaxInt(nDivsAll, 1) );
        printf( "  Gain =%6d (%6.2f %%)",     p->nGain,   100.0 * p->nGain / Abc_MaxInt(p->nAnds, 1) );
        printf( "  GainX = %d  ",             p->nGainX  );
        Abc_PrintTime( 1, "Time", Abc_Clock() - p->clkStart );
    }
    Dam_ManFree( p );
    return pNew;
}
Gia_Man_t * Gia_ManMultiExtract( Gia_Man_t * p, int fSimpleAnd, int nNewNodesMax, int fVerbose, int fVeryVerbose )
{
    Gia_Man_t * pNew, * pNew1, * pNew2;
    if ( fVerbose )     Gia_ManPrintStats( p, NULL );
    pNew = fSimpleAnd ? Gia_ManDup( p ) : Gia_ManDupMuxes( p );
    if ( fVerbose )     Gia_ManPrintStats( pNew, NULL );
    pNew1 = Dam_ManMultiExtractInt( pNew, nNewNodesMax, fVerbose, fVeryVerbose );
    if ( fVerbose )     Gia_ManPrintStats( pNew1, NULL );
    Gia_ManStop( pNew );
    pNew2 = Gia_ManDupNoMuxes( pNew1 );
    if ( fVerbose )     Gia_ManPrintStats( pNew2, NULL );
    Gia_ManStop( pNew1 );
    return pNew2;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END
