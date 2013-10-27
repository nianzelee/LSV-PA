/**CFile****************************************************************

  FileName    [bmcMulti.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [SAT-based bounded model checking.]

  Synopsis    [Proving multi-output properties.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: bmcMulti.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "bmc.h"
#include "proof/ssw/ssw.h"
#include "aig/gia/giaAig.h"

ABC_NAMESPACE_IMPL_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Divides outputs into solved and unsolved.]

  Description [Return array of unsolved outputs to extract into a new AIG.
  Updates the resulting CEXes (vCexesOut) and current output map (vOutMap).]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Int_t * Gia_ManProcessOutputs( Vec_Ptr_t * vCexesIn, Vec_Ptr_t * vCexesOut, Vec_Int_t * vOutMap )
{
    Abc_Cex_t * pCex; 
    Vec_Int_t * vLeftOver;
    int i, iOut;
    assert( Vec_PtrSize(vCexesIn) == Vec_IntSize(vOutMap) );
    vLeftOver = Vec_IntAlloc( Vec_PtrSize(vCexesIn) );
    Vec_IntForEachEntry( vOutMap, iOut, i )
    {
        assert( Vec_PtrEntry(vCexesOut, iOut) == NULL );
        pCex = (Abc_Cex_t *)Vec_PtrEntry( vCexesIn, i );
        if ( pCex ) // found a CEX for output iOut
        {
            Vec_PtrWriteEntry( vCexesIn, i, NULL );
            Vec_PtrWriteEntry( vCexesOut, iOut, pCex );
        }
        else // still unsolved
        {
            Vec_IntWriteEntry( vOutMap, Vec_IntSize(vLeftOver), iOut );
            Vec_IntPush( vLeftOver, i );
        }
    }
    Vec_IntShrink( vOutMap, Vec_IntSize(vLeftOver) );
    return vLeftOver;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManMultiReport( Aig_Man_t * p, char * pStr, int nTotalPo, int nTotalSize, abctime clkStart )
{
    printf( "%3s : ", pStr );
    printf( "PI =%6d  ", Saig_ManPiNum(p) );
    printf( "PO =%6d  ", Saig_ManPoNum(p) );
    printf( "FF =%7d  ", Saig_ManRegNum(p) );
    printf( "ND =%7d  ", Aig_ManNodeNum(p) );
    printf( "Solved =%7d (%5.1f %%)  ", nTotalPo-Saig_ManPoNum(p), 100.0*(nTotalPo-Saig_ManPoNum(p))/Abc_MaxInt(1, nTotalPo) );
    printf( "Size   =%7d (%5.1f %%)  ", Aig_ManObjNum(p),          100.0*Aig_ManObjNum(p)/Abc_MaxInt(1, nTotalSize) );
    Abc_PrintTime( 1, "Time", Abc_Clock() - clkStart );
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Aig_Man_t * Gia_ManMultiProveSyn( Aig_Man_t * p, int fVerbose, int fVeryVerbose )
{
    Aig_Man_t * pAig;
    Gia_Man_t * pGia, * pTemp;
    pGia = Gia_ManFromAig( p );
    pGia = Gia_ManAigSyn2( pTemp = pGia, 0, 0 );
    Gia_ManStop( pTemp );
    pAig = Gia_ManToAig( pGia, 0 );
    Gia_ManStop( pGia );
    return pAig;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Gia_ManMultiProveAig( Aig_Man_t * p, int TimeOutGlo, int TimeOutLoc, int TimeOutInc, int fUseSyn, int fVerbose, int fVeryVerbose )
{
    Ssw_RarPars_t ParsSim, * pParsSim = &ParsSim;
    Saig_ParBmc_t ParsBmc, * pParsBmc = &ParsBmc;
    Vec_Int_t * vOutMap, * vLeftOver;
    Vec_Ptr_t * vCexes;
    Aig_Man_t * pTemp;
    abctime clkStart = Abc_Clock();
    int nTimeToStop  = TimeOutGlo ? TimeOutGlo * CLOCKS_PER_SEC + Abc_Clock(): 0;
    int nTotalPo     = Saig_ManPoNum(p);
    int nTotalSize   = Aig_ManObjNum(p);
    int i, RetValue  = -1;
    if ( fVerbose )
        printf( "MultiProve parameters: Global timeout = %d sec.  Local timeout = %d sec.  Time increase = %d.\n", TimeOutGlo, TimeOutLoc, TimeOutInc );
    // create output map
    vOutMap = Vec_IntStartNatural( Saig_ManPoNum(p) ); // maps current outputs into their original IDs
    vCexes  = Vec_PtrStart( Saig_ManPoNum(p) );        // maps solved outputs into their CEXes (or markers)
    for ( i = 0; i < 1000; i++ )
    {
        // perform SIM3
        Ssw_RarSetDefaultParams( pParsSim );
        pParsSim->fSolveAll = 1;
        pParsSim->fNotVerbose = 1;
        pParsSim->fSilent = !fVeryVerbose;
        pParsSim->TimeOut = TimeOutLoc;
        pParsSim->nRandSeed = (i * 17) % 500;
        RetValue *= Ssw_RarSimulate( p, pParsSim );
        // sort outputs
        if ( p->vSeqModelVec )
        {
            vLeftOver = Gia_ManProcessOutputs( p->vSeqModelVec, vCexes, vOutMap );
            if ( Vec_IntSize(vLeftOver) == 0 )
                break;
            // remove solved        
            p = Saig_ManDupCones( pTemp = p, Vec_IntArray(vLeftOver), Vec_IntSize(vLeftOver) );
            Vec_IntFree( vLeftOver );
            Aig_ManStop( pTemp );
        }
        if ( fVerbose )
            Gia_ManMultiReport( p, "SIM", nTotalPo, nTotalSize, clkStart );

        // check timeout
        if ( nTimeToStop && Abc_Clock() > nTimeToStop )
        {
            printf( "Global timeout (%d sec) is reached.\n", TimeOutGlo );
            break;
        }

        // perform BMC
        Saig_ParBmcSetDefaultParams( pParsBmc );
        pParsBmc->fSolveAll = 1;
        pParsBmc->fNotVerbose = 1;
        pParsBmc->fSilent = !fVeryVerbose;
        pParsBmc->nTimeOut = TimeOutLoc;
        RetValue *= Saig_ManBmcScalable( p, pParsBmc );
        // sort outputs
        if ( p->vSeqModelVec )
        {
            vLeftOver = Gia_ManProcessOutputs( p->vSeqModelVec, vCexes, vOutMap );
            if ( Vec_IntSize(vLeftOver) == 0 )
                break;
            // remove solved        
            p = Saig_ManDupCones( pTemp = p, Vec_IntArray(vLeftOver), Vec_IntSize(vLeftOver) );
            Vec_IntFree( vLeftOver );
            Aig_ManStop( pTemp );
        }
        if ( fVerbose )
            Gia_ManMultiReport( p, "BMC", nTotalPo, nTotalSize, clkStart );

        // check timeout
        if ( nTimeToStop && Abc_Clock() > nTimeToStop )
        {
            printf( "Global timeout (%d sec) is reached.\n", TimeOutGlo );
            break;
        }

        // synthesize
        if ( fUseSyn )
        {
            p = Gia_ManMultiProveSyn( pTemp = p, fVerbose, fVeryVerbose );
            Aig_ManStop( pTemp );
            if ( fVerbose )
                Gia_ManMultiReport( p, "SYN", nTotalPo, nTotalSize, clkStart );
        }

        // increase timeout
        TimeOutLoc *= TimeOutInc;
    }
    Vec_IntFree( vOutMap );
    Aig_ManStop( p );
    return vCexes;
}
int Gia_ManMultiProve( Gia_Man_t * p, int TimeOutGlo, int TimeOutLoc, int TimeOutInc, int fUseSyn, int fVerbose, int fVeryVerbose )
{
    Aig_Man_t * pAig;
    if ( p->vSeqModelVec )
        Vec_PtrFreeFree( p->vSeqModelVec ), p->vSeqModelVec = NULL;
    pAig = Gia_ManToAig( p, 0 );
    p->vSeqModelVec = Gia_ManMultiProveAig( pAig, TimeOutGlo, TimeOutLoc, TimeOutInc, fUseSyn, fVerbose, fVeryVerbose );
    assert( Vec_PtrSize(p->vSeqModelVec) == Gia_ManPoNum(p) );
//    Aig_ManStop( pAig );
    return Vec_PtrCountZero(p->vSeqModelVec) == Vec_PtrSize(p->vSeqModelVec) ? -1 : 0;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END
