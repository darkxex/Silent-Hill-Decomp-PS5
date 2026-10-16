#include "ReadAVI.h"

ReadAVI::chunk_type_int_t ReadAVI::chunk_types[ChunkTypesCnt] = {};

ReadAVI::ReadAVI(const char*)
    : avi_header(), stream_header_vid(), stream_format_vid(), stream_header_auds(),
      stream_format_auds(), fileSize(0), data_buf(0), data_buf_size(0), headers_parsed(false)
{
}

ReadAVI::~ReadAVI() {}

int ReadAVI::GetFrameFromIndex(frame_entry_t*) { return -1; }
