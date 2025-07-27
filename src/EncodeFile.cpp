#include "EncodeFile.h"
#pragma once

bool EncodeFile::InitFormat()
{
    avformat_alloc_output_context2(&format_ctx, nullptr, nullptr, out_file_path.c_str());
    if(!format_ctx) {
        fprintf(stderr, "Cloud not create output context\n");
        return false;
    }
    const AVOutputFormat *fmt = format_ctx->oformat;

    // 添加视频流
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        std::cout << "output fmt is " << fmt->video_codec << std::endl;
        if (!InitVideoEncoder("libx264", m_width, m_height, fps, 400000, AV_PIX_FMT_YUV420P)) {
            fprintf(stderr, "Init video encoder failed.\n");
            CleanSource();
            return false;
        }

        video_stream = avformat_new_stream(format_ctx, nullptr);
        if (!video_stream) {
            fprintf(stderr, "Could not create video stream.\n");
            CleanSource();
            return false;
        }
        cout << "video stream id:" << video_stream->id << ", format_ctx nb_streams:" << format_ctx->nb_streams << endl;
        video_stream->id = (int)format_ctx->nb_streams - 1;
        avcodec_parameters_from_context(video_stream->codecpar, video_codec_ctx);
        cout << "video_codec_ctx->time_base.num:" << video_codec_ctx->time_base.num << "video_codec_ctx->time_base.den:" << video_codec_ctx->time_base.den << endl;
        video_stream->time_base = video_codec_ctx->time_base;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        int audio_sample_rate = 16000;
        std::cout << "output fmt is " << fmt->audio_codec << std::endl;
        if (!InitAudioEncoder("aac", audio_sample_rate, 2, 200000)) {
            fprintf(stderr, "Init audio encoder failed.\n");
            CleanSource();
            return false;
        }
        audio_stream = avformat_new_stream(format_ctx, nullptr);
        if(!audio_stream) {
            fprintf(stderr, "Cloud not create audio stream\n");
            CleanSource();
            return false;
        }
        cout << "audio stream id:" << audio_stream->id << ", format_ctx nb_streams:" << format_ctx->nb_streams << endl;
        audio_stream->id = (int)format_ctx->nb_streams - 1;
        avcodec_parameters_from_context(audio_stream->codecpar, audio_codec_ctx);
        audio_stream->time_base = {1, audio_sample_rate};
    }
    return true;
}

bool EncodeFile::InitAudioEncoder(const char *codec_name, int sample_rate, int channels, int bitrate) {
    // 查找编码器
    const AVCodec *codec = avcodec_find_decoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec %s not found", codec_name);
        return false;
    }

    // 创建编码器上下文
    audio_codec_ctx = avcodec_alloc_context3(codec);
    if (!audio_codec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return false;
    }
    audio_codec_ctx->bit_rate = bitrate;
    audio_codec_ctx->sample_rate = sample_rate;
    audio_codec_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    av_channel_layout_uninit(&audio_codec_ctx->ch_layout);
    av_channel_layout_default(&audio_codec_ctx->ch_layout, channels);

    // 打开编码器
    if (avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        fprintf(stderr, "Could not open audio codec.\n");
        avcodec_free_context(&audio_codec_ctx);
        return false;
    }
    return true;
}

bool EncodeFile::InitVideoEncoder(const char *codec_name, int width, int height, int fps, int bitrate, AVPixelFormat pix_fmt)
{
    const AVCodec *codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec %s not found.", codec_name);
        return false;
    }

    // 创建编码器上下文
    video_codec_ctx = avcodec_alloc_context3(codec);
    if (!video_codec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return false;
    }

    // 设置编码器参数
    video_codec_ctx->bit_rate = bitrate;
    video_codec_ctx->width = width;
    video_codec_ctx->height = height;
    video_codec_ctx->time_base = {1, fps};
    video_codec_ctx->framerate = {fps, 1};
    video_codec_ctx->gop_size = 25;
    video_codec_ctx->max_b_frames = 1;
    video_codec_ctx->pix_fmt = pix_fmt;

    AVDictionary *opt = nullptr;
    // 对于H264 可以设置额外参数
    if (video_codec_ctx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&opt, "preset", "slow", 0); // 编码质量预设
        av_dict_set(&opt, "crf", "23", 0);      // 恒定质量模式
        av_dict_set(&opt, "tune", "film", 0);
    }
    // 打开编码器
    if (avcodec_open2(video_codec_ctx, codec, &opt) < 0) {
        fprintf(stderr, "Could not open codec\n");
        avcodec_free_context(&video_codec_ctx);
        av_dict_free(&opt);
        video_codec_ctx = nullptr;
        return false;
    }
    av_dict_free(&opt);
    return true;
}

void EncodeFile::CleanSource() {
    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
    }
    if (audio_codec_ctx) {
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
    }
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    if (format_ctx && (format_ctx->flags & AVFMT_NOFILE)) avio_closep(&format_ctx->pb);
    if (format_ctx) {
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }
}

void EncodeFile::StartEncode()
{
    // 打开输出文件
    if (!(format_ctx->flags & AVFMT_NOFILE)) {
        if (avio_open(&format_ctx->pb, out_file_path.c_str(), AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Cloud not open output file %s\n", out_file_path.c_str());
            CleanSource();
        }
    }
    // 写入头文件
    if (avformat_write_header(format_ctx, nullptr) < 0) {
        fprintf(stderr, "Error writing header.\n");
        CleanSource();
    }

    size_t audio_size = audio_frames->size();
    size_t video_size = video_frames->size();

    cout << "audio size:" << audio_size << ", video size:" << video_size << endl;

    int start_time = 0;
    AVFrame *audio_frame = av_frame_alloc();
    AVFrame *video_frame = av_frame_alloc();
    while (video_frames->size() > 0) {
//        if (audio_frames->pop(audio_frame)) {
//            if (EncodeAndWriteAudio(audio_codec_ctx, format_ctx, audio_stream, audio_frame) != 0) {
//                break;
//            }
//        }
        if (video_frames->pop(video_frame)) {
            if (EncodeAndWriteVideo(video_codec_ctx, format_ctx, video_stream, video_frame) != 0) {
//                break;
            }
        }
//        av_frame_unref(audio_frame);
        av_frame_unref(video_frame);
    }
    av_frame_free(&audio_frame);
    av_frame_free(&video_frame);
    // 刷新视频编码器
//    ret = encode_and_write_video(video_codec_ctx, fmt_ctx, video_stream, NULL);
    if (EncodeAndWriteVideo(video_codec_ctx, format_ctx, video_stream, nullptr) < 0) {
        fprintf(stderr, "Error flushing video encoder\n");
        CleanSource();
    }

    CleanSource();
}

int EncodeFile::EncodeAndWriteVideo(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx, AVStream *stream, AVFrame *frame) {
    AVPacket *pkt = av_packet_alloc();
    if(!pkt) {
        fprintf(stderr, "Cloud not allocate packet\n");
        return -1;
    }

    // 发送帧到编码器
    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error send frame to encoder.\n");
        return -1;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding.\n");
            break;
        }
        // 设置数据包时间戳，由编码时间戳改为封装时间戳
        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        cout << "pkt->pts:" << pkt->pts;
        // 写入编码后的数据包
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error writing video packet\n");
            break;
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    return ret == AVERROR_EOF ? 0 : ret;
}

int
EncodeFile::EncodeAndWriteAudio(AVCodecContext *codec_Ctx, AVFormatContext *fmt_ctx, AVStream *stream, AVFrame *frame)
{
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Cloud not allocate packet\n");
        return -1;
    }
    // 发送帧到编码器
    int ret = avcodec_send_frame(codec_Ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending audio frame to encoder\n");
        return -1;
    }

    while(ret >= 0) {
        ret = avcodec_receive_packet(codec_Ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during audio encoding \n");
            break;
        }
        av_packet_rescale_ts(pkt, codec_Ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;

        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error writing audio packet\n");
            break;
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    return ret == AVERROR_EOF ? 0 : ret;
}
