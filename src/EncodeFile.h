#pragma once

#include <iostream>
#include "utils/SafeQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
using namespace std;
class EncodeFile
{
private:
	AVFormatContext* format_ctx;
	AVCodecContext* audio_codec_ctx;
	AVCodecContext* video_codec_ctx;

	queue<AVFrame*>* audio_frames;
	queue<AVFrame*>* video_frames;
	string out_file_path;
	
public:
	EncodeFile(const string& out_file_path, queue<AVFrame*>* audio_queue, queue<AVFrame*>* video_queue) : out_file_path(out_file_path), audio_frames(audio_queue), video_frames(video_queue) {
		audio_codec_ctx = nullptr;
		video_codec_ctx = nullptr;
		format_ctx = nullptr;
	}

	bool InitEncode();
	void StartEncode()
};
