/**
* @brief Initialize Model 3 (interleaved odd/even statistics).
*
* Sets the per‑call model state pointer to @c NULL. Input functions and their
* count are accepted but not used by this model.
*
* @param[out] pModelState Opaque per‑call state pointer (set to @c NULL).
* @param[in]  IFarr       Array of input functions (unused).
* @param[in]  NumIF       Number of input functions (unused).
*
* @return bool @c true on success (initialization is trivial and unconditional).
*
* @pre  @c NumTms is valid for the current TAC.
* @post No resources retained; model state remains @c NULL.
*
* @thread_safety Not thread‑safe (relies on framework globals elsewhere).
*/

#include	"stdafx.h"

char	M3_IFpanelName[]	= "";
char	M3_ModelName[]	= "3. Interleaved 2-state profile";

int	M3_NumIfuncs = 0;

const int	M3_NumFreeParms	= 0;
const int	M3_NumOutParms	= 4;

BOOL	M3_UseNoise		= FALSE;
BOOL	M3_UseGlobalTac	= FALSE;
BOOL	M3_OutFitCurve	= FALSE;
BOOL	M3_ExtrapolateEnable	= FALSE;

UINT32 M3_AllowedOptim	= BM(VA_OPTIM_NONE);			// Allowed optimizations
UINT32 M3_Optim		= VA_OPTIM_NONE;	
int	 M3_OptimGridN	= 0;
int	 M3_OptimNiter	= 0;


UINT32 M3_Modality	= MCLASS_MSK_ALL;
UINT32 M3_DynDim		= DYNDIM_MSK_ALL;
UINT32 M3_ConcConv	= BM(CONCTYPE_NOCONV);

double M3_FreeParm[1];
double M3_FreeParmDefault[1];

PSTR	M3_FPName[1];

static char	OPName0[] =  "mean of odd frames";
static char	OPName1[] =  "stdev of odd frames";
static char	OPName2[] =  "mean of even frames";
static char	OPName3[] =  "stdev of even frames";
PSTR	M3_OPName[M3_NumOutParms] = { OPName0,OPName1,OPName2,OPName3 };

static char	OPUnits0[] = "";
static char	OPUnits1[] = "";
static char	OPUnits2[] = "";
static char	OPUnits3[] = "";
PSTR	M3_OPUnits[M3_NumOutParms] = { OPUnits0,OPUnits1,OPUnits2,OPUnits3 };

PR_CLRMAP	M3_ClrScheme[M3_NumOutParms] = { PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW,PR_CLRMAP_RAINBOW };


/**
* @brief Initialize Model 3 (interleaved odd/even statistics).
*
* Sets the per‑call model state pointer to @c NULL. Input functions and their
* count are accepted but not used by this model.
*
* @param[out] pModelState Opaque per‑call state pointer (set to @c NULL).
* @param[in]  IFarr       Array of input functions (unused).
* @param[in]  NumIF       Number of input functions (unused).
*
* @return bool @c true on success (initialization is trivial and unconditional).
*
* @pre  @c NumTms is valid for the current TAC.
* @post No resources retained; model state remains @c NULL.
*
* @thread_safety Not thread‑safe (relies on framework globals elsewhere).
*/

bool	M3_ModelInit(
	PVOID*	pModelState,
	PINPUTFUNC	IFarr,
	int		NumIF )
{
bool	res	= false;

	*pModelState = NULL;

	res	= true;
func_exit:
	return res;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////
void	M3_ModelClose( PVOID ModelState )
{
}



static int	ExtractEven(
		PDOUBLE	Tac,
		int		N,
		PDOUBLE	Arr )
{
int	NE = (N+1)/2;
	for ( int i=0; i<NE; i++ )
		Arr[i] = Tac[i*2];
	return NE;
}

static int	ExtractOdd(
		PDOUBLE	Tac,
		int		N,
		PDOUBLE	Arr )
{
int	ND = N/2;
	for ( int i=0; i<ND; i++ )
		Arr[i] = Tac[i*2+1];
	return ND;
}


/**
* @brief Compute odd/even frame means and standard deviations and emit them.
*
* Converts @p Sig to concentration units, splits the TAC into two interleaved
* subseries using the 1‑based frame convention (odd: frames 1,3,5,…; even:
* frames 2,4,6,…), computes mean and stdev for each via @c PR_ArrStats(), and
* writes requested outputs in this fixed order:
*   OP[0] = mean(odd‑numbered frames)
*   OP[1] = stdev(odd‑numbered frames)
*   OP[2] = mean(even‑numbered frames)
*   OP[3] = stdev(even‑numbered frames)
*
* @param[in]  Sig     Pointer to TAC samples (length @c NumTms) in time order.
*                     Internally converted with
*                     @code funcSigToConc(Sig, NumTms, Tac, 1, NULL); @endcode
* @param[out] OutParm Framework‑managed writer used by @c Write().
*
* @return bool @c true on success; @c false if an allocation or guarded call
*              fails (as enforced by framework macros).
*
* @pre
*   - @c NumTms > 0 and @c Sig has @c NumTms elements.
*   - TAC is sorted by increasing acquisition time.
*
* @post
*   - Temporary buffers (@c Tac and work array) are freed before return.
*
* @details
*   The implementation selects odd‑numbered frames (1‑based) by copying
*   0‑based even indices via @c ExtractEven(), then selects even‑numbered
*   frames (1‑based) via @c ExtractOdd(). Means and stdevs are computed with
*   @c PR_ArrStats(). Outputs are gated by @c ParmReq[0..3].
*
* @warning
*   Ensure the indexing convention matches your downstream expectations:
*   odd/even refers to **frame numbers** (1‑based), not array indices. :contentReference[oaicite:2]{index=2}
*
* @complexity
*   O(N) time and O(N) temporary memory where N = @c NumTms.
*/

bool	M3_ModelFunc(
	PDOUBLE	Sig,			//Signal
	PIVAL		OutParm )
{
PDOUBLE	Tac	= NULL;
PDOUBLE	Arr	= NULL;
bool		res	= false;


	xz( AllocMem<double >(Tac,NumTms ));
	funcSigToConc( Sig,NumTms,Tac,1,NULL );

	xz( AllocMem<double >(Arr,(NumTms+1)/2 ));

	// Process ODD timepoints
	// We need to select even because of the Tstart=1
int	N = ExtractEven( Tac,NumTms,Arr );
double	EvenStdev,
		EvenMean = PR_ArrStats( Arr,N,&EvenStdev );

	N = ExtractOdd( Tac,NumTms,Arr );
double	OddStdev,
		OddMean = PR_ArrStats( Arr,N,&OddStdev );

	if ( ParmReq[0] )	Write( OutParm,EvenMean );
	if ( ParmReq[1] )	Write( OutParm,EvenStdev );				
	if ( ParmReq[2] )	Write( OutParm,OddMean );
	if ( ParmReq[3] ) Write( OutParm,OddStdev );

	res	= true;
func_exit:
	pf_free(&Tac);
	pf_free(&Arr);
	return res;
}
