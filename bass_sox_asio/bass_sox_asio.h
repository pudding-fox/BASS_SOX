#include "../bass/bass.h"
#include "../bass/bass_addon.h"

BOOL BASSDEF(DllMain)(HANDLE dll, DWORD reason, LPVOID reserved);

const VOID* BASSDEF(BASSplugin)(DWORD face);

BOOL BASSDEF(BASS_SOX_ASIO_StreamGet)(DWORD* handle);

BOOL BASSDEF(BASS_SOX_ASIO_StreamSet)(DWORD handle);

BOOL BASSDEF(BASS_SOX_ASIO_ChannelEnable)(BOOL input, DWORD channel, void *user);