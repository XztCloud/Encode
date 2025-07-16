//
// Created by XztCloud on 2025/7/13.
//

#ifndef ENCODE_SAVEFRAMETOOL_H
#define ENCODE_SAVEFRAMETOOL_H
#include <iostream>
#include "utils/SafeLatestQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
class SaveFrameTool {
private:
    SafeLatestQueue<uint8_t*> audio_data_queue;
    SafeLatestQueue<uint8_t*> video_data_queue;
    string file_path;
    int sample_rate;
    int m_width;
    int m_height;
    AVFormatContext* format_ctx;
    AVCodecContext* video_codec_ctx;
    AVCodecContext* audio_codec_ctx;

public:
    SaveFrameTool(const char* file_path) : file_path(file_path), format_ctx(nullptr), video_codec_ctx(nullptr), audio_codec_ctx(
        nullptr) {};
    bool InitEncode(string);

};


#endif //ENCODE_SAVEFRAMETOOL_H
