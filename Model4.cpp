/**
* @file Model4.cpp
* @brief Model 4 — Reference curve distance and correlation.
*
* @details
* Converts the input time–activity curve (TAC) to concentration, aligns a
* user-provided reference curve to the current time base, selects a frame
* window via free parameters, then computes:
*   - OP[0] Distance between TAC and reference curve using either L1 or L2 norm
*           integrated over time (piecewise‑linear).
*   - OP[1] Pearson correlation between TAC and reference curve over the window.
*
* Frame indexing in the free parameters is **1‑based and inclusive**; passing
* 0 for either Start or End selects the full [1..NumTms] range. TAC samples are
* assumed to be sorted by **time order** (not dynamic component order). :contentReference[oaicite:1]{index=1}
*
* @section params Free Parameters
*   - FP[0] "L-norm" (int): choose 1 (L1) or 2 (L2). Default = 2.
*   - FP[1] "start index" (int): 1‑based inclusive start frame; 0 → first frame.
*   - FP[2] "end index"   (int): 1‑based inclusive end frame;   0 → last frame.
*
* @section io Inputs / Outputs
*   - Input TAC: @c Signal (double[NumTms]) — converted to concentration via
*                @c funcSigToConc().
*   - Input function(s): @c IFarr[0] — the **reference curve**; prepared and
*                aligned to the time base by @c PR_PrepareInputFunc().
*   - Time base: array from @c PrepareAndCheckTimeArr() used for time‑weighted
*                integrations.
*   - Output: @c OutParm — values written by @c Write() when requested
*                via @c ParmReq[].
*
* @section outputs Outputs and Units
*   - OP[0] Distance:
*       - L1 → ∫ |TAC(t) − Ref(t)| dt  (units: conc × time)
*       - L2 → sqrt( ∫ (TAC(t) − Ref(t))^2 dt )  (units: conc × √time)
*   - OP[1] Correlation (dimensionless, typically in [−1, 1]).
*
* @section deps Dependencies
*   @c PrepareAndCheckTimeArr, @c PR_PrepareInputFunc, @c funcSigToConc,
*   @c PR_IntegrateDiffL1_PWL, @c PR_IntegrateDiffL2_PWL, @c PR_Correlation,
*   @c AllocMem, @c pf_free, @c Write, @c ParmReq, @c xz, @c xmsg.
*
* @section ts Thread-safety
*   Not thread‑safe: uses module‑static globals (@c gLnorm, @c gIfunc, @c gTarr,
*   @c gStr, @c gEnd, @c gLng).
*
* @section mem Memory
*   Allocates a temporary TAC buffer (@c Cnc) during evaluation; the prepared
*   reference curve (@c gIfunc) and time array (@c gTarr) are created at init
*   and freed in @c M4_ModelClose().
*
* @section config Model configuration
*   - @c M4_NumIFuncs = 1 (expects one reference curve)
*   - @c M4_NumFreeParms = 3 ; @c M4_NumOutParms = 2
*   - Allowed optimizations: none (see @c M4_AllowedOptim / @c M4_Optim).
*
* @section license License
*   (Add your project’s license or reference a LICENSE file.)
*/

#include	"stdafx.h"

char	M4_IFpanelName[]	= "Reference curve";

char	M4_ModelName[]	= "4. Reference curve distance and correlation";


UINT32 M4_Modality	= MCLASS_MSK_ALL;
UINT32 M4_DynDim		= BM(DYNDIM_TIME);
UINT32 M4_ConcConv	= CONCTYPE_MSK_ALL;

UINT32 M4_AllowedOptim	= BM(VA_OPTIM_NONE);			// Allowed optimizations
UINT32 M4_Optim		= VA_OPTIM_NONE;	
int	 M4_OptimGridN	= 0;
int	 M4_OptimNiter	= 0;

int	M4_NumIfuncs	= 1;

const int	M4_NumFreeParms	= 3;
const int	M4_NumOutParms	= 2;

BOOL	M4_UseNoise		= FALSE;
BOOL	M4_UseGlobalTac	= FALSE;
BOOL	M4_OutFitCurve	= FALSE;
BOOL	M4_ExtrapolateEnable	= FALSE;


double M4_FreeParm[M4_NumFreeParms]		= { 2,0,0 };
double M4_FreeParmDefault[M4_NumFreeParms]= { 2,0,0 };


static char	FPNAME0[]	= "L-norm";
static char	FPNAME1[]	= "start index";
static char FPNAME2[]	= "end index";
PSTR	M4_FPName[3]	= { FPNAME0,FPNAME1,FPNAME2 };

static char	OPName0[] = "Distance";
static char	OPName1[] = "correlation";
PSTR	M4_OPName[] = { OPName0,OPName1 };

static char	OPUnits0[] = "";
static char	OPUnits1[] = "";
static char	OPUnits2[] = "";
PSTR	M4_OPUnits[] = { OPUnits0,OPUnits1,OPUnits2 };

PR_CLRMAP	M4_ClrScheme[] = { PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW };

static int		gLnorm;
static PDOUBLE	gIfunc	= NULL;
static PDOUBLE	gTarr		= NULL;
static int		gStr		= NULL;
static int		gEnd		= NULL;
static int		gLng		= NULL;

/**
* @brief Initialize Model 4 (reference curve distance & correlation).
*
* Validates inputs, parses the L‑norm selector, prepares the time base and
* the reference curve aligned to that time base, and resolves the active
* frame window.
*
* @param[out] pModelState
*   Opaque per‑call state pointer (unused by this model; remains unchanged).
* @param[in]  IFarr
*   Array of input functions; @c IFarr[0] must be the reference curve with
*   length equal to @c NumTms.
* @param[in]  NumIF
*   Number of input functions provided (unused beyond sanity checks).
*
* @return bool
*   @c true on success; @c false if any guarded allocation or validation fails.
*
* @pre
*   - @c IFarr[0].n == @c NumTms (reference curve matches TAC length).
*   - @c M4_FreeParm[0] in {1,2}; @c M4_FreeParm[1], @c M4_FreeParm[2] are
*     either 0 or valid 1‑based inclusive indices with Start ≤ End.
*
* @post
*   - @c gLnorm ∈ {1,2}.
*   - @c gTarr = PrepareAndCheckTimeArr(...); @c gIfunc = PR_PrepareInputFunc(...).
*   - @c gStr, @c gEnd are 0‑based inclusive indices; @c gLng = gEnd−gStr+1.
*
* @details
*   If either Start or End is 0, the full [1..NumTms] range is selected.
*   Indices are validated in 1‑based space, then converted to 0‑based. :contentReference[oaicite:2]{index=2}
*
* @thread_safety
*   Writes module‑static globals; not thread‑safe.
*/

bool	M4_ModelInit(
	PVOID*	pModelState,
	PINPUTFUNC	IFarr,
	int		NumIF )
{
bool	res	= false;

	if (	IFarr[0].n!=NumTms )		xmsg( msgIncorrectIfunc );

	gLnorm = iround(M4_FreeParm[0]);
	if ( !in_interval( gLnorm,1,2 ))	xmsg( msgSpecifyL1orL2metric );

	// Prepare the matching input function
	xz( gTarr = PrepareAndCheckTimeArr( 3 ));
	xz( gIfunc = PR_PrepareInputFunc( IFarr+0,gTarr,NumTms ));

int	Str = M4_FreeParm[1],
	End = M4_FreeParm[2];
	if ( !Str || !End ) {
		gStr = 1;
		gEnd = NumTms;
	}
	else {
		if (	!in_interval( Str,1,NumTms )	||
			!in_interval( End,1,NumTms )	||
			Str>End )	xmsg( msgInvalidTimeIndex );
	
		gStr = Str;
		gEnd=  End;
	}

	gStr--;
	gEnd--;
	gLng = gEnd-gStr+1;

	res	= true;
func_exit:
	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
void	M4_ModelClose( PVOID ModelState )
{
	pf_free(&gIfunc);
	pf_free(&gTarr);
}


/**
* @brief Compute distance and correlation to the reference curve over the window.
*
* Steps:
*   1) Convert @p Signal (TAC) to concentration via @c funcSigToConc().
*   2) Slice both TAC and reference to [@c gStr, @c gEnd] (length @c gLng).
*   3) Compute distance using the selected L‑norm over time (piecewise‑linear):
*        - L1:  dist = PR_IntegrateDiffL1_PWL(...)
*        - L2:  dist = sqrt( PR_IntegrateDiffL2_PWL(...) )
*   4) Compute Pearson correlation: @c PR_Correlation(refSlice, tacSlice, gLng).
*   5) Emit outputs conditionally:
*        - OP[0] = @c dist      (when @c ParmReq[0])
*        - OP[1] = @c corr      (when @c ParmReq[1])
*
* @param[in]  Signal
*   Pointer to TAC samples (length @c NumTms) in time order.
* @param[out] OutParm
*   Framework-managed writer used by @c Write().
*
* @return bool
*   @c true on success; @c false if a guarded allocation or call fails.
*
* @units
*   - L1 distance: conc × time
*   - L2 distance: conc × √time
*   - Correlation: dimensionless (≈ [−1, 1])
*
* @pre
*   - @c M4_ModelInit() completed successfully.
*   - @c gIfunc and @c gTarr are prepared; @c gLng ≥ 1.
*
* @post
*   - Temporary buffer @c Cnc is freed before return.
*
* @warning
*   Frame indices in the UI are 1‑based; internal arrays are 0‑based. TAC is
*   assumed to be in **time order**, not dynamic component order. :contentReference[oaicite:4]{index=4}
*
* @complexity
*   O(N) time and O(N) temporary memory, where N = @c gLng.
*/

bool	M4_ModelFunc(
	PDOUBLE	Signal,
	PIVAL		OutParm )
{
PDOUBLE	Cnc	= NULL;
bool		res	= false;

PR_CONCCONVBASE ConvBase;
	xz( AllocMem<double >(Cnc,NumTms ));
	funcSigToConc( Signal,NumTms,Cnc,1,&ConvBase );


double dist;
	if ( gLnorm==2 ) {
		dist	= sqrt(PR_IntegrateDiffL2_PWL( Cnc+gStr,gIfunc+gStr,gTarr+gStr,gLng ));
	}
	else {
		dist	= PR_IntegrateDiffL1_PWL( Cnc+gStr,gIfunc+gStr,gTarr+gStr,gLng );
	}


double corr = PR_Correlation( gIfunc+gStr,Cnc+gStr,gLng );

	if ( ParmReq[0] )	Write( OutParm,dist );
	if ( ParmReq[1] )	Write( OutParm,corr );

	res	= true;
func_exit:
	pf_free(&Cnc);
	return res;
}
