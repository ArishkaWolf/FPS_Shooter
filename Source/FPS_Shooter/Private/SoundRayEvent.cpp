#include "SoundRayEvent.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarSoundRaysDebug(
	TEXT("SoundRays.Debug"), 0,
	TEXT("Draw sound rays, hearing evidence and NPC debug labels. 0: off (default), 1: on."), ECVF_Cheat);

bool IsSoundRayDebugEnabled()
{
	return CVarSoundRaysDebug.GetValueOnGameThread() != 0;
}
