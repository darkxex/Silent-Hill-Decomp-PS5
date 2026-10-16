#include <AL/al.h>
#include <AL/alc.h>

/* Silent audio backend until the PS5 audio output is wired up. */
void alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei samplerate) {   }
void alBufferiv(ALuint buffer, ALenum param, const ALint *values) {   }
void alDeleteBuffers(ALsizei n, const ALuint *buffers) {   }
void alDeleteSources(ALsizei n, const ALuint *sources) {   }
void alDistanceModel(ALenum distanceModel) {   }
void alGenBuffers(ALsizei n, ALuint *buffers) { static ALuint next = 1; for (ALsizei i = 0; i < n; i++) buffers[i] = next++; }
void alGenSources(ALsizei n, ALuint *sources) { static ALuint next = 1; for (ALsizei i = 0; i < n; i++) sources[i] = next++; }
ALenum alGetError(void) {  return 0; }
void* alGetProcAddress(const ALchar *fname) {  return 0; }
void alGetSourcei(ALuint source, ALenum param, ALint *value) {   }
ALboolean alIsExtensionPresent(const ALchar *extname) {  return 0; }
void alListener3f(ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) {   }
void alListenerf(ALenum param, ALfloat value) {   }
void alListenerfv(ALenum param, const ALfloat *values) {   }
void alSource3f(ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) {   }
void alSource3i(ALuint source, ALenum param, ALint value1, ALint value2, ALint value3) {   }
void alSourcePause(ALuint source) {   }
void alSourcePlay(ALuint source) {   }
void alSourceQueueBuffers(ALuint source, ALsizei nb, const ALuint *buffers) {   }
void alSourceStop(ALuint source) {   }
void alSourceUnqueueBuffers(ALuint source, ALsizei nb, ALuint *buffers) {   }
void alSourcef(ALuint source, ALenum param, ALfloat value) {   }
void alSourcei(ALuint source, ALenum param, ALint value) {   }
ALCboolean alcCloseDevice(ALCdevice *device) {  return 0; }
ALCcontext* alcCreateContext(ALCdevice *device, const ALCint *attrlist) {  return 0; }
void alcDestroyContext(ALCcontext *context) {   }
ALCcontext* alcGetCurrentContext(void) {  return 0; }
ALCenum alcGetError(ALCdevice *device) {  return 0; }
void alcGetIntegerv(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values) {   }
ALCvoid* alcGetProcAddress(ALCdevice *device, const ALCchar *funcname) {  return 0; }
const ALCchar* alcGetString(ALCdevice *device, ALCenum param) {  return 0; }
ALCboolean alcIsExtensionPresent(ALCdevice *device, const ALCchar *extname) {  return 0; }
ALCboolean alcMakeContextCurrent(ALCcontext *context) {  return 0; }
ALCdevice* alcOpenDevice(const ALCchar *devicename) {  return 0; }
