#include <AL/al.h>
#include <AL/alc.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

/* Silent audio backend until the PS5 audio output is wired up. Playback is timed
 * in software: the game waits on AL_SOURCE_STATE / AL_BUFFERS_PROCESSED to know a
 * voice line has ended, and scripted events (the first cutscene dialogue) only
 * advance once it has. */
#define STUB_MAX_BUFFERS 4096
#define STUB_MAX_SOURCES 256
#define STUB_MAX_QUEUE   64

typedef struct { double durMs; } StubBuffer;

typedef struct {
    ALuint  queue[STUB_MAX_QUEUE];
    int     queued;
    int     processed;
    int     state;
    int     looping;
    double  headMs;
    double  startMs;
    double  pausedAtMs;
} StubSource;

static StubBuffer g_bufs[STUB_MAX_BUFFERS];
static StubSource g_srcs[STUB_MAX_SOURCES];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static double NowMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

static StubSource* Src(ALuint id)
{
    return (id > 0 && id < STUB_MAX_SOURCES) ? &g_srcs[id] : NULL;
}

static double BufDur(ALuint id)
{
    return (id > 0 && id < STUB_MAX_BUFFERS) ? g_bufs[id].durMs : 0.0;
}

static void Advance(StubSource* s)
{
    double now;

    if (s->state != AL_PLAYING)
        return;
    now = NowMs();
    while (s->processed < s->queued)
    {
        double dur = BufDur(s->queue[s->processed]);

        if (now - s->startMs < dur)
            return;
        s->startMs += dur;
        if (s->looping && s->queued == 1)
            continue;
        s->processed++;
    }
    s->state = AL_STOPPED;
}

void alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei samplerate)
{
    int bytesPerFrame = 4;

    if (buffer == 0 || buffer >= STUB_MAX_BUFFERS || samplerate <= 0)
        return;
    switch (format)
    {
    case AL_FORMAT_MONO8:   bytesPerFrame = 1; break;
    case AL_FORMAT_MONO16:
    case AL_FORMAT_STEREO8: bytesPerFrame = 2; break;
    default:                bytesPerFrame = 4; break;
    }
    pthread_mutex_lock(&g_lock);
    g_bufs[buffer].durMs = (double)size / (double)bytesPerFrame * 1000.0 / (double)samplerate;
    pthread_mutex_unlock(&g_lock);
}
void alBufferiv(ALuint buffer, ALenum param, const ALint *values) {   }
void alDeleteBuffers(ALsizei n, const ALuint *buffers) {   }
void alDeleteSources(ALsizei n, const ALuint *sources)
{
    pthread_mutex_lock(&g_lock);
    for (ALsizei i = 0; i < n; i++)
    {
        StubSource* s = Src(sources[i]);
        if (s) memset(s, 0, sizeof(*s));
    }
    pthread_mutex_unlock(&g_lock);
}
void alDistanceModel(ALenum distanceModel) {   }
void alGenBuffers(ALsizei n, ALuint *buffers) { static ALuint next = 1; for (ALsizei i = 0; i < n; i++) buffers[i] = next++; }
void alGenSources(ALsizei n, ALuint *sources)
{
    static ALuint next = 1;
    for (ALsizei i = 0; i < n; i++)
    {
        sources[i] = next++;
        if (Src(sources[i])) g_srcs[sources[i]].state = AL_INITIAL;
    }
}
ALenum alGetError(void) {  return 0; }
void* alGetProcAddress(const ALchar *fname) {  return 0; }
void alGetSourcei(ALuint source, ALenum param, ALint *value)
{
    StubSource* s = Src(source);

    if (!s || !value)
        return;
    pthread_mutex_lock(&g_lock);
    Advance(s);
    switch (param)
    {
    case AL_SOURCE_STATE:      *value = s->state ? s->state : AL_INITIAL; break;
    case AL_BUFFERS_PROCESSED: *value = s->processed; break;
    case AL_BUFFERS_QUEUED:    *value = s->queued; break;
    case AL_LOOPING:           *value = s->looping; break;
    case AL_BUFFER:            *value = (s->queued > 0) ? (ALint)s->queue[0] : 0; break;
    default:                   *value = 0; break;
    }
    pthread_mutex_unlock(&g_lock);
}
ALboolean alIsExtensionPresent(const ALchar *extname) {  return 0; }
void alListener3f(ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) {   }
void alListenerf(ALenum param, ALfloat value) {   }
void alListenerfv(ALenum param, const ALfloat *values) {   }
void alSource3f(ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) {   }
void alSource3i(ALuint source, ALenum param, ALint value1, ALint value2, ALint value3) {   }
void alSourcef(ALuint source, ALenum param, ALfloat value) {   }
void alSourcePause(ALuint source)
{
    StubSource* s = Src(source);

    if (!s) return;
    pthread_mutex_lock(&g_lock);
    Advance(s);
    if (s->state == AL_PLAYING) { s->state = AL_PAUSED; s->pausedAtMs = NowMs(); }
    pthread_mutex_unlock(&g_lock);
}
void alSourcePlay(ALuint source)
{
    StubSource* s = Src(source);

    if (!s) return;
    pthread_mutex_lock(&g_lock);
    Advance(s);
    if (s->state == AL_PAUSED)
    {
        s->startMs += NowMs() - s->pausedAtMs;
        s->state = AL_PLAYING;
    }
    else if (s->state != AL_PLAYING)
    {
        if (s->processed >= s->queued)
            s->processed = 0;
        s->startMs = NowMs();
        s->state = (s->queued > 0) ? AL_PLAYING : AL_STOPPED;
    }
    pthread_mutex_unlock(&g_lock);
}
void alSourceQueueBuffers(ALuint source, ALsizei nb, const ALuint *buffers)
{
    StubSource* s = Src(source);

    if (!s) return;
    pthread_mutex_lock(&g_lock);
    for (ALsizei i = 0; i < nb && s->queued < STUB_MAX_QUEUE; i++)
        s->queue[s->queued++] = buffers[i];
    pthread_mutex_unlock(&g_lock);
}
void alSourceStop(ALuint source)
{
    StubSource* s = Src(source);

    if (!s) return;
    pthread_mutex_lock(&g_lock);
    if (s->state != AL_INITIAL)
    {
        s->state = AL_STOPPED;
        s->processed = s->queued;
    }
    pthread_mutex_unlock(&g_lock);
}
void alSourceUnqueueBuffers(ALuint source, ALsizei nb, ALuint *buffers)
{
    StubSource* s = Src(source);

    if (!s) return;
    pthread_mutex_lock(&g_lock);
    for (ALsizei i = 0; i < nb && s->processed > 0 && s->queued > 0; i++)
    {
        if (buffers) buffers[i] = s->queue[0];
        memmove(&s->queue[0], &s->queue[1], sizeof(ALuint) * (size_t)(s->queued - 1));
        s->queued--;
        s->processed--;
    }
    pthread_mutex_unlock(&g_lock);
}
void alSourcei(ALuint source, ALenum param, ALint value)
{
    StubSource* s = Src(source);

    if (!s) return;
    pthread_mutex_lock(&g_lock);
    if (param == AL_BUFFER)
    {
        s->queued = 0;
        s->processed = 0;
        if (value > 0) s->queue[s->queued++] = (ALuint)value;
        s->state = AL_INITIAL;
    }
    else if (param == AL_LOOPING)
        s->looping = value ? 1 : 0;
    pthread_mutex_unlock(&g_lock);
}
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
