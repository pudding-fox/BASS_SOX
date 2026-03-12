#if WITH_DEV_TRACE
#include <stdio.h>
#endif

#include "bass_sox_asio.h"
#include "../bass_sox/resampler.h"
#include "../bass_sox/resampler_registry.h"
#include "../bass/bassasio.h"

//2.4.0.0
#define BASS_VERSION 0x02040000

DWORD channel_handle = 0;

//I have no idea how to prevent linking against this routine in msvcrt.
//It doesn't exist on Windows XP.
//Hopefully it doesn't do anything important.
int _except_handler4_common() {
	return 0;
}

static const BASS_PLUGININFO plugin_info = { BASS_VERSION, 0, NULL };

BOOL BASSDEF(DllMain)(HANDLE dll, DWORD reason, LPVOID reserved) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls((HMODULE)dll);
		if (HIWORD(BASS_GetVersion()) != BASSVERSION || !GetBassFunc()) {
			MessageBoxA(0, "Incorrect BASS.DLL version (" BASSVERSIONTEXT " is required)", "BASS", MB_ICONERROR | MB_OK);
			return FALSE;
		}
		break;
	case DLL_PROCESS_DETACH:
		channel_handle = 0;
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

BOOL BASSDEF(BASS_SOX_ASIO_StreamGet)(DWORD* handle) {
	if (!channel_handle) {
		return FALSE;
	}
	*handle = channel_handle;
#if WITH_DEV_TRACE
	printf("BASS SOX ASIO stream: %d.\n", channel_handle);
#endif
	return TRUE;
}

BOOL BASSDEF(BASS_SOX_ASIO_StreamSet)(DWORD handle) {
	BASS_SOX_RESAMPLER* resampler;
	if (!resampler_registry_get(handle, &resampler)) {
#if WITH_DEV_TRACE
		printf("No resampler for channel: %d\n", handle);
#endif
		return FALSE;
	}
	channel_handle = handle;
#if WITH_DEV_TRACE
	printf("BASS SOX ASIO stream: %d.\n", channel_handle);
#endif
	return TRUE;
}

DWORD CALLBACK asio_sox_stream_proc(BOOL input, DWORD channel, void* buffer, DWORD length, void* user) {
	DWORD result;
	if (!channel_handle) {
		result = 0;
	}
	else {
		result = BASS_ChannelGetData(channel_handle, buffer, length);
		switch (result)
		{
		case BASS_STREAMPROC_END:
		case BASS_ERROR_UNKNOWN:
			result = 0;
			break;
		default:
#if WITH_DEV_TRACE
			printf("Write %d bytes to ASIO buffer\n", result);
#endif
			break;
		}
	}
	if (result < length) {
#if WITH_DEV_TRACE
		printf("Buffer underrun while writing to ASIO buffer.\n");
#endif
	}
	return result;
}

BOOL BASSDEF(BASS_SOX_ASIO_ChannelEnable)(BOOL input, DWORD channel, void *user) {
	BOOL success = BASS_ASIO_ChannelEnable(input, channel, &asio_sox_stream_proc, user);
	if (!success) {
#if WITH_DEV_TRACE
		printf("BASS SOX ASIO enabled.\n");
#endif
	}
	return success;
}