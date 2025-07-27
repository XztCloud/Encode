#pragma once

#include <iostream>
#include <utility>
#include "utils/SafeQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
using namespace std;
class EncodeFile
{
private:
	AVFormatContext* format_ctx;
	AVCodecContext* audio_codec_ctx;
	AVCodecContext* video_codec_ctx;
    AVStream *audio_stream;
    AVStream *video_stream;
    SwsContext* sws_ctx;

	SafeQueue<AVFrame*>* audio_frames;
	SafeQueue<AVFrame*>* video_frames;
	string out_file_path;
    int m_width;
    int m_height;
    int fps;

    void CleanSource();
	
public:
    EncodeFile(string _out_file_path, int width, int height, int fps, SafeQueue<AVFrame*>* audio_queue, SafeQueue<AVFrame*>* video_queue) : \
    out_file_path(std::move(_out_file_path)), m_width(width), fps(fps), m_height(height), audio_frames(audio_queue), video_frames(video_queue) {
        audio_codec_ctx = nullptr;
        video_codec_ctx = nullptr;
        audio_stream = nullptr;
        video_stream = nullptr;
        format_ctx = nullptr;
        sws_ctx = nullptr;
    }

	bool InitFormat();
    bool InitAudioEncoder(const char* codec_name, int sample_rate, int channels, int bitrate);
    bool InitVideoEncoder(const char* codec_name, int width, int height, int fps, int bitrate, AVPixelFormat pix_fmt);
	void StartEncode();

    static int EncodeAndWriteVideo(AVCodecContext* codec_ctx, AVFormatContext* fmt_ctx, AVStream* stream, AVFrame* frame);
    int EncodeAndWriteAudio(AVCodecContext* codec_Ctx, AVFormatContext* fmt_ctx, AVStream* stream, AVFrame* frame);
};
