#ifndef NoiseSuppressionDLL_h__
#define NoiseSuppressionDLL_h__

#ifdef NSDLL_EXPORT
#ifdef WIN32
#define NSDLL_API extern "C" __declspec(dllexport)
#else
#define NSDLL_API extern "C" __attribute__((visibility("default")))
#endif
#else
#define NSDLL_API
#endif

#include <stdint.h>

typedef struct NSContextT NSContext;

/*
* This function creates an instance of the floating point Noise Suppression.
*/
NSDLL_API NSContext* NS_Create();

NSDLL_API void NS_Free(NSContext* NS_inst);

/*
* This function initializes a NS instance and has to be called before any other
* processing is made.
*
* Input:
*      - NS_inst       : Instance that should be initialized
*      - fs            : sampling frequency
*
* Output:
*      - NS_inst       : Initialized instance
*
* Return value         :  0 - Ok
*                        -1 - Error
*/
NSDLL_API int NS_Init(NSContext* NS_inst, uint32_t fs, uint32_t bit, int channels);

/*
* This changes the aggressiveness of the noise suppression method.
*
* Input:
*      - NS_inst       : Noise suppression instance.
*      - mode          : 0: Mild, 1: Medium , 2: Aggressive
*
* Output:
*      - NS_inst       : Updated instance.
*
* Return value         :  0 - Ok
*                        -1 - Error
*/
NSDLL_API int NS_Set_Policy(NSContext* NS_inst, int mode);

/*
* This functions does Noise Suppression for the inserted speech frame. The
* input and output signals should always be 10ms (80 or 160 samples).
*
* Input
*      - NS_inst       : Noise suppression instance.
*      - spframe       : Pointer to speech frame buffer for each band
*      - num_bands     : Number of bands
*
* Output:
*      - NS_inst       : Updated NS instance
*      - outframe      : Pointer to output frame for each band
*/
NSDLL_API void NS_Process(NSContext* NS_inst,
	uint8_t *sample_data,
	int sample_count,
	uint8_t **out_sample);

/* Returns the internally used prior speech probability of the current frame.
* There is a frequency bin based one as well, with which this should not be
* confused.
*
* Input
*      - handle        : Noise suppression instance.
*
* Return value         : Prior speech probability in interval [0.0, 1.0].
*                        -1 - NULL pointer or uninitialized instance.
*/
NSDLL_API float NS_Prior_Speech_Probability(NSContext* handle);


#endif // NoiseSuppressionDLL_h__
