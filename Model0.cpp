/**
* @file Model0.cpp
* @brief Model 0 — Basic measurements over a TAC segment.
*
* @details
* Implements "0. Basic measurements", which computes summary statistics over a
* time-activity curve (TAC) segment selected by free parameters. The module
* converts the input signal to concentration, extracts the requested
* time-window, and reports:
*   - OP[0] Max value
*   - OP[1] Value spread (= max - min)
*   - OP[2] Median value
*   - OP[3] Mean value
*   - OP[4] Value standard deviation
*   - OP[5] Coefficient of variation (= std/mean)
*   - OP[6] Skewness
*   - OP[7] Kurtosis
*
* @note TAC is sorted in **time order**, not dynamic-component order. This
*       convention is used throughout the DEMP framework (see in-file note).
*
* @section params Free Parameters
*   - FP[0] "Start Index" (int): zero-based start index into the TAC.
*   - FP[1] "Length (0=all remaining)" (int): number of frames; 0 means
*           from Start Index to the end.
*
* @section io Inputs/Outputs
*   - Input: @c Signal (double[NumTms]) — TAC samples in time order. Units
*            depend on @c funcSigToConc() conversion.
*   - Output: @c OutParm — framework-managed writer used by @c Write() to
*            emit only the requested outputs according to @c ParmReq[].
*
* @section deps Dependencies
* Relies on framework utilities/macros and globals (non-exhaustive):
*   GetStartEndInx, PR_MakeRelativeArr, funcSigToConc, PR_GetArrMinMax,
*   VA_CreateVol, VA_VolCalcRoiInfo, FindMinVal, FindMaxVal, Write,
*   PR_FrameDelete, AllocMem, pf_free, xz, NumTms, AbsTarr, ParmReq.
*
* @section ts Thread-safety
* Not thread-safe: uses statics/globals (@c gStart, @c gEnd, @c Tarr).
*
* @section mem Memory
* Allocates a transient TAC buffer and an internal volume for ROI statistics;
* both are released before return. A relative time array (@c Tarr) is created
* at init and freed in @c M0_ModelClose().
*
*
*
*/

#include	"stdafx.h"


char	M0_IFpanelName[]	= "";

char	M0_ModelName[]	= "0. Basic measurements";
UINT32 M0_Modality	= MCLASS_MSK_ALL;
UINT32 M0_DynDim		= DYNDIM_MSK_ALL;
UINT32 M0_ConcConv	= /*BM(CONCTYPE_NOCONV);*/	CONCTYPE_MSK_ALL;

UINT32 M0_AllowedOptim	= BM(VA_OPTIM_NONE);			// Allowed optimizations
UINT32 M0_Optim		= VA_OPTIM_NONE;		
int	 M0_OptimGridN	= 0;
int	 M0_OptimNiter	= 0;

const int	M0_NumIfuncs	= 0;
const int	M0_NumFreeParms	= 2;
const int	M0_NumOutParms	= 8;

BOOL	M0_UseNoise		= FALSE;
BOOL	M0_UseGlobalTac	= FALSE;
BOOL	M0_OutFitCurve	= FALSE;
BOOL	M0_ExtrapolateEnable	= FALSE;


double M0_FreeParmDefault[M0_NumFreeParms] = { 0,0 };
double M0_FreeParm[M0_NumFreeParms]	= { 0,0 };



static char FPNAME0[]	= "Start Index";
static char	FPNAME1[]	= "Length (0=all remaining)";
PSTR	M0_FPName[M0_NumFreeParms] = { FPNAME0,FPNAME1 };


static char	OPName0[] = "Max value";
static char	OPName1[] = "Value spread";
static char OPName2[] = "Median value";
static char	OPName3[] = "Mean value";
static char	OPName4[] = "Value StdDev";
static char OPName5[] = "CoeffOfVariation";
static char OPName6[] = "Skewness";
static char OPName7[] = "Kurtosis";



PSTR	M0_OPName[M0_NumOutParms] = { OPName0,OPName1,OPName2,OPName3,OPName4,OPName5,OPName6,OPName7 };

static char	OPUnits0[] = "";
static char	OPUnits1[] = "";
static char	OPUnits2[] = "";
static char	OPUnits3[] = "";
static char	OPUnits4[] = "";
static char OPUnits5[] = "";
static char OPUnits6[] = "";
static char OPUnits7[] = "";

PSTR	M0_OPUnits[M0_NumOutParms] = { OPUnits0,OPUnits1,OPUnits2,OPUnits3,OPUnits4,OPUnits5,OPUnits6,OPUnits7 };


PR_CLRMAP	M0_ClrScheme[M0_NumOutParms] = { PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW  };


static int		gStart,
			gEnd;


static PDOUBLE	Tarr = NULL;

/**
* @brief Initialize Model 0 ("Basic measurements") for the current TAC.
*
* Computes the effective [start, end] indices from the free parameters and
* builds a relative time array used by downstream code.
*
* @param[out] pModelState
*   Opaque per-voxel/per-call state pointer used by the framework.
*   This model does not maintain a state object and sets it to @c NULL.
*
* @return bool
*   @c true on success; @c false on failure.
*
* @pre
*   - @c M0_FreeParm[0] ("Start Index") and @c M0_FreeParm[1] ("Length") are set.
*   - Globals @c NumTms and @c AbsTarr are valid.
*
* @post
*   - @c gStart and @c gEnd hold the active segment (0-based, inclusive/exclusive inferred by usage).
*   - @c Tarr points to a newly created relative time array; freed in @c M0_ModelClose().
*
* @note
*   The current implementation returns @c true unconditionally at the end;
*   if you want error propagation from @c PR_MakeRelativeArr() via @c xz,
*   consider returning the local @c res instead.
*
* @thread_safety Not thread-safe (writes globals @c gStart, @c gEnd, @c Tarr).
*/

bool	M0_ModelInit( PVOID* pModelState )
{
bool	res	= false;

	GetStartEndInx( iround(M0_FreeParm[0]),iround(M0_FreeParm[1]),&gStart,&gEnd );

	xz( Tarr = PR_MakeRelativeArr( AbsTarr,NumTms ));

	*pModelState = NULL;

	res	= true;
func_exit:
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
void	M0_ModelClose( PVOID ModelState )
{
	pf_free(&Tarr);
}


/**
* @brief Compute summary statistics over the selected TAC segment.
*
* Converts the input @p Signal to concentration, selects the [start, end]
* window defined by @c gStart/@c gEnd (or the full TAC if both are zero),
* and computes:
*   - max, min (for spread), median, mean, std dev, coefficient of variation,
*     skewness, and kurtosis.
* Only the outputs requested via @c ParmReq[] are written in order to @p OutParm.
*
* @param[in]  Signal
*   Pointer to TAC samples (length @c NumTms) in time order. The function
*   calls @c funcSigToConc(Signal, NumTms, Tac, 1, NULL) before analysis.
*
* @param[out] OutParm
*   Framework-managed output writer. Values are appended in the fixed order:
*   OP[0]=Max value, OP[1]=Value spread, OP[2]=Median, OP[3]=Mean,
*   OP[4]=StdDev, OP[5]=CoeffOfVariation, OP[6]=Skewness, OP[7]=Kurtosis.
*
* @return bool
*   @c true on success; @c false if a framework allocation/guard fails.
*
* @pre
*   - @c M0_ModelInit() was called and completed successfully.
*   - @c Signal has @c NumTms elements.
*
* @post
*   - Transient allocations (TAC buffer, ROI volume) are released before return.
*
* @details
*   After conversion to concentration, the function slices the TAC to
*   [Start, End], then:
*     1) Finds min/max for the slice via @c PR_GetArrMinMax().
*     2) Wraps the slice in a temporary volume (@c VA_CreateVol) to reuse
*        the framework's ROI statistics functions (@c VA_VolCalcRoiInfo).
*     3) Emits only the requested outputs, in order, using @c Write().
*
* @warning
*   Bounds are derived from free parameters; ensure they are within [0, NumTms).
*
* @complexity
*   O(N) time and O(N) scratch memory for N = number of selected frames.
*/

bool	M0_ModelFunc(
	PDOUBLE	Signal,
	PIVAL		OutParm )
{
PDOUBLE	Tac	= NULL;
PFRAME	V	= NULL;
bool		res	= false;

	xz( AllocMem<double >(Tac,NumTms ));
	funcSigToConc( Signal,NumTms,Tac,1,NULL );


int	Start,End;
	if ((gStart==0) && (gEnd==0))	{ Start = 0; End = NumTms-1; }
	else					{ Start = gStart; End = gEnd; }


const PDOUBLE	TP = Tac+Start;
const int		NT = End-Start+1;

	
double MinSi,MaxSi;
	PR_GetArrMinMax( TP,NT,&MinSi,&MaxSi );

	
	// Calculate Entropy
double MaxVal,MeanSi,StdDev,CoeffOfVariation,Median,Skew,Kurtosis;

	{
	DIM3D Dim( NT,1,1,1 );
	xz( V = VA_CreateVol( TP,64,&Dim ));

	double	Vmin = FindMinVal( TP,NT,NULL ),
			Vmax = FindMaxVal( TP,NT,NULL );

	VA_ROIINFO RoiInfo;
	xz( VA_VolCalcRoiInfo( false,V,0,NULL,0,Vmin,Vmax,false,VOIDVAL,0,&RoiInfo ));

	MaxVal	= RoiInfo.RoiMaxVox;
	MeanSi	= RoiInfo.AvgSi;
	StdDev	= RoiInfo.StdDev;
	CoeffOfVariation = RoiInfo.CoeffOfVariation();
	Median	= RoiInfo.Median;
	Skew		= RoiInfo.Skewness;
	Kurtosis	= RoiInfo.Kurtosis;
	}
	

	if ( ParmReq[0] ) Write( OutParm,MaxVal );			// "Signal Spread"
	if ( ParmReq[1] )	Write( OutParm,MaxSi-MinSi );			// "Signal Spread"
	if ( ParmReq[2] )	Write( OutParm,Median );			// Median signal
	if ( ParmReq[3] )	Write( OutParm,MeanSi );			// "Mean Signal"	
	if ( ParmReq[4] ) Write( OutParm,StdDev );			// "Signal StdDev"
	if ( ParmReq[5] ) Write( OutParm,CoeffOfVariation );		// "Signal StdDev"
	if ( ParmReq[6] ) Write( OutParm,Skew );				
	if ( ParmReq[7] ) Write( OutParm,Kurtosis );
	
	res	= true;
func_exit:
	PR_FrameDelete(&V);
	pf_free(&Tac);
	return res;
}




