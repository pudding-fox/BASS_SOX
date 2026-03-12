#if WITH_DEV_TRACE
#include <stdio.h>
#endif

#include "resampler.h"
#include "resampler_buffer.h"
#include "resampler_registry.h"
#include "resampler_lock.h"
#include "resampler_settings.h"
#include "resampler_soxr.h"

#define BASS_ERROR 0xFFFFFFFF

BASS_SOX_RESAMPLER* resampler_create() {
	BASS_SOX_RESAMPLER* resampler = calloc(sizeof(BASS_SOX_RESAMPLER), 1);
	return resampler;
}

BOOL resampler_eof(BASS_SOX_RESAMPLER* resampler) {
	return resampler->source_end && resampler->soxr_end;
}

BOOL resampler_free(BASS_SOX_RESAMPLER* resampler) {
	BOOL success = TRUE;
#if WITH_DEV_TRACE
	printf("Releasing resampler.\n");
#endif
	success &= resampler_soxr_free(resampler);
	success &= resampler_buffer_free(resampler);
	success &= resampler_settings_free(resampler);
	success &= resampler_lock_free(resampler);
	free(resampler);
	resampler = NULL;
	if (!success) {
#if WITH_DEV_TRACE
		printf("Failed to release resampler.\n");
#endif
	}
	return success;
}

BOOL ensure_resampler(BASS_SOX_RESAMPLER* resampler) {
	BOOL success = TRUE;
	//If reload is requested then release the current resampler.
	if (resampler->reload) {
#if WITH_DEV_TRACE
		printf("Reloading resampler.\n");
#endif
		resampler->ready = FALSE;
		success &= resampler_soxr_free(resampler);
		success &= resampler_buffer_free(resampler);
		resampler->reload = FALSE;
	}

	//If no resampler exists then create it.
	if (!resampler->soxr) {
		if (!resampler_soxr_create(resampler)) {
			success = FALSE;
		}
		else if (!resampler_buffer_create(resampler)) {
#if WITH_DEV_TRACE
			printf("Failed to allocate buffers.\n");
#endif
			success = FALSE;
		}
		else {
			resampler->ready = TRUE;
		}
	}
	return success;
}

BOOL read_input_data(BASS_SOX_RESAMPLER* resampler) {
	BASS_SOX_RESAMPLER_BUFFER* buffer = resampler->buffer;
	buffer->input_buffer_length = BASS_ChannelGetData(
		resampler->source_channel,
		buffer->input_buffer,
		(DWORD)buffer->input_buffer_capacity
	);
	if (buffer->input_buffer_length == BASS_ERROR) {
#if WITH_DEV_TRACE
		printf("Error reading from source channel.\n");
#endif
		buffer->input_buffer_length = 0;
		resampler->source_end = TRUE;
		return FALSE;
	}
	else if ((buffer->input_buffer_length & BASS_STREAMPROC_END) == BASS_STREAMPROC_END) {
#if WITH_DEV_TRACE
		printf("Source channel has ended.\n");
#endif
		buffer->input_buffer_length &= ~BASS_STREAMPROC_END;
		resampler->source_end = TRUE;
	}
	else {
#if WITH_DEV_TRACE
		printf("Read %d bytes from source channel.\n", buffer->input_buffer_length);
#endif
		if (resampler->source_end) {
			resampler->source_end = FALSE;
		}
	}
	if (buffer->input_buffer_length) {
		return TRUE;
	}
	return FALSE;
}

BOOL read_resampled_data(BASS_SOX_RESAMPLER* resampler) {
	size_t input_frame_count;
	size_t output_frame_count;
	BASS_SOX_RESAMPLER_BUFFER* buffer = resampler->buffer;
	if (!buffer->input_buffer_length) {
		if (!read_input_data(resampler)) {
#if WITH_DEV_TRACE
			printf("Could not read any input data.\n");
#endif
		}
	}
	resampler->soxr_error = soxr_process(
		resampler->soxr,
		buffer->input_buffer,
		buffer->input_buffer_length / resampler->input_frame_size,
		&input_frame_count,
		buffer->resampler_buffer,
		buffer->resampler_buffer_capacity / resampler->output_frame_size,
		&output_frame_count
	);
	if (resampler->soxr_error) {
#if WITH_DEV_TRACE
		printf("Sox reported an error: %s\n", resampler->soxr_error);
#endif
		return FALSE;
	}
	resampler->soxr_delay = soxr_delay(resampler->soxr);
	buffer->input_buffer_length = 0;
	buffer->resampler_buffer_length = output_frame_count * resampler->output_frame_size;
#if WITH_DEV_TRACE
	if (input_frame_count) {
		printf("Read %d samples from input buffer.\n", input_frame_count);
	}
	if (output_frame_count) {
		printf("Wrote %d samples to output buffer.\n", output_frame_count);
	}
#endif
	if (input_frame_count || output_frame_count) {
		resampler->soxr_end = FALSE;
		return TRUE;
	}
	else {
		resampler->soxr_end = TRUE;
		return FALSE;
	}
}

BOOL read_playback_data(BASS_SOX_RESAMPLER* resampler) {
	BASS_SOX_RESAMPLER_BUFFER* buffer = resampler->buffer;
	RING_BUFFER* playback_buffer = buffer->playback_buffer;
	if (!buffer->resampler_buffer_length) {
		if (!read_resampled_data(resampler)) {
			return FALSE;
		}
	}
	if (buffer->resampler_buffer_length > ring_buffer_write_capacity(playback_buffer)) {
		return FALSE;
	}
	ring_buffer_write(playback_buffer, buffer->resampler_buffer, buffer->resampler_buffer_length);
	buffer->resampler_buffer_length = 0;
	return TRUE;
}

BOOL resampler_populate(BASS_SOX_RESAMPLER* resampler) {
	DWORD success = FALSE;
	if (!resampler_lock_enter(resampler, IGNORE)) {
		return FALSE;
	}
	if (ensure_resampler(resampler)) {
		if (read_playback_data(resampler)) {
			success = TRUE;
		}
	}
	resampler_lock_exit(resampler);
	return success;
}

DWORD CALLBACK resampler_proc(HSTREAM handle, void* buffer, DWORD length, void* user) {
	DWORD remaining = length;
	DWORD position = 0;
	BASS_SOX_RESAMPLER* resampler = (BASS_SOX_RESAMPLER*)user;
	RING_BUFFER* playback_buffer;
	if (!resampler->settings->background) {
		resampler_populate(resampler);
	}
	if (!resampler->ready) {
#if WITH_DEV_TRACE
		printf("Resampler is not yet ready.\n");
#endif
		return 0;
	}
	playback_buffer = resampler->buffer->playback_buffer;
	position = (DWORD)ring_buffer_read(playback_buffer, buffer, length);
	if (!position && resampler_eof(resampler)) {
		if (!resampler->settings->keep_alive) {
			position = BASS_STREAMPROC_END;
		}
		else {
#if WITH_DEV_TRACE
			printf("Ignoring BASS_STREAMPROC_END due to keep_alive.\n");
#endif
		}
	}
	else if (position < length) {
#if WITH_DEV_TRACE
		printf("Buffer underrun while writing to playback buffer.\n");
#endif
	}
	return position;
}