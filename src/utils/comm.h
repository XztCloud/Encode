//
// Created by XztCloud on 2025/8/3.
//

#ifndef ENCODE_COMM_H
#define ENCODE_COMM_H
extern "C" {
#include "libavutil/error.h"
}
inline const char* ffmpeg_error_string(int errnum) {
    static thread_local char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return errbuf;
}

#endif //ENCODE_COMM_H
