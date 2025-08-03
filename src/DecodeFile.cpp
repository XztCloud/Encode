#include "DecodeFile.h"
#include <string>
#include "utils/SaveFrameToPNG.h"

#define SAVE_PNG (0)

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
        cout << "src video width:" << video_codec_ctx->width << ", src video height:" << video_codec_ctx->height << endl;
        AVRational avg_frame_rate = format_ctx->streams[video_stream_index]->avg_frame_rate; // 平均帧率
        AVRational r_frame_rate = format_ctx->streams[video_stream_index]->r_frame_rate; // 真实帧率
        double avg_fps = av_q2d(avg_frame_rate);
        double r_fps = av_q2d(r_frame_rate);
        cout << "avg frame rate:" << avg_fps << ", real frame rate:" << r_fps << endl;
    }
    // 初始化音频解码器
    if (audio_stream_index != -1) {
        audio_codec_ctx = avcodec_alloc_context3(audio_codec);
        avcodec_parameters_to_context(audio_codec_ctx, format_ctx->streams[audio_stream_index]->codecpar);
        if (avcodec_open2(audio_codec_ctx, audio_codec, nullptr) < 0) {
            cerr << "Cloud not open audio codec" << endl;
            return false;
        }
        cout << "sample rate:" << audio_codec_ctx->sample_rate << endl;
        // 检查音频解码器参数
        if (audio_codec_ctx) {
            cout << "Audio decoder config:\n"
                 << "  sample_fmt: " << av_get_sample_fmt_name(audio_codec_ctx->sample_fmt) << "\n"
                 << "  channels: " << audio_codec_ctx->ch_layout.nb_channels << "\n"
                 << "  sample_rate: " << audio_codec_ctx->sample_rate << endl;

            // 验证输入流参数是否匹配
            AVStream* audio_stream = format_ctx->streams[audio_stream_index];
            if (audio_codec_ctx->sample_fmt == AV_SAMPLE_FMT_NONE) {
                cerr << "ERROR: Audio sample format not negotiated!" << endl;
                return false;
            }
        }

    }
    return true;
}

void DecodeFile::StartDecode() {

    AVPacket* pkt = av_packet_alloc();
    int64_t pts_count = 0;
    while (av_read_frame(format_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_index) {
            // 视频解码
            if (avcodec_send_packet(video_codec_ctx, pkt) < 0) {
                cerr << "Error sending video packet" << endl;
                continue;
            }
            AVFrame* frame = av_frame_alloc();
            if (!frame) {
                cerr << "Failed to allocate audio frame" << endl;
                continue;
            }
            while (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
                frame->pict_type = AV_PICTURE_TYPE_NONE;
                AVFrame* frame_copy = av_frame_alloc();
                av_frame_ref(frame_copy, frame);    // 深拷贝
                if (SAVE_PNG) {
                    if (frame_copy->format == AV_PIX_FMT_YUV420P) {
                        SaveFrameToPNG(frame_copy);
                    }
                }
                video_frames->push(frame_copy);  // 存入队列
                av_frame_unref(frame); // 必须解除引用
            }
            av_frame_free(&frame);
        } else if (pkt->stream_index == audio_stream_index) {
            // 音频解码
            int send_ret = avcodec_send_packet(audio_codec_ctx, pkt);
            if (send_ret < 0 && send_ret != AVERROR(EAGAIN)) {
                cerr << "Error sending audio packet: " << ffmpeg_error_string(send_ret) << endl;
                continue;
            }

            while (true) {
                AVFrame* frame = av_frame_alloc();
                int recv_ret = avcodec_receive_frame(audio_codec_ctx, frame);

                if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) {
                    av_frame_free(&frame);  // 释放空帧
                    break;  // 需要更多输入或结束
                }
                else if (recv_ret < 0) {
                    cerr << "Audio decode error: " << ffmpeg_error_string(recv_ret) << endl;
                    av_frame_free(&frame);
                    break;
                }

                // 正常处理解码后的帧
                printf("Audio frame: pts=%lld\n", frame->pts);
                AVFrame* frame_copy = av_frame_alloc();
                av_frame_ref(frame_copy, frame);
                audio_frames->push(frame_copy);
                av_frame_free(&frame);
            }
        }
        av_packet_unref(pkt); // 释放数据包
    }
    // 刷新解码器缓冲区
    if (video_codec_ctx) {
        avcodec_send_packet(video_codec_ctx, nullptr);
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            cerr << "Failed to allocate audio frame" << endl;
        }
        while (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
            AVFrame *frame_copy = av_frame_alloc();
            av_frame_ref(frame_copy, frame);
            video_frames->push(frame_copy);
            av_frame_unref(frame); // 必须解除引用
        }
        av_packet_unref(pkt); // 释放数据包
    }
    if (audio_codec_ctx) {
        avcodec_send_packet(audio_codec_ctx, nullptr);
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            cerr << "Failed to allocate audio frame" << endl;
        }
        while (avcodec_receive_frame(audio_codec_ctx, frame) >= 0) {
            AVFrame *frame_copy = av_frame_alloc();
            av_frame_ref(frame_copy, frame);
            audio_frames->push(frame_copy);
            av_frame_unref(frame); // 必须解除引用
        }
        av_packet_unref(pkt); // 释放数据包
    }

    // 清理资源
    av_packet_free(&pkt);
    if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    avformat_close_input(&format_ctx);
}

bool DecodeFile::GetVideoInfo(int &width, int &height, int &fps, AVPixelFormat& pix_fmt, AVCodecID& video_codec_id) {
    // 获取视频信息
    if (!video_codec_ctx || !format_ctx) {
        cerr << "Video codec context or format context is null" << endl;
        return false;
    }
    width = video_codec_ctx->width;
    height = video_codec_ctx->height;
    AVRational avg_frame_rate = format_ctx->streams[video_stream_index]->avg_frame_rate; // 平均帧率
    AVRational r_frame_rate = format_ctx->streams[video_stream_index]->r_frame_rate; // 真实帧率
    double avg_fps = av_q2d(avg_frame_rate);
    double r_fps = av_q2d(r_frame_rate);
    fps = (int)avg_fps;
    pix_fmt = video_codec_ctx->pix_fmt;
    video_codec_id = video_codec_ctx->codec_id;
    return true;
}

bool DecodeFile::GetAudioInfo(int &sample_rate, int &channels, int64_t &bit_rate, AVCodecID& audio_codec_id) {
    if (!audio_codec_ctx) {
        cerr << "Audio codec context is null" << endl;
        return false;
    }
    sample_rate = audio_codec_ctx->sample_rate;
    channels = audio_codec_ctx->ch_layout.nb_channels;
    bit_rate = audio_codec_ctx->bit_rate;
    audio_codec_id = audio_codec_ctx->codec_id;
    return true;
}
