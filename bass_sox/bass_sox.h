#include "../bass/bass.h"
#include "../bass/bass_addon.h"
#include "../libsoxr/soxr.h"

//Some error happens.
#define BASS_SOX_ERROR_UNKNOWN	-1

typedef enum {
	QUALITY = 0,
	PHASE = 1,
	STEEP_FILTER = 2,
	PLAYBACK_BUFFER_LENGTH = 4,
	BACKGROUND = 6,
	KEEP_ALIVE = 7,
	INPUT_BUFFER_LENGTH = 8,
	NO_DITHER = 9
} BASS_SOX_ATTRIBUTE;

typedef enum {
	Q_QQ = SOXR_QQ,
	Q_LQ = SOXR_LQ,
	Q_MQ = SOXR_MQ,
	Q_HQ = SOXR_HQ,
	Q_VHQ = SOXR_VHQ,
	Q_16_BITQ = SOXR_16_BITQ,
	Q_20_BITQ = SOXR_20_BITQ,
	Q_24_BITQ = SOXR_24_BITQ,
	Q_28_BITQ = SOXR_28_BITQ,
	Q_32_BITQ = SOXR_32_BITQ
} BASS_SOX_QUALITY;

typedef enum {
	LINEAR = SOXR_LINEAR_PHASE,
	INTERMEDIATE = SOXR_INTERMEDIATE_PHASE,
	MINIMUM = SOXR_MINIMUM_PHASE
} BASS_SOX_PHASE;

BOOL BASSDEF(DllMain)(HANDLE dll, DWORD reason, LPVOID reserved);

const VOID* BASSDEF(BASSplugin)(DWORD face);

//Create a BASS stream containing a resampler payload for the specified frequency (freq).
HSTREAM BASSDEF(BASS_SOX_StreamCreate)(DWORD freq, DWORD flags, DWORD handle, void* user);

//Prepare some data so the stream can play instantly.
BOOL BASSDEF(BASS_SOX_StreamBuffer)(DWORD handle);

//Clears any buffered data. Use when manually changing position.
BOOL BASSDEF(BASS_SOX_StreamBufferClear)(DWORD handle);

//Get the length of buffered data. Can be used to calculate the stream position.
BOOL BASSDEF(BASS_SOX_StreamBufferLength)(DWORD handle, DWORD mode, DWORD* value);

//Set an attribute on the associated resampler.
BOOL BASSDEF(BASS_SOX_ChannelSetAttribute)(DWORD handle, DWORD attrib, DWORD value);

//Get an attribute from the associated resampler.
BOOL BASSDEF(BASS_SOX_ChannelGetAttribute)(DWORD handle, DWORD attrib, DWORD* value);

//Get the last error encountered by sox.
const char* BASSDEF(BASS_SOX_GetLastError(HSTREAM handle));