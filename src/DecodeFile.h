#pragma once
#include <iostream>
#include "utils/SafeQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}


class DecodeFile
{
private:
    // 存储解码后的帧
    SafeQueue<AVFrame*> video_frames;
    SafeQueue<AVFrame*> audio_frames;
    string file_path;
    AVFormatContext* format_ctx;
    AVCodecContext* video_codec_ctx;
    AVCodecContext* audio_codec_ctx;
    int video_stream_index;
    int audio_stream_index;

public:
    explicit DecodeFile(const char *file_path):file_path(file_path), format_ctx(nullptr), video_codec_ctx(nullptr), audio_codec_ctx(
            nullptr), video_stream_index(-1), audio_stream_index(-1) {};
    bool InitDeCode();

};