/**
* @file Model5.cpp
* @brief Model 5 — Time of active rise (TAR) and rise slope.
*
* @details
* Implements "5. Time of active rise". The model converts the input
* time–activity curve (TAC) to concentration, constructs a relative time
* array from absolute frame times, and measures the time it takes the TAC to
* rise from a **low** to a **high** fraction of its peak value (both
* user‑specified). It also reports the average slope across that interval:
*   - OP[0] Active rise time (TAR) in seconds
*   - OP[1] Slope across TAR
*
* Thresholds are interpreted as fractions of the peak amplitude:
*   ThrA = ThrKoffA × max(TAC), ThrB = ThrKoffB × max(TAC), with ThrA < ThrB
* recommended. Crossings are searched only on the **rising portion** up to
* and including the global maximum sample. TAC samples are assumed to be in
* **time order**. :contentReference[oaicite:1]{index=1}
*
* @section params Free Parameters
*   - FP[0] "Active Rise Low Threshold"  (double, default 0.20)
*   - FP[1] "Active Rise High Threshold" (double, default 0.95)
*     (Both are fractions of the TAC peak; typical choice: 0 < FP0 < FP1 < 1.)
*
* @section io Inputs / Outputs
*   - Input TAC: @c Signal (double[NumTms]) — converted to concentration via
*                @c funcSigToConc().
*   - Time base: relative time array from
*                @c PR_MakeRelativeArr(AbsTarr, NumTms) (seconds).
*   - Output: @c OutParm — values written by @c Write() when requested via
*                @c ParmReq[] (OP[0]=TAR, OP[1]=Slope). :contentReference[oaicite:2]{index=2}
*
* @section outputs Outputs and Units
*   - OP[0] Active rise time TAR = t(ThrB) − t(ThrA)  [seconds]
*   - OP[1] Slope = (ThrB − ThrA) / TAR               [units of conc / second]
*     (OP units are configured in @c M5_OPUnits: "sec" for OP[0], empty for OP[1].)
*     :contentReference[oaicite:3]{index=3}
*
* @section deps Dependencies
*   @c PR_MakeRelativeArr, @c funcSigToConc, @c FindMaxVal, @c FindThresholdTime,
*   @c AllocMem, @c pf_free, @c Write, @c ParmReq, @c xz, @c VOIDVOX. :contentReference[oaicite:4]{index=4}
*
* @section config Model configuration
*   - @c M5_NumIfuncs = 0 ; @c M5_NumFreeParms = 2 ; @c M5_NumOutParms = 2
*   - Allowed optimizations: @c VA_OPTIM_NONE. :contentReference[oaicite:5]{index=5}
*
* @section ts Thread‑safety
*   Not thread‑safe: uses module‑static state (@c RISE_THRA, @c RISE_THRB,
*   @c gTarr). :contentReference[oaicite:6]{index=6}
*
* @section mem Memory
*   Creates a relative time array at init and frees it at close; allocates a
*   transient TAC buffer during evaluation. :contentReference[oaicite:7]{index=7}
*
* @section license License
*   (Add your project’s license or reference a LICENSE file.)
*/

#include	"stdafx.h"

char	M5_IFpanelName[]	= "";

char	M5_ModelName[]	= "5. Time of active rise";
int	M5_NumIfuncs	= 0;

UINT32 M5_Modality	= MCLASS_MSK_ALL;
UINT32 M5_DynDim		= BM(DYNDIM_TIME);
UINT32 M5_ConcConv	= CONCTYPE_MSK_ALL;

UINT32 M5_AllowedOptim	= BM(VA_OPTIM_NONE);			// Allowed optimizations
UINT32 M5_Optim		= VA_OPTIM_NONE;	
int	 M5_OptimGridN	= 0;
int	 M5_OptimNiter	= 0;


const int	M5_NumFreeParms		= 2;
const int	M5_NumOutParms		= 2;

BOOL	M5_UseNoise			= FALSE;
BOOL	M5_UseGlobalTac		= FALSE;
BOOL	M5_OutFitCurve		= FALSE;
BOOL	M5_ExtrapolateEnable	= FALSE;


double M5_FreeParm[M5_NumFreeParms]			= { 0.2,0.95 };
double M5_FreeParmDefault[M5_NumFreeParms]	= { 0.2,0.95 };


static char	FPNAME0[]	= "Active Rise Low Threshold";
static char	FPNAME1[]	= "Active Rise High Threshold";
PSTR	M5_FPName[M5_NumFreeParms] = { FPNAME0,FPNAME1 };

static char	OPName0[]	= "Active rise time";
static char OPName1[]	= "Slope";

PSTR	M5_OPName[] = { OPName0,OPName1 };

static char	OPUnits0[] = "sec";
static char	OPUnits1[] = "";
PSTR	M5_OPUnits[] = { OPUnits0,OPUnits1 };

PR_CLRMAP	M5_ClrScheme[] = { PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW };


static double	RISE_THRA,
			RISE_THRB;

static PDOUBLE	gTarr	= NULL;

/**
* @brief Initialize Model 5 (Time of active rise).
*
* Loads the low/high threshold fractions from the model free parameters and
* builds a relative time array from @c AbsTarr for subsequent timing
* operations.
*
* @param[out] pModelState
*   Opaque per‑call state pointer used by the framework. This model does not
*   maintain state and leaves it unchanged (typically @c NULL).
*
* @return bool
*   @c true on success; @c false if a guarded allocation fails.
*
* @pre
*   - @c M5_FreeParm[0] (low fraction) and @c M5_FreeParm[1] (high fraction)
*     are set.
*   - @c AbsTarr and @c NumTms are valid for the current TAC.
*
* @post
*   - @c RISE_THRA and @c RISE_THRB reflect the configured fractions.
*   - @c gTarr points to a newly created relative time array (seconds) created
*     by @c PR_MakeRelativeArr(); freed in @c M5_ModelClose().
*
* @thread_safety Not thread‑safe (writes module‑static @c gTarr and thresholds).
*
* @see PR_MakeRelativeArr(). :contentReference[oaicite:8]{index=8}
*/

bool	M5_ModelInit( PVOID* pModelState )
{
bool	res	= false;

	RISE_THRA	= M5_FreeParm[0];
	RISE_THRB	= M5_FreeParm[1];

	xz( gTarr = PR_MakeRelativeArr( AbsTarr,NumTms ));

	res	= true;
func_exit:
	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
void	M5_ModelClose( PVOID ModelState )
{
	pf_free(&gTarr);
}


/**
* @brief Compute time of active rise (TAR) and average slope between two
*        threshold fractions of the peak, restricted to the rising phase.
*
* @param[in]  Y         TAC values (length @p N) in concentration units.
* @param[in]  X         Corresponding time values (length @p N), typically a
*                       relative time array in seconds.
* @param[in]  N         Number of samples.
* @param[in]  ThrKoffA  Low threshold fraction of the peak (e.g., 0.2).
* @param[in]  ThrKoffB  High threshold fraction of the peak (e.g., 0.95).
* @param[out] pTAR      Resulting time interval t(ThrB) − t(ThrA) (seconds).
* @param[out] pSlope    Average slope ( (ThrB − ThrA) / TAR ).
*
* @return bool
*   @c true on success; @c false if either threshold crossing is undefined or
*   degenerate (e.g., equal times), in which case @p pTAR and @p pSlope are set
*   to @c VOIDVOX.
*
* @details
*   1) Find the global maximum value and its index @c Tmax over @p Y.
*   2) Define thresholds @c ThrA = max(Y)×ThrKoffA and @c ThrB = max(Y)×ThrKoffB.
*   3) Search only the rising portion @c Y[0..Tmax] for the times when @p Y
*      crosses @c ThrA and @c ThrB (via @c FindThresholdTime with rising=true).
*   4) Compute @c *pTAR = tb − ta and @c *pSlope = (ThrB − ThrA) / *pTAR. :contentReference[oaicite:10]{index=10}
*
* @pre
*   - Typically 0 < @p ThrKoffA < @p ThrKoffB < 1 (not enforced by code).
*   - @p X is monotonic non‑decreasing over the interval.
*
* @complexity O(N) time.
*/

static bool	CalcTAR(
		PDOUBLE	Y,
		PDOUBLE	X,
		int		N,
		double	ThrKoffA,
		double	ThrKoffB,
		PDOUBLE	pTAR,
		PDOUBLE	pSlope )
{
bool	res	= false;


INT64	Tmax;
const double MaxY = FindMaxVal( Y,N,&Tmax ),
		 ThrA = MaxY*ThrKoffA,
		 ThrB = MaxY*ThrKoffB;

const int	 RiseN = Tmax+1;

double	ta = FindThresholdTime( Y,RiseN,ThrA,true,X ),
		tb = FindThresholdTime( Y,RiseN,ThrB,true,X );


	if ( ta==VOIDVOX	|| tb==VOIDVOX ||	IsEqual(ta,tb))	goto func_exit;

	*pTAR = tb-ta;
	*pSlope = (ThrB-ThrA)/(*pTAR);


	res	= true;
func_exit:
	if ( !res ) *pTAR = *pSlope = VOIDVOX;
	return res;
}


/**
* @brief Convert TAC to concentration, compute TAR & slope, and emit outputs.
*
* Steps:
*   1) Allocate a working buffer and convert @p Signal to concentration via
*      @c funcSigToConc() (storing conversion base in @c PR_CONCCONVBASE).
*   2) Call @c CalcTAR( Cnc, gTarr, NumTms, RISE_THRA, RISE_THRB, &TAR, &Slope ).
*   3) Conditionally write outputs (guarded by @c ParmReq[]):
*        - OP[0] = TAR (seconds)
*        - OP[1] = Slope
*
* @param[in]  Signal  Pointer to TAC samples (length @c NumTms) in time order.
* @param[out] OutParm Framework‑managed writer used by @c Write().
*
* @return bool
*   @c true on success; @c false if an allocation or a guarded call fails. All
*   transient allocations are released before return. :contentReference[oaicite:11]{index=11}
*
* @pre
*   - @c M5_ModelInit() completed successfully (valid @c gTarr and thresholds).
*   - @c NumTms > 0.
*
* @post
*   - Temporary buffer is freed before return.
*
* @warning
*   The model inspects only the **rising phase** up to the global maximum.
*   If thresholds are not crossed within that region (or are equal), outputs
*   are set to @c VOIDVOX. TAC must be in **time order**. :contentReference[oaicite:12]{index=12}
*
* @complexity
*   O(N) time and O(N) temporary memory, where N = @c NumTms.
*/

bool	M5_ModelFunc(
	PDOUBLE	Signal,
	PIVAL		OutParm )
{
PDOUBLE	Cnc	= NULL;
bool		res	= false;

PR_CONCCONVBASE ConvBase;
	xz( AllocMem<double >(Cnc,NumTms ));
	funcSigToConc( Signal,NumTms,Cnc,1,&ConvBase );


double TAR,Slope;
	xz( CalcTAR( Cnc,gTarr,NumTms,RISE_THRA,RISE_THRB,&TAR,&Slope ));


	if ( ParmReq[0])	Write( OutParm,TAR );
	if ( ParmReq[1])	Write( OutParm,Slope );

	res	= true;
func_exit:
	pf_free(&Cnc);
	return res;
}

