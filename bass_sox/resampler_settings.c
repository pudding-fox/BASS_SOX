#include "resampler_settings.h"

BOOL resampler_settings_create(BASS_SOX_RESAMPLER* resampler) {
	resampler->settings = calloc(sizeof(BASS_SOX_RESAMPLER_SETTINGS), 1);
	if (!resampler->settings) {
		return FALSE;
	}
	resampler->settings->quality = SOXR_DEFAULT_QUALITY;
	resampler->settings->phase = SOXR_DEFAULT_PHASE;
	resampler->settings->steep_filter = SOXR_DEFAULT_STEEP_FILTER;
	resampler->settings->input_buffer_length = DEFAULT_INPUT_BUFFER_LENGTH;
	resampler->settings->playback_buffer_length = DEFAULT_PLAYBACK_BUFFER_LENGTH;
	resampler->settings->background = DEFAULT_BACKGROUND;
	resampler->settings->keep_alive = DEFAULT_KEEP_ALIVE;
	resampler->settings->no_dither = DEFAULT_NO_DITHER;
	return TRUE;
}

BOOL resampler_settings_free(BASS_SOX_RESAMPLER* resampler) {
	free(resampler->settings);
	resampler->settings = NULL;
	return TRUE;
}