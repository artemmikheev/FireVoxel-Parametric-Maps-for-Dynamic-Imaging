/**
* @file Model6.cpp
* @brief Model 6 — Cerebral Blood Volume (CBV) baseline integral.
*
* @details
* Computes a CBV-like baseline integral from an MR time–activity curve (TAC).
* The model:
*   1) Builds a relative time array from absolute frame times.
*   2) Uses the global TAC to determine pre/post baseline window lengths.
*   3) For each voxel, checks an "air" condition on the raw TAC (not concentration),
*      optionally skips an initial number of frames, and estimates pre/post baselines.
*   4) Baseline‑corrects the TAC with a linear trend between the discovered bolus
*      start/end times.
*   5) Converts signal to a concentration‑like curve via @f$\Delta R(t)=-\ln(S(t)/S_0)@f$
*      with @f$S_0=@f$ pre‑bolus baseline, clamped for numerical safety.
*   6) Integrates @f$\Delta R(t)@f$ over the bolus window and (optionally) normalizes
*      by a white‑matter ROI integral.
*
* Output:
*   - OP[0] “CBV baseline integral” — the time integral of @f$\Delta R(t)@f$,
*     scaled by a white‑matter normalization factor if available.
*
* @note
*   - The “air” threshold is applied to **raw TACs** (not concentrations), which is
*     why the header warns that an air threshold is only appropriate for TAC‑based
*     models. TAC samples are assumed to be ordered by acquisition time. :contentReference[oaicite:1]{index=1}
*
* @section params Free Parameters
*   - FP[0] **Background Threshold** (double): multiplier for @c demp_NoiseLevel
*     to set @c AirThresh used by @c IsAir_ByMin().
*   - FP[1] **Skip Initial Time Points** (int): number of leading frames to skip.
*
* @section io Inputs/Outputs
*   - Input TAC: raw @c Tac (double[NumTms]) — **not** converted to concentration
*     prior to the air check; concentration‑like values are derived internally.
*   - Time base: @c Tarr = PR_MakeRelativeArr(AbsTarr, NumTms) (same units as
*     @c AbsTarr, typically seconds).
*   - Output: @c OutParm — written with OP[0] via @c Write().
*
* @section deps Dependencies
*   Uses (non‑exhaustive): @c PR_MakeRelativeArr, @c PR_ArrStats, @c CalculateIntegral,
*   @c IsAir_ByMin, @c Write, @c pf_free, globals @c GlobalTac, @c demp_NoiseLevel,
*   @c RoiTacArr, @c NumRoiTac.
*
* @section units Units
*   Before WM normalization, the integral has units of **time** (the units of
*   @c Tarr). If normalization by a white‑matter ROI is applied, OP[0] becomes
*   a **dimensionless ratio** (relative CBV).
*
* @section config Model configuration
*   - @c M6_NumIfuncs = 0 ; @c M6_NumFreeParms = 2 ; @c M6_NumOutParms = 1
*   - @c M6_UseNoise = TRUE ; @c M6_UseGlobalTac = TRUE ; optimizations disabled.
*
* @section ts Thread‑safety
*   Not thread‑safe: uses module‑static globals (@c Tarr, @c AirThresh, @c SkipTimes,
*   @c pre_N/@c post_N, @c WhiteMatterNorm).
*
* @section mem Memory
*   Allocates a relative time array at init and frees it at close. Per‑voxel work
*   uses stack buffers sized @c DEF_MAXNUMTMS.
*
* @section impl Implementation notes
*   - The code intends to compute @c WhiteMatterNorm from a white‑matter ROI
*     integral, but the call that would populate the local @c Integral is
*     commented out; ensure this normalization is computed before use. :contentReference[oaicite:2]{index=2}
*
* @section license License
*   (Add your project’s license notice or reference a LICENSE file.)
*/

#include	"stdafx.h"

char	M6_IFpanelName[]	= "";

char	M6_ModelName[]	= ""; //6.  Cerebral Blood Volume";

const int	M6_NumFreeParms	= 2;
const int	M6_NumOutParms	= 1;

int	M6_NumIfuncs	= 0;


BOOL	M6_UseNoise		= TRUE;
BOOL	M6_UseGlobalTac	= TRUE;
BOOL	M6_OutFitCurve	= FALSE;

UINT32 M6_Modality	= BM(MCLASS_MR);
UINT32 M6_DynDim		= BM(DYNDIM_TIME);
UINT32 M6_ConcConv	= CONCTYPE_MSK_ALL;

UINT32 M6_AllowedOptim	= BM(VA_OPTIM_NONE);			// Allowed optimizations
UINT32 M6_Optim		= VA_OPTIM_NONE;	
int	 M6_OptimGridN	= 0;
int	 M6_OptimNiter	= 0;

double M6_FreeParm[M6_NumFreeParms]	= { 20,0 };
double M6_FreeParmDefault[M6_NumFreeParms] = { 20,0 };

static char	FPNAME0[]	= "Background Threshold";
static char	FPNAME1[]	= "Skip Initial Time Points";
PSTR	M6_FPName[M6_NumFreeParms] = { FPNAME0,FPNAME1 };

static char	OPName0[] = "CBV baseline integral";
PSTR	M6_OPName[M6_NumOutParms] = { OPName0 };

static char	OPUnits0[]	= "";
PSTR	M6_OPUnits[M6_NumOutParms] = { OPUnits0 };

PR_CLRMAP	M6_ClrScheme[M6_NumOutParms] = { PR_CLRMAP_RAINBOW };


static double	AirThresh;
static double	WhiteMatterNorm;
static int		SkipTimes;
static int		wNumTms;

const double	MAX_BASELINE_DEV = 0.05;
const double	MAX_BASELINE_SPLIT = 0.2;


enum {
	PASS_START		= 2,
	PRE_MAXBASELINE	= 20,
	BOLUS_BEFORESTART	= 4
};



static int	pre_N,
		post_N;

const double	PRE_N_THR	= 0.95,
			POST_N_THR  = 0.95;

static PDOUBLE	Tarr	= NULL;

/**
* @brief Initialize Model 6 (CBV baseline integral).
*
* Builds a relative time array, derives pre/post baseline window sizes from the
* global TAC, and prepares thresholds and normalization.
*
* @param[out] pModelState
*   Opaque per‑call state pointer (unused by this model; left unchanged).
*
* @return bool
*   @c true on success; @c false if a guarded allocation/validation fails.
*
* @pre
*   - @c AbsTarr/@c NumTms are valid; @c GlobalTac is available.
*   - @c M6_FreeParm[0] (background threshold multiplier) and
*     @c M6_FreeParm[1] (skip count) are set.
*   - At most one white‑matter ROI is provided (@c NumRoiTac <= 1).
*
* @post
*   - @c Tarr = PR_MakeRelativeArr(AbsTarr, NumTms).
*   - @c AirThresh = M6_FreeParm[0] * demp_NoiseLevel.
*   - @c SkipTimes set; working length @c wNumTms = NumTms - SkipTimes.
*   - @c pre_N/@c post_N derived from @c GlobalTac (see code for details).
*   - @c WhiteMatterNorm initialized (intended as 1 / WM integral; see note).
*
* @details
*   Baseline windows are derived using thresholds @c PRE_N_THR and @c POST_N_THR
*   relative to the minimum of the (shifted) global TAC. If a single WM ROI is
*   present, its TAC is checked (@c IsAir_ByMin) and intended to define a
*   normalization factor @c WhiteMatterNorm = 1 / Integral(ROI). The current
*   implementation leaves the integral uninitialized (commented code). :contentReference[oaicite:3]{index=3}
*
* @thread_safety Not thread‑safe: writes module‑static state.
*/

bool	M6_ModelInit( PVOID* pModelState )
{	
bool	res	= false;
	if ( NumRoiTac>1 ) xmsg( "This Model requires no more than one White Matter ROI" );

	xz( Tarr = PR_MakeRelativeArr( AbsTarr,NumTms ));

	AirThresh = M6_FreeParm[0]*demp_NoiseLevel;
	SkipTimes = (int)(M6_FreeParm[1]);

	// Define working number of timepoints	
	wNumTms = NumTms-SkipTimes;	
	
	//............................................................................
	// Define pre_N\post_N values
	
PDOUBLE	wTac = GlobalTac+PASS_START;
double	SA = wTac[0],
		SB = wTac[wNumTms-1];

	// now find the Minimum
double MinSi = SA;
	for ( int t=1; t<wNumTms; t++ ) 
		MinSi = min( MinSi,wTac[t] );


	// Find pre_N
double Thr = (SA-MinSi)*PRE_N_THR;
	for ( pre_N=1; pre_N<wNumTms; pre_N++ ) 
		if ( wTac[pre_N]-MinSi<Thr ) break;

	// Find post_N
PDOUBLE wTacEnd = wTac+wNumTms-1;
	Thr = (SB-MinSi)*POST_N_THR;
	for ( post_N=1; post_N<wNumTms; post_N++ )
		if ( wTacEnd[-post_N]-MinSi<Thr ) break;

	//...............................................................................
	// Define the White Matter norm
	//
	if ( NumRoiTac==1 ) {
		const PDOUBLE RoiTac = RoiTacArr[0];

		double Tac[DEF_MAXNUMTMS];
		for ( int t=0; t<NumTms; t++ ) Tac[t] = fabs(RoiTac[t]);
	
		if ( IsAir_ByMin( Tac,NumTms )) xmsg("White Matter ROI TAC is incorrect"); 

		// Initialize White Matter Norm
		WhiteMatterNorm = ONE;
		double	Integral;
		PDOUBLE p = &Integral;
		//M6_ModelFunc( Tac,p );
		WhiteMatterNorm = ONE/Integral;
	}
	else	WhiteMatterNorm = ONE;


	res	= true;
func_exit:
	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
void	M6_ModelClose( PVOID ModelState )
{
	pf_free(&Tarr);
}


/**
* @brief Legacy helper for curve fitting: returns @f$f(x)=a_1\,x\,e^{-a_2 x}@f$
*        and (optionally) its partial derivatives.
*
* @param[in]  x     Evaluation point.
* @param[in]  a     Parameter array where @c a[1]=a1 and @c a[2]=a2
*                   (note: 0‑based array with legacy 1‑based indexing).
* @param[out] dyda  If non‑null, stores partials: @c dyda[1]=∂f/∂a1 = x e^{-a2 x},
*                   @c dyda[2]=∂f/∂a2 = -a1 a2 x e^{-a2 x}.
*
* @return double  Function value @f$a_1 x e^{-a_2 x}@f$.
*
* @note The comment above this function mentions a different formula involving TR;
*       the implementation here follows @f$a_1 x e^{-a_2 x}@f$ and does not use TR. :contentReference[oaicite:4]{index=4}
*/

double	GammaFunc(
		double	x,
		double*	a,
		double*	dyda )
{
double e = exp( -a[2]*x );

	if ( dyda ) {
		dyda[1] = x*e;
		dyda[2] = -a[1]*a[2]*x*e;
	}

	return a[1]*x*e;
}



static bool	ArrayIsGreater(
		PDOUBLE	Arr,
		int		Num,
		double	Val )
{
	for ( int i=0; i<Num; i++ )
		if ( Arr[i]<=Val ) return false;


	return true;
}




/**
* @brief Locate bolus start/end indices within a (potentially trimmed) TAC.
*
* The minimum of @p wTac is treated as the bolus “peak” (signal drop in DSC).
* The function then searches:
*   - **Start**: backward from the peak to @c pre_N until @f$wTac[t-1] > pre\_bl - noise@f$.
*   - **End**: forward from the peak+2 to @c Last = wNumTms - post_N until either
*     @f$wTac[t] > post\_bl - noise@f$ or a new downward excursion exceeds @f$noise@f$.
* The end index is clamped to @c min(found-1, Last-1).
*
* @param[in]  wTac      Working TAC (after initial skips), length @p wNumTms.
* @param[in]  wNumTms   Number of working time points.
* @param[in]  noise     Noise estimate (e.g., stdev from pre‑baseline).
* @param[in]  pre_bl    Mean of the pre‑baseline window.
* @param[in]  post_bl   Mean of the post‑baseline window.
* @param[out] pBolusStart Receives start index (0‑based, @c >= pre_N).
* @param[out] pBolusEnd   Receives end index (0‑based, @c < wNumTms - post_N).
*
* @complexity O(wNumTms).
*
* @pre @c pre_N and @c post_N must be initialized in @c M6_ModelInit().
*
* @note Indices are with respect to @p wTac (i.e., after skipping initial frames). :contentReference[oaicite:6]{index=6}
*/

static void	FindBolusPosition(
		PDOUBLE	wTac,
		int		wNumTms,
		double	noise,
		double	pre_bl,
		double	post_bl,
		int*		pBolusStart,
		int*		pBolusEnd )
{
	// Find position and value of bolus peak
double msd		= wTac[0];
int	 b_peak	= 0;
	for ( int t=1; t<wNumTms; t++ )
		if ( wTac[t]<msd ) {
			msd = wTac[t];
			b_peak = t;
		}


	// Find start of bolus
double cutoff = pre_bl - noise;
int	 b_start;
	for ( b_start=b_peak; b_start>pre_N; b_start-- ) 
		if ( wTac[b_start-1]>cutoff ) break;


	// Find end of bolus
	cutoff = post_bl - noise;		// Baseline
int	b_end;
double mx	= msd;
int	 Last = wNumTms-post_N;

	for ( b_end = b_peak+2; b_end<Last; b_end++ ) {
		if ( wTac[b_end]>mx )	mx = wTac[b_end];

		if (	(wTac[b_end]>cutoff)	||
			(wTac[b_end]<(mx-noise))) break;
	}
	b_end = min( b_end-1,Last-1 );


	*pBolusStart= b_start;
	*pBolusEnd	= b_end;
}


/**
* @brief Compute the CBV baseline integral for a single TAC and emit OP[0].
*
* Steps:
*   1) Reject voxels classified as “air” by @c IsAir_ByMin(Tac, AirThresh).
*   2) Trim the TAC/time arrays by @c SkipTimes.
*   3) Estimate pre/post baselines and noise using @c PR_ArrStats().
*   4) Find bolus start/end via @c FindBolusPosition().
*   5) Baseline‑correct the TAC with a linear trend between start/end.
*   6) Convert to @f$\Delta R(t)=-\ln(S(t)/S_0)@f$ (with clamping).
*   7) Integrate @f$\Delta R(t)@f$ over [start, end] using @c CalculateIntegral().
*   8) Write @c Intg * WhiteMatterNorm to @p OutParm (OP[0]).
*
* @param[in]  Tac     Pointer to raw TAC samples (length @c NumTms) in time order.
* @param[out] OutParm Framework‑managed writer used by @c Write().
*
* @return bool
*   @c true on success; @c false if a guarded check fails (e.g., air voxel,
*   invalid bolus window).
*
* @pre
*   - @c M6_ModelInit() completed successfully; @c Tarr, @c pre_N/@c post_N,
*     and thresholds are set.
*   - @c NumTms > @c SkipTimes and TAC is time‑sorted.
*
* @post
*   - OP[0] is emitted. (In this implementation, it is written unconditionally
*     rather than being gated by @c ParmReq[0].)
*
* @units
*   Integral units match the time units of @c Tarr (e.g., seconds). After
*   white‑matter normalization, the result is dimensionless.
*
* @warning
*   Frame indexing is 0‑based internally; any UI using 1‑based indices must
*   be reconciled externally. This model operates on raw TACs for the air
*   check; concentration conversion is handled internally afterward. :contentReference[oaicite:7]{index=7}
*
* @complexity
*   O(N) time and O(N) temporary memory, where N = @c NumTms.
*/

bool	M6_ModelFunc(
	PDOUBLE	Tac,
	PIVAL		OutParm )
{
bool	res	= false;

	// Set values for void voxels
	xnz( IsAir_ByMin( Tac,AirThresh ));


PDOUBLE	wTac		= Tac+SkipTimes;
int		wNumTms	= NumTms-SkipTimes;
PDOUBLE	wTarr		= Tarr+SkipTimes;


	//......................................................
	// Find the point of minimal signal
double noise;
double pre_bl = PR_ArrStats( wTac,pre_N,&noise ),
	 post_bl= PR_ArrStats( wTac+(wNumTms-post_N),post_N,NULL );
	
	// Find position of the Bolus
int	b_start,b_end;
	FindBolusPosition( wTac,wNumTms,noise,pre_bl,post_bl,&b_start,&b_end );
	xnz( b_start>=b_end );

	// Perform baseline correction
double b_stime = wTarr[b_start];
double sf = (post_bl-pre_bl)/(wTarr[b_end]-b_stime);

double CorrTac[DEF_MAXNUMTMS];
	for ( int t=b_start; t<=b_end; t++ ) 
		CorrTac[t] = wTac[t] - sf*(wTarr[t]-b_stime);


	// Find tracer concentration
const double S0 = pre_bl;

double Cx[DEF_MAXNUMTMS];
	for ( int t=0; t<wNumTms; t++ ) {
		double tmp = CorrTac[t]/S0;
		if ( tmp>0.01 && tmp<ONE )	Cx[t] = -log(tmp);
		else					Cx[t] = ZERO;
	}
 
	//----------------------------------------------------------------
	// R2 integral with BaseLine
double Intg = CalculateIntegral( Cx+b_start,wTarr+b_start,b_end-b_start+1 );

	Write( OutParm,Intg*WhiteMatterNorm );

	res	= true;
func_exit:
	return res;
}











