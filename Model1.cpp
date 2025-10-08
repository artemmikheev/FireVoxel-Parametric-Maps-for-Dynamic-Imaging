/**
* @file Model1.cpp
* @brief Model 1 — Area Under the Curve (AUC) over a selected TAC segment.
*
* @details
* Implements "1. Area Under the Curve (AUC)". The model converts the input
* time–activity curve (TAC) to concentration units, selects a time window
* via free parameters, and computes the integral of the TAC over absolute
* time for that window. The single output is:
*   - OP[0] Curve integral by time (AUC)
*
* @section params Free Parameters
*   - FP[0] "Start Index" (int): zero-based index of the first frame in the segment.
*   - FP[1] "Length (0=all remaining)" (int): number of frames to include;
*           0 means "from Start Index to the end".
*
* @section io Inputs/Outputs
*   - Input: @c Signal (double[NumTms]) — TAC samples in time order.
*            The function internally calls @c funcSigToConc() to convert
*            signal to concentration before integration.
*   - Time base: @c AbsTarr (double[NumTms]) — absolute frame times used as the
*                integration variable.
*   - Output: @c OutParm — framework writer used by @c Write() to emit OP[0]
*            when requested by @c ParmReq[0].
*
* @section deps Dependencies
* Uses framework utilities/globals (non‑exhaustive):
*   GetStartEndInx, iround, funcSigToConc, PR_CalculateIntegral,
*   AllocMem, pf_free, Write, ParmReq, AbsTarr, NumTms.
*
* @section ts Thread-safety
* Not thread‑safe: writes/reads global indices @c gStart and @c gEnd.
*
* @section mem Memory
* Allocates a temporary TAC buffer; released before return.
*
* @section units Units
* AUC units are [concentration units of @c funcSigToConc()] ×
* [time units of @c AbsTarr] over the selected window.
*
* @section note Notes
* Assumes TAC samples are ordered by increasing time index.
*/

#include	"stdafx.h"

char	M1_IFpanelName[]	= "";

char	M1_ModelName[]	= "1. Area Under the Curve (AUC)";

UINT32 M1_Modality	= MCLASS_MSK_ALL;
UINT32 M1_DynDim		= BM(DYNDIM_TIME);
UINT32 M1_ConcConv	= CONCTYPE_MSK_ALL;

UINT32 M1_AllowedOptim	= BM(VA_OPTIM_NONE);			// Allowed optimizations
UINT32 M1_Optim		= VA_OPTIM_NONE;	
int	 M1_OptimGridN	= 0;
int	 M1_OptimNiter	= 0;

const int	M1_NumIfuncs	= 0;
const int	M1_NumFreeParms	= 2;
const int	M1_NumOutParms	= 1;

BOOL	M1_UseNoise		= FALSE;
BOOL	M1_UseGlobalTac	= FALSE;
BOOL	M1_OutFitCurve	= FALSE;
BOOL	M1_ExtrapolateEnable	= FALSE;

double M1_FreeParmDefault[M1_NumFreeParms] = { 0,0 };
double M1_FreeParm[M1_NumFreeParms]	= { 0,0 };

static char FPNAME0[]	= "Start Index";
static char	FPNAME1[]	= "Length (0=all remaining)";
PSTR	M1_FPName[M1_NumFreeParms] = { FPNAME0,FPNAME1 };


static char	OPName0[] = "Curve integral by time";
PSTR	M1_OPName[M1_NumOutParms] = { OPName0 };

static char	OPUnits0[] = "";
PSTR	M1_OPUnits[M1_NumOutParms] = { OPUnits0 };

PR_CLRMAP	M1_ClrScheme[M1_NumOutParms] = { PR_CLRMAP_RAINBOW };



static int	gStart,gEnd;


/**
* @brief Initialize Model 1 (AUC) for the current TAC.
*
* Computes the active [start, end] indices from the free parameters and stores
* them in the module globals.
*
* @param[out] pModelState
*   Opaque per‑call state pointer used by the framework. This model does not
*   maintain state and sets it to @c NULL.
*
* @return bool
*   Always @c true (initialization succeeds unconditionally in this model).
*
* @pre
*   - @c M1_FreeParm[0] ("Start Index") and @c M1_FreeParm[1] ("Length") are set.
*   - @c NumTms and @c AbsTarr are valid for the current TAC.
*
* @post
*   - @c gStart and @c gEnd contain the selected inclusive indices.
*
* @details
*   Index calculation is delegated to @c GetStartEndInx(iround(FP0), iround(FP1), &gStart, &gEnd).
*
* @thread_safety Not thread‑safe (updates globals @c gStart/@c gEnd).
*/

bool	M1_ModelInit( PVOID* pModelState )
{
	*pModelState = NULL;

	GetStartEndInx( iround(M1_FreeParm[0]),iround(M1_FreeParm[1]),&gStart,&gEnd );

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
void	M1_ModelClose( PVOID ModelState )
{
}


/**
* @brief Compute AUC over the selected TAC segment and emit OP[0] if requested.
*
* Converts @p Signal to concentration, slices the TAC to the inclusive window
* [@c gStart, @c gEnd], then integrates that slice with respect to absolute
* time using @c PR_CalculateIntegral().
*
* @param[in]  Signal
*   Pointer to TAC samples (length @c NumTms) in time order.
*   The function calls @c funcSigToConc(Signal, NumTms, Tac, 1, NULL).
*
* @param[out] OutParm
*   Framework-managed writer. When @c ParmReq[0] is nonzero, writes:
*   - OP[0] = "Curve integral by time" (AUC over the selected window).
*
* @return bool
*   @c true on success; @c false if an allocation/guarded call fails (as
*   enforced by the framework's @c xz macro and related checks).
*
* @pre
*   - @c M1_ModelInit() completed successfully.
*   - 0 ≤ @c gStart ≤ @c gEnd < @c NumTms.
*   - @c AbsTarr is monotonic over [@c gStart, @c gEnd].
*
* @post
*   - Temporary TAC buffer is freed before return.
*
* @details
*   Let N = (@c gEnd - @c gStart + 1). Integration is performed as:
*     @code
*     AUC = PR_CalculateIntegral(Tac + gStart, AbsTarr + gStart, N);
*     @endcode
*   Only OP[0] is produced by this model; additional outputs are ignored.
*
* @warning
*   The function assumes valid bounds and a nonempty window (N ≥ 1).
*
* @complexity
*   O(N) time and O(N) temporary memory where N = number of frames in the window.
*/

bool	M1_ModelFunc(
	PDOUBLE	Signal,
	PIVAL		OutParm )
{
PDOUBLE	Tac = NULL;
bool		res = false;

	xz( AllocMem<double >(Tac,NumTms ));
	funcSigToConc( Signal,NumTms,Tac,1,NULL );

int	Lng = gEnd-gStart+1;

double	AUC	= PR_CalculateIntegral( Tac+gStart,AbsTarr+gStart,Lng );

	if ( ParmReq[0] ) Write( OutParm,AUC );

	res	= true;
func_exit:
	pf_free(&Tac);
	return res;
}

