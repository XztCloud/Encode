#include "DecodeFile.h"
#include <string>
#include "utils/SaveFrameToPNG.h"

bool DecodeFile::InitDeCode() {
    const AVCodec* video_codec = nullptr, *audio_codec = nullptr;
    if (avformat_open_input(&format_ctx, file_path.c_str(), nullptr, nullptr) < 0) {
        std:cerr << "cloud not open file:" << file_path << std::endl;
        return false;
    }
    // 查找流信息
    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        cerr << "Cloud not find stream info" << endl;
        return false;
    }
    // 查找视频流和音频流
    for (int i = 0; i < format_ctx->nb_streams; i++) {
        AVStream* stream = format_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index == -1) {
            video_stream_index = i;
            video_codec = avcodec_find_decoder(stream->codecpar->codec_id);
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_index == -1) {
            audio_stream_index = i;
            cout << "audio encode type is " << stream->codecpar->codec_id << endl;
            const char* codec_name = avcodec_get_name(stream->codecpar->codec_id);

            printf("Audio codec: %s (ID: %d)\n", codec_name, stream->codecpar->codec_id);
            audio_codec = avcodec_find_decoder(stream->codecpar->codec_id);
        }
    }
    // 初始化视频解码器
    if (video_stream_index != -1) {
        video_codec_ctx = avcodec_alloc_context3(video_codec);
        avcodec_parameters_to_context(video_codec_ctx, format_ctx->streams[video_stream_index]->codecpar);
        if (avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0) {
            cerr << "Cloud not open video codec" << endl;
            return false;
        }
    }
    // 初始化音频解码器
    if (audio_stream_index != -1) {
        audio_codec_ctx = avcodec_alloc_context3(audio_codec);
        avcodec_parameters_to_context(audio_codec_ctx, format_ctx->streams[audio_stream_index]->codecpar);
        if (avcodec_open2(audio_codec_ctx, audio_codec, nullptr) < 0) {
            cerr << "Cloud not open audio codec" << endl;
            return false;
        }
    }
    return true;
}

void DecodeFile::StartDecode() {
    AVFrame* frame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    while (av_read_frame(format_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            // 视频解码
            if (avcodec_send_packet(video_codec_ctx, pkt) < 0) {
                cerr << "Error sending video packet" << endl;
                continue;
            }
            while (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
                AVFrame* frame_copy = av_frame_alloc();
                av_frame_ref(frame_copy, frame);    // 深拷贝
                if (frame_copy->format == AV_PIX_FMT_YUV420P) {
                    SaveFrameToPNG(frame_copy);
                }
                video_frames->push(frame_copy);  // 存入队列
            }
        } else if (pkt->stream_index == audio_stream_index) {
            // 音频解码
            if (avcodec_send_packet(audio_codec_ctx, pkt) < 0) {
                cerr << "Error sending audio packet" << endl;
                continue;
            }
            while (avcodec_receive_frame(audio_codec_ctx, frame) >= 0) {
                AVFrame *frame_copy = av_frame_alloc();
                av_frame_ref(frame_copy, frame);
                audio_frames->push(frame_copy);
            }
        }
        av_packet_unref(pkt); // 释放数据包
    }
    // 刷新解码器缓冲区
    if (video_codec_ctx) {
        avcodec_send_packet(video_codec_ctx, nullptr);
        while (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
            AVFrame *frame_copy = av_frame_alloc();
            av_frame_ref(frame_copy, frame);
            video_frames->push(frame_copy);
        }
    }
    if (audio_codec_ctx) {
        avcodec_send_packet(audio_codec_ctx, nullptr);
        while (avcodec_receive_frame(audio_codec_ctx, frame) >= 0) {
            AVFrame *frame_copy = av_frame_alloc();
            av_frame_ref(frame_copy, frame);
            audio_frames->push(frame_copy);
        }
    }

    // 清理资源
    av_packet_free(&pkt);
    av_frame_free(&frame);
    if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    avformat_close_input(&format_ctx);
}
