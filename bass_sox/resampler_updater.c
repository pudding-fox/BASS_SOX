#if WITH_DEV_TRACE
#include <stdio.h>
#endif

#include "resampler_updater.h"

#define UPDATE_PRIORITY THREAD_PRIORITY_HIGHEST

#define UPDATE_INTERVAL 100

#define UPDATE_TIMEOUT 0

DWORD WINAPI background_update(void* args) {
	BASS_SOX_RESAMPLER* resampler = args;
	while (!resampler->shutdown) {
		while (resampler_populate(resampler)) {
			//Nothing to do.
		}
		Sleep(UPDATE_INTERVAL);
	}
	return TRUE;
}

BOOL resampler_updater_start(BASS_SOX_RESAMPLER* resampler) {
	if (resampler->thread) {
		return TRUE;
	}
#if WITH_DEV_TRACE
	printf("Starting background update.\n");
#endif
	resampler->shutdown = FALSE;
	resampler->thread = CreateThread(NULL, 0, background_update, resampler, 0, NULL);
	if (!resampler->thread) {
#if WITH_DEV_TRACE
		printf("Failed to create thread.\n");
#endif
		return FALSE;
	}
	if (!SetThreadPriority(resampler->thread, UPDATE_PRIORITY)) {
#if WITH_DEV_TRACE
		printf("Failed to set thread priority.\n");
#endif
		return FALSE;
	}
	return TRUE;
}

BOOL resampler_updater_stop(BASS_SOX_RESAMPLER* resampler) {
	DWORD result;
	if (!resampler->thread) {
		return TRUE;
	}
#if WITH_DEV_TRACE
	printf("Stopping background update.\n");
#endif
	resampler->shutdown = TRUE;
	result = WaitForSingleObject(resampler->thread, INFINITE);
	if (!CloseHandle(resampler->thread)) {
#if WITH_DEV_TRACE
		printf("Failed to release thread.\n");
#endif
		return FALSE;
	}
	resampler->thread = NULL;
	if (result == WAIT_OBJECT_0) {
#if WITH_DEV_TRACE
		printf("Failed to stop thread.\n");
#endif
		return TRUE;
}
	return FALSE;
}