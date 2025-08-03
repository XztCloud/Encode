#include "EncodeFile.h"

bool EncodeFile::InitFormat()
{
    avformat_alloc_output_context2(&format_ctx, nullptr, "mp4", out_file_path.c_str());
    if(!format_ctx) {
        fprintf(stderr, "Cloud not create output context\n");
        return false;
    }
    const AVOutputFormat *fmt = format_ctx->oformat;

    // 添加视频流
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        std::cout << "output fmt is " << fmt->video_codec << std::endl;
        if (!InitVideoEncoder(params.video_codec_id, params.width, params.height, params.fps, 4000000, params.pix_fmt)) {
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
        std::cout << "output fmt is " << fmt->audio_codec << std::endl;

        if (!InitAudioEncoder(params.audio_codec_id, params.sample_rate, params.channels, params.audio_bit_rate)) {
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
        audio_stream->time_base = {1, params.sample_rate};
    }
    return true;
}

bool EncodeFile::InitAudioEncoder(AVCodecID codec_id, int sample_rate, int channels, int64_t bitrate) {
    // 查找编码器
    const AVCodec *codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec %s not found",avcodec_get_name(codec_id));
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

    // 强制关键参数一致性
    if (audio_codec_ctx->sample_fmt != AV_SAMPLE_FMT_FLTP) {
        fprintf(stderr, "Unsupported sample format: %d\n", audio_codec_ctx->sample_fmt);
        return false;
    }

    // 检查帧尺寸对齐
    if (audio_codec_ctx->frame_size <= 0) {
        audio_codec_ctx->frame_size = 1024; // AAC典型值
    }

    // 打开编码器
    if (avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        fprintf(stderr, "Could not open audio codec.\n");
        avcodec_free_context(&audio_codec_ctx);
        return false;
    }
    return true;
}

bool EncodeFile::InitVideoEncoder(AVCodecID codec_id, int width, int height, int fps, int bitrate, AVPixelFormat pix_fmt)
{
    const AVCodec *codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Codec %s not found.", avcodec_get_name(codec_id));
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
    video_codec_ctx->gop_size = fps;    // 未生效
    video_codec_ctx->max_b_frames = 0;
    video_codec_ctx->pix_fmt = pix_fmt;

    AVDictionary *opt = nullptr;
    // 对于H264 可以设置额外参数
    if (video_codec_ctx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&opt, "preset", "fast", 0); // 编码质量预设
        av_dict_set(&opt, "tune", "film", 0);
        // av_dict_set(&opt, "keyint", "50", 0);
        // av_dict_set(&opt, "min-keyint", "50", 0);
        // av_dict_set(&opt, "force-cfr", "1", 0);   // 固定帧率模式
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
        printf("video queue size:%zu\n", video_frames->size());
        if (video_frames->pop(video_frame)) {
            if (EncodeAndWriteVideo(video_codec_ctx, format_ctx, video_stream, video_frame) != 0) {
//                break;
            }
        }
//        av_frame_unref(audio_frame);
        av_frame_unref(video_frame);
    }
    while (audio_frames->size() > 0) {
        if (audio_frames->pop(audio_frame)) {
            if (EncodeAndWriteAudio(audio_codec_ctx, format_ctx, audio_stream, audio_frame) != 0) {
                // break;
            }
            av_frame_unref(audio_frame);
        }
    }
    av_frame_free(&audio_frame);
    av_frame_free(&video_frame);
    // 刷新视频编码器
    int ret = EncodeAndWriteVideo(video_codec_ctx, format_ctx, video_stream, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Flushing failed: %s\n", ffmpeg_error_string(ret));
        CleanSource();
        return;
    }
    ret = EncodeAndWriteAudio(audio_codec_ctx, format_ctx, audio_stream, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Flushing failed: %s\n", ffmpeg_error_string(ret));
        CleanSource();
        return;
    }
    printf("finish encode\n");

    // 写入文件尾
    if (av_write_trailer(format_ctx) < 0) {
        fprintf(stderr, "Failed to write trailer\n");
    }

    // 确保关闭文件句柄
    if (!(format_ctx->flags & AVFMT_NOFILE)) {
        avio_closep(&format_ctx->pb);
    }
    CleanSource();
}

int EncodeFile::EncodeAndWriteVideo(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx, AVStream *stream, AVFrame *frame) {
    AVPacket *pkt = av_packet_alloc();
    if(!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        return -1;
    }
    static int64_t frame_count = 0;
    if (frame) {
        printf("ori frame->pts: %lld, duration:%lld\n", frame->pts, frame->duration);
        frame->pts = frame_count;
        frame->duration = 1;  // 每帧持续一个时间基单位
        if(frame_count % 50 == 0) {
            frame->pict_type = AV_PICTURE_TYPE_I;
            frame->key_frame = 1;
        }
        frame_count++;
    }

    // 发送帧到编码器
    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending frame to encoder: %s\n", ffmpeg_error_string(ret));
        av_packet_free(&pkt);
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error during encoding: %s\n", ffmpeg_error_string(ret));
            break;
        }
        // 设置数据包时间戳，从编码器时间基转为流时间基
        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;

        printf("pkt->pts:%lld, pkt->duration:%lld\n", pkt->pts, pkt->duration);
        if (frame != nullptr) {
            printf("frame->pts: %lld\n", frame->pts);
        }

        // 写入编码后的数据包
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        if (ret < 0) {
            fprintf(stderr, "Error writing video packet: %s\n", ffmpeg_error_string(ret));
            break;
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    // 若返回值为 AVERROR_EOF，说明正常结束，否则返回错误码
    return ret == AVERROR_EOF ? 0 : ret;
}

int EncodeFile::EncodeAndWriteAudio(AVCodecContext *codec_Ctx, AVFormatContext *fmt_ctx, AVStream *stream, AVFrame *frame)
{
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Cloud not allocate packet\n");
        return -1;
    }
    // 发送帧到编码器
    int ret = avcodec_send_frame(codec_Ctx, frame);
    if (ret < 0) {
        av_packet_free(&pkt);
        fprintf(stderr, "Error sending audio frame to encoder, %s\n", ffmpeg_error_string(ret));
        return ret;
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
    return (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) ? 0 : ret;
}
