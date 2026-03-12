#if WITH_DEV_TRACE
#include <stdio.h>
#endif

#include "bass_sox.h"
#include "resampler.h"
#include "resampler_buffer.h"
#include "resampler_registry.h"
#include "resampler_updater.h"
#include "resampler_lock.h"
#include "resampler_settings.h"

//2.4.0.0
#define BASS_VERSION 0x02040000

#define BUFFER_CLEAR_TIMEOUT 5000

//I have no idea how to prevent linking against this routine in msvcrt.
//It doesn't exist on Windows XP.
//Hopefully it doesn't do anything important.
int _except_handler4_common() {
	return 0;
}

static const BASS_PLUGININFO plugin_info = { BASS_VERSION, 0, NULL };

BOOL BASSDEF(DllMain)(HANDLE dll, DWORD reason, LPVOID reserved) {
	DWORD a;
	DWORD length;
	static BASS_SOX_RESAMPLER* resamplers[MAX_RESAMPLERS];
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls((HMODULE)dll);
		if (HIWORD(BASS_GetVersion()) != BASSVERSION || !GetBassFunc()) {
			MessageBoxA(0, "Incorrect BASS.DLL version (" BASSVERSIONTEXT " is required)", "BASS", MB_ICONERROR | MB_OK);
			return FALSE;
		}
		break;
	case DLL_PROCESS_DETACH:
		if (resampler_registry_get_all(resamplers, &length)) {
			for (a = 0; a < length; a++) {
				BASS_StreamFree(resamplers[a]->output_channel);
			}
		}
		break;
	}
	return TRUE;
}

const VOID* BASSDEF(BASSplugin)(DWORD face) {
	switch (face) {
	case BASSPLUGIN_INFO:
		return (void*)&plugin_info;
	}
	return NULL;
}

//Determine whether the specified flags imply BASS_SAMPLE_FLOAT.
BOOL is_float(DWORD flags) {
	return  (flags & BASS_SAMPLE_FLOAT) == BASS_SAMPLE_FLOAT;
}

static void CALLBACK BASS_SOX_StreamFree(HSYNC handle, DWORD channel, DWORD data, void* user) {
	BASS_SOX_RESAMPLER* resampler;
	if (!resampler_registry_get(channel, &resampler)) {
#if WITH_DEV_TRACE
		printf("No resampler for channel: %d\n", channel);
#endif
		return;
	}
#if WITH_DEV_TRACE
	printf("Releasing resampler.\n");
#endif
	BASS_StreamFree(channel);
	resampler_updater_stop(resampler);
	resampler_registry_remove(resampler);
	resampler_free(resampler);
}

//Create a BASS stream containing a resampler payload for the specified frequency (freq).
HSTREAM BASSDEF(BASS_SOX_StreamCreate)(DWORD freq, DWORD flags, DWORD handle, void* user) {
	BOOL success = TRUE;
	BASS_CHANNELINFO input_channel_info;
	BASS_CHANNELINFO output_channel_info;
	BASS_SOX_RESAMPLER* resampler;
	HSTREAM output_channel;

	if (!BASS_ChannelGetInfo(handle, &input_channel_info)) {
#if WITH_DEV_TRACE
		printf("Failed to get info for channel: %d\n", handle);
#endif
		return 0;
	}

	if (input_channel_info.freq == freq) {
#if WITH_DEV_TRACE
		printf("Channel does not require resampling.\n");
#endif
		return handle;
	}

	resampler = resampler_create();
	output_channel = BASS_StreamCreate(freq, input_channel_info.chans, flags, &resampler_proc, resampler);
	if (output_channel == 0) {
#if WITH_DEV_TRACE
		printf("Failed to create output channel.\n");
#endif
		return 0;
	}

	if (!BASS_ChannelGetInfo(output_channel, &output_channel_info)) {
#if WITH_DEV_TRACE
		printf("Failed to get info for channel: %d\n", output_channel);
#endif
		return 0;
	}

	resampler->input_rate = input_channel_info.freq;
	resampler->output_rate = freq;
	resampler->channels = input_channel_info.chans;
	resampler->source_channel = handle;
	resampler->output_channel = output_channel;
	resampler->input_sample_size = is_float(input_channel_info.flags) ? sizeof(float) : sizeof(short);
	resampler->input_frame_size = resampler->input_sample_size * resampler->channels;
	resampler->output_sample_size = is_float(output_channel_info.flags) ? sizeof(float) : sizeof(short);
	resampler->output_frame_size = resampler->output_sample_size * resampler->channels;

	success &= resampler_settings_create(resampler);
	success &= resampler_lock_create(resampler);
	success &= resampler_registry_add(resampler);

	if (!success) {
#if WITH_DEV_TRACE
		printf("Failed to properly construct resampler, releasing it.\n");
#endif
		BASS_StreamFree(output_channel);
		resampler_registry_remove(resampler);
		resampler_free(resampler);
		return 0;
	}

	BASS_ChannelSetSync(output_channel, BASS_SYNC_FREE, 0, &BASS_SOX_StreamFree, NULL);

#if WITH_DEV_TRACE
	printf("Created resampler channel: %d => %d\n", handle, output_channel);
#endif

	return output_channel;
}

BOOL BASSDEF(BASS_SOX_StreamBuffer)(DWORD handle) {
	BOOL success = FALSE;
	BASS_SOX_RESAMPLER* resampler;
	if (!resampler_registry_get(handle, &resampler)) {
#if WITH_DEV_TRACE
		printf("No resampler for channel: %d\n", handle);
#endif
		return FALSE;
	}
#if WITH_DEV_TRACE
	printf("Populating buffers.\n");
#endif
	while (resampler_populate(resampler)) {
		success = TRUE;
	}
	return success;
}

BOOL BASSDEF(BASS_SOX_StreamBufferClear)(DWORD handle) {
	BOOL success;
	BASS_SOX_RESAMPLER* resampler;
	if (!resampler_registry_get(handle, &resampler)) {
#if WITH_DEV_TRACE
		printf("No resampler for channel: %d\n", handle);
#endif
		return FALSE;
	}
	if (!resampler_lock_enter(resampler, BUFFER_CLEAR_TIMEOUT)) {
		return FALSE;
	}
#if WITH_DEV_TRACE
	printf("Clearing buffers.\n");
#endif
	success = resampler_buffer_clear(resampler);
	if (!resampler_lock_exit(resampler)) {
		return FALSE;
	}
	return success;
}

BOOL BASSDEF(BASS_SOX_StreamBufferLength)(DWORD handle, DWORD mode, DWORD* value) {
	BASS_SOX_RESAMPLER* resampler;
	if (!resampler_registry_get(handle, &resampler)) {
#if WITH_DEV_TRACE
		printf("No resampler for channel: %d\n", handle);
#endif
		return FALSE;
	}
	if (mode != BASS_POS_BYTE) {
		error(BASS_ERROR_NOTAVAIL);
		return FALSE;
}
#if WITH_DEV_TRACE
	printf("Getting buffer length\n");
#endif
	return resampler_buffer_length(resampler, value);
}

BOOL BASSDEF(BASS_SOX_ChannelSetAttribute)(DWORD handle, DWORD attrib, DWORD value) {
	BASS_SOX_RESAMPLER* resampler;
	BASS_SOX_RESAMPLER_SETTINGS* settings;
	if (!resampler_registry_get(handle, &resampler)) {
#if WITH_DEV_TRACE
		printf("No resampler for channel: %d\n", handle);
#endif
		return FALSE;
	}
#if WITH_DEV_TRACE
	printf("Setting resampler attribute: %d = %d\n", attrib, value);
#endif
	settings = resampler->settings;
	switch (attrib) {
	case QUALITY:
		settings->quality = value;
		break;
	case PHASE:
		settings->phase = value;
		break;
	case STEEP_FILTER:
		settings->steep_filter = value;
		break;
	case PLAYBACK_BUFFER_LENGTH:
		if (value >= MIN_BUFFER_LEGNTH && value <= MAX_BUFFER_LENGTH) {
			settings->playback_buffer_length = value;
		}
		else {
			return FALSE;
		}
		break;
	case BACKGROUND:
		settings->background = value;
		if (settings->background) {
			resampler_updater_start(resampler);
		}
		else {
			resampler_updater_stop(resampler);
		}
		break;
	case KEEP_ALIVE:
		settings->keep_alive = value;
		break;
	case INPUT_BUFFER_LENGTH:
		if (value >= MIN_BUFFER_LEGNTH && value <= MAX_BUFFER_LENGTH) {
			settings->input_buffer_length = value;
		}
		else {
			return FALSE;
		}
		break;
	case NO_DITHER:
		settings->no_dither = value;
		break;
	default:
#if WITH_DEV_TRACE
		printf("Unrecognized resampler attribute: %d\n", attrib);
#endif
		return FALSE;
	}
	//TODO: Does this need to be synchronized?
#if WITH_DEV_TRACE
	printf("Requesting reload.\n");
#endif
	resampler->reload = TRUE;
	return TRUE;
}

BOOL BASSDEF(BASS_SOX_ChannelGetAttribute)(DWORD handle, DWORD attrib, DWORD* value) {
	BASS_SOX_RESAMPLER* resampler;
	BASS_SOX_RESAMPLER_SETTINGS* settings;
	if (!resampler_registry_get(handle, &resampler)) {
#if WITH_DEV_TRACE
		printf("No resampler for channel: %d\n", handle);
#endif
		return FALSE;
	}
	settings = resampler->settings;
	switch (attrib) {
	case QUALITY:
		*value = settings->quality;
		break;
	case PHASE:
		*value = settings->phase;
		break;
	case STEEP_FILTER:
		*value = settings->steep_filter;
		break;
	case PLAYBACK_BUFFER_LENGTH:
		*value = (DWORD)settings->playback_buffer_length;
		break;
	case BACKGROUND:
		*value = settings->background;
		break;
	case KEEP_ALIVE:
		*value = settings->keep_alive;
		break;
	case INPUT_BUFFER_LENGTH:
		*value = (DWORD)settings->input_buffer_length;
		break;
	case NO_DITHER:
		*value = (DWORD)settings->no_dither;
		break;
	default:
#if WITH_DEV_TRACE
		printf("Unrecognized resampler attribute: %d\n", attrib);
#endif
		return FALSE;
	}
#if WITH_DEV_TRACE
	printf("Getting resampler attribute: %d = %d\n", attrib, value);
#endif
	return TRUE;
}

//Get the last error encountered by sox.
const char* BASSDEF(BASS_SOX_GetLastError)(HSTREAM handle) {
	BASS_SOX_RESAMPLER* resampler;
	if (!resampler_registry_get(handle, &resampler)) {
#if WITH_DEV_TRACE
		printf("No resampler for channel: %d\n", handle);
#endif
		return "No such resampler.";
	}
	return resampler->soxr_error;
}