/**CFile****************************************************************

  FileName    [demo.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [ABC as a static library.]

  Synopsis    [A demo program illustrating the use of ABC as a static library.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: demo.c,v 1.00 2005/11/14 00:00:00 alanmi Exp $]

***********************************************************************/

#include <stdio.h>
#include <time.h>

#include "base/main/main.h"
#include "base/wlc/wlc.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [The main() procedure.]

  Description [This procedure compiles into a stand-alone program for 
  DAG-aware rewriting of the AIGs. A BLIF or PLA file to be considered
  for rewriting should be given as a command-line argument. Implementation 
  of the rewriting is inspired by the paper: Per Bjesse, Arne Boralv, 
  "DAG-aware circuit compression for formal verification", Proc. ICCAD 2004.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int main( int argc, char * argv[] )
{
    // parameters
    int fUseResyn2  = 0;
    int fPrintStats = 1;
    int fVerify     = 1;
    // variables
    Abc_Frame_t * pAbc;
    char * pFileName;
    char Command[1000];
    clock_t clkRead, clkResyn, clkVer, clk;

    //////////////////////////////////////////////////////////////////////////
    // get the input file name
    if ( argc != 2 )
    {
        printf( "Wrong number of command-line arguments.\n" );
        return 1;
    }
    pFileName = argv[1];

    //////////////////////////////////////////////////////////////////////////
    // start the ABC framework
    Abc_Start();
    pAbc = Abc_FrameGetGlobalFrame();

clk = clock();
    //////////////////////////////////////////////////////////////////////////
    // read the file
    // sprintf( Command, "read %s", pFileName );
    // if ( Cmd_CommandExecute( pAbc, Command ) )
    // {
    //     fprintf( stdout, "Cannot execute command \"%s\".\n", Command );
    //     return 1;
    // }

    //////////////////////////////////////////////////////////////////////////
    // parse the file
    Gia_Man_t * pNew;
    Wlc_Ntk_t * pNtk = Wlc_ReadVer( pFileName, NULL );
    if ( pNtk == NULL )
        return 1;
    Wlc_WriteVer( pNtk, pFileName, 0, 0 );

    pNew = Wlc_NtkBitBlast( pNtk, NULL );
    Gia_AigerWrite( pNew, "test.aig", 0, 0, 0 );
    Gia_ManStop( pNew );

    Wlc_NtkFree( pNtk );

    //////////////////////////////////////////////////////////////////////////
    // balance
    sprintf( Command, "balance" );
    if ( Cmd_CommandExecute( pAbc, Command ) )
    {
        fprintf( stdout, "Cannot execute command \"%s\".\n", Command );
        return 1;
    }
clkRead = clock() - clk;

    //////////////////////////////////////////////////////////////////////////
    // print stats
    if ( fPrintStats )
    {
        sprintf( Command, "print_stats" );
        if ( Cmd_CommandExecute( pAbc, Command ) )
        {
            fprintf( stdout, "Cannot execute command \"%s\".\n", Command );
            return 1;
        }
    }

clk = clock();
    //////////////////////////////////////////////////////////////////////////
    // synthesize
    if ( fUseResyn2 )
    {
        sprintf( Command, "balance; rewrite -l; refactor -l; balance; rewrite -l; rewrite -lz; balance; refactor -lz; rewrite -lz; balance" );
        if ( Cmd_CommandExecute( pAbc, Command ) )
        {
            fprintf( stdout, "Cannot execute command \"%s\".\n", Command );
            return 1;
        }
    }
    else
    {
        sprintf( Command, "balance; rewrite -l; rewrite -lz; balance; rewrite -lz; balance" );
        if ( Cmd_CommandExecute( pAbc, Command ) )
        {
            fprintf( stdout, "Cannot execute command \"%s\".\n", Command );
            return 1;
        }
    }
clkResyn = clock() - clk;

    //////////////////////////////////////////////////////////////////////////
    // print stats
    if ( fPrintStats )
    {
        sprintf( Command, "print_stats" );
        if ( Cmd_CommandExecute( pAbc, Command ) )
        {
            fprintf( stdout, "Cannot execute command \"%s\".\n", Command );
            return 1;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // write the result in blif
    sprintf( Command, "write_blif result.blif" );
    if ( Cmd_CommandExecute( pAbc, Command ) )
    {
        fprintf( stdout, "Cannot execute command \"%s\".\n", Command );
        return 1;
    }

    //////////////////////////////////////////////////////////////////////////
    // perform verification
clk = clock();
    if ( fVerify )
    {
        sprintf( Command, "cec %s result.blif", pFileName );
        if ( Cmd_CommandExecute( pAbc, Command ) )
        {
            fprintf( stdout, "Cannot execute command \"%s\".\n", Command );
            return 1;
        }
    }
clkVer = clock() - clk;

    printf( "Reading = %6.2f sec   ",     (float)(clkRead)/(float)(CLOCKS_PER_SEC) );
    printf( "Rewriting = %6.2f sec   ",   (float)(clkResyn)/(float)(CLOCKS_PER_SEC) );
    printf( "Verification = %6.2f sec\n", (float)(clkVer)/(float)(CLOCKS_PER_SEC) );

    //////////////////////////////////////////////////////////////////////////
    // stop the ABC framework
    Abc_Stop();
    return 0;
}

