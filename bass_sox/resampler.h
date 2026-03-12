#ifndef RESAMPLER
#define RESAMPLER

#include "../bass/bass.h"
#include "../libsoxr/soxr.h"

#include "ring_buffer.h"

#define BASS_ERR -1

typedef struct {
	void* input_buffer;
	size_t input_buffer_length;
	size_t input_buffer_capacity;
	void* resampler_buffer;
	size_t resampler_buffer_length;
	size_t resampler_buffer_capacity;
	RING_BUFFER* playback_buffer;
} BASS_SOX_RESAMPLER_BUFFER;

typedef struct {
	unsigned long quality;
	unsigned long phase;
	BOOL steep_filter;
	size_t input_buffer_length;
	size_t playback_buffer_length;
	BOOL background;
	BOOL keep_alive;
	BOOL no_dither;
} BASS_SOX_RESAMPLER_SETTINGS;

typedef struct {
	int input_rate;
	int output_rate;
	int channels;
	HSTREAM source_channel;
	HSTREAM output_channel;
	BOOL source_end;
	size_t input_sample_size;
	size_t input_frame_size;
	size_t output_sample_size;
	size_t output_frame_size;
	BASS_SOX_RESAMPLER_BUFFER* buffer;
	BASS_SOX_RESAMPLER_SETTINGS* settings;
	soxr_t soxr;
	soxr_error_t soxr_error;
	BOOL soxr_end;
	double soxr_delay;
	BOOL reload;
	HANDLE lock;
	HANDLE thread;
	volatile BOOL shutdown;
	volatile BOOL ready;
} BASS_SOX_RESAMPLER;

BASS_SOX_RESAMPLER* resampler_create();

BOOL resampler_free(BASS_SOX_RESAMPLER* resampler);

BOOL resampler_populate(BASS_SOX_RESAMPLER* resampler);

__declspec(dllexport)
DWORD CALLBACK resampler_proc(HSTREAM handle, void* buffer, DWORD length, void* user);

#endif