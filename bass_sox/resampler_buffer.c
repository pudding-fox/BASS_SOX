#if WITH_DEV_TRACE
#include <stdio.h>
#endif

#include <math.h>

#include "resampler_buffer.h"
#include "resampler_lock.h"

static size_t get_buffer_length(size_t buffer_length, int rate, size_t frame_size) {
	//TODO: I'm sure there's a better way to do this, without floating point.
	return (size_t)(((float)buffer_length / 1000) * (rate * frame_size));
}

BOOL resampler_buffer_create(BASS_SOX_RESAMPLER* resampler) {
	size_t input_buffer_length = get_buffer_length(resampler->settings->input_buffer_length, resampler->input_rate, resampler->input_frame_size);
	size_t resampler_buffer_length = get_buffer_length(resampler->settings->input_buffer_length, resampler->output_rate, resampler->output_frame_size);
	size_t playback_buffer_length = get_buffer_length(resampler->settings->playback_buffer_length, resampler->output_rate, resampler->output_frame_size);
	if (playback_buffer_length < resampler_buffer_length) {
#if WITH_DEV_TRACE
		printf("Warning: Resampler buffer length is less than playback buffer length.");
#endif
		playback_buffer_length = resampler_buffer_length;
	}
	if (resampler->buffer) {
		resampler_buffer_free(resampler);
	}
#if WITH_DEV_TRACE
	printf("Buffers: Input = %d, Resampler = %d, Playback = %d", input_buffer_length, resampler_buffer_length, playback_buffer_length);
#endif
	resampler->buffer = calloc(sizeof(BASS_SOX_RESAMPLER_BUFFER), 1);
	if (!resampler->buffer) {
		return FALSE;
	}
	resampler->buffer->input_buffer_capacity = input_buffer_length;
	resampler->buffer->resampler_buffer_capacity = resampler_buffer_length;
	resampler->buffer->input_buffer = malloc(resampler->buffer->input_buffer_capacity);
	if (!resampler->buffer->input_buffer) {
		resampler_buffer_free(resampler);
		return FALSE;
	}
	resampler->buffer->resampler_buffer = malloc(resampler->buffer->resampler_buffer_capacity);
	if (!resampler->buffer->resampler_buffer) {
		resampler_buffer_free(resampler);
		return FALSE;
	}
	resampler->buffer->playback_buffer = ring_buffer_create(playback_buffer_length);
	return TRUE;
}

BOOL resampler_buffer_clear(BASS_SOX_RESAMPLER* resampler) {
	if (!resampler_lock_enter(resampler, IGNORE)) {
		return FALSE;
	}
	if (resampler->buffer) {
#if WITH_DEV_TRACE
		printf("Clearing buffers.\n");
#endif
		resampler->buffer->input_buffer_length = 0;
		resampler->buffer->resampler_buffer_length = 0;
		if (resampler->buffer->playback_buffer) {
			ring_buffer_clear(resampler->buffer->playback_buffer);
		}
	}
	resampler_lock_exit(resampler);
	return TRUE;
}

BOOL resampler_buffer_free(BASS_SOX_RESAMPLER* resampler) {
	if (resampler->buffer) {
#if WITH_DEV_TRACE
		printf("Releasing buffers.\n");
#endif
		if (resampler->buffer->playback_buffer) {
			ring_buffer_free(resampler->buffer->playback_buffer);
			resampler->buffer->playback_buffer = NULL;
		}
		if (resampler->buffer->input_buffer) {
			free(resampler->buffer->input_buffer);
			resampler->buffer->input_buffer = NULL;
		}
		if (resampler->buffer->resampler_buffer) {
			free(resampler->buffer->resampler_buffer);
			resampler->buffer->resampler_buffer = NULL;
		}
		free(resampler->buffer);
		resampler->buffer = NULL;
	}
	return TRUE;
}

BOOL resampler_buffer_length(BASS_SOX_RESAMPLER* resampler, DWORD* length) {
	double ratio;
	*length = 0;
	if (!resampler_lock_enter(resampler, IGNORE)) {
		return FALSE;
	}
	ratio = (double)resampler->input_rate / (double)resampler->output_rate;
	if (resampler->buffer) {
		*length += resampler->buffer->input_buffer_length;
		*length += (DWORD)ceil(resampler->buffer->resampler_buffer_length * ratio);
		*length += (DWORD)ceil(ring_buffer_read_capacity(resampler->buffer->playback_buffer) * ratio);
	}
	if (resampler->soxr_delay) {
		*length += (DWORD)ceil((resampler->soxr_delay * resampler->output_sample_size * resampler->channels) * ratio);
	}
	resampler_lock_exit(resampler);
#if WITH_DEV_TRACE
	printf("Current buffer length: %d\n", *length);
#endif
	return TRUE;
}