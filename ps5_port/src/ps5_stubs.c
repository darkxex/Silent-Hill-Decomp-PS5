#include "fmv/fmv_player.h"

int FMV_Play(int file_idx, int max_frames)
{
    (void)file_idx;
    (void)max_frames;
    return 0;
}

void FMV_Init(void) {}

void FMV_Shutdown(void) {}

float g_PcFmvVolume = 1.0f;
int g_PcFmvPsxVolume = 1;

int FMV_GetCount(void) { return 0; }

const char* FMV_GetName(int tableIdx)
{
    (void)tableIdx;
    return "";
}

int FMV_GetFileIdx(int tableIdx)
{
    (void)tableIdx;
    return -1;
}

int Pc_Ra_PreviewFirst(void) { return 0; }

int Pc_Ra_Why(const char* filter)
{
    (void)filter;
    return 0;
}

#include <stddef.h>

const size_t ps5_opengl_heap_size = 1536u * 1024u * 1024u;
