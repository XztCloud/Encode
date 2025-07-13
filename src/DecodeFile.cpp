#include "DecodeFile.h"


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
                video_frames.push(frame_copy);  // 存入队列
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
                audio_frames.push(frame_copy);
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
            video_frames.push(frame_copy);
        }
    }
    if (audio_codec_ctx) {
        avcodec_send_packet(audio_codec_ctx, nullptr);
        while (avcodec_receive_frame(audio_codec_ctx, frame) >= 0) {
            AVFrame *frame_copy = av_frame_alloc();
            av_frame_ref(frame_copy, frame);
            audio_frames.push(frame_copy);
        }
    }

    // 清理资源
    av_packet_free(&pkt);
    av_frame_free(&frame);
    if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    avformat_close_input(&format_ctx);
}

void DecodeFile::SaveFrameToPNG(AVFrame* frame) {
    SwsContext* sws_ctx = sws_getContext(
            frame->width, frame->height, (AVPixelFormat)frame->format,  // 输入格式
            frame->width, frame->height, AV_PIX_FMT_RGB24,             // 输出格式
            SWS_BILINEAR, NULL, NULL, NULL
    );
    if (!sws_ctx) {
        fprintf(stderr, "Failed to create SwsContext\n");
        return;
    }
    // 分配目标帧（RGB24）
    AVFrame* rgb_frame = av_frame_alloc();
    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = frame->width;
    rgb_frame->height = frame->height;
    av_frame_get_buffer(rgb_frame, 0);

    // 执行转换
    sws_scale(sws_ctx,
              frame->data, frame->linesize, 0, frame->height,
              rgb_frame->data, rgb_frame->linesize
    );

    // 清理 SwsContext
    sws_freeContext(sws_ctx);

    // 查找 PNG 编码器
    const AVCodec* png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!png_codec) {
        fprintf(stderr, "PNG encoder not found\n");
        av_frame_free(&rgb_frame);
        return;
    }
    // 初始化编码器上下文
    AVCodecContext* enc_ctx = avcodec_alloc_context3(png_codec);
    enc_ctx->width = rgb_frame->width;
    enc_ctx->height = rgb_frame->height;
    enc_ctx->pix_fmt = AV_PIX_FMT_RGB24;
    enc_ctx->time_base = (AVRational){1, 25};  // 无关紧要，但必须设置

    if (avcodec_open2(enc_ctx, png_codec, NULL) < 0) {
        fprintf(stderr, "Could not open PNG encoder\n");
        avcodec_free_context(&enc_ctx);
        av_frame_free(&rgb_frame);
        return;
    }
    // 创建 AVPacket 接收编码数据
    AVPacket* pkt = av_packet_alloc();

    // 发送帧到编码器
    if (avcodec_send_frame(enc_ctx, rgb_frame) < 0) {
        fprintf(stderr, "Error sending frame to encoder\n");
        av_packet_unref(pkt);
        av_packet_free(&pkt);
        avcodec_close(enc_ctx);
        avcodec_free_context(&enc_ctx);
        av_frame_free(&rgb_frame);
    }

    // 接收编码后的 PNG 数据
    if (avcodec_receive_packet(enc_ctx, pkt) < 0) {
        fprintf(stderr, "Error encoding PNG\n");
        av_packet_unref(pkt);
        av_packet_free(&pkt);
        avcodec_close(enc_ctx);
        avcodec_free_context(&enc_ctx);
        av_frame_free(&rgb_frame);
    }
    static int cnt = 0;
    string out_file = "pic/output"+ to_string(cnt)+".png";
    cnt++;
    // 写入文件
    FILE* file = fopen(out_file.c_str(), "wb");
    fwrite(pkt->data, 1, pkt->size, file);
    fclose(file);

    // 清理资源
    av_packet_unref(pkt);
    av_packet_free(&pkt);
    avcodec_close(enc_ctx);
    avcodec_free_context(&enc_ctx);
    av_frame_free(&rgb_frame);
}

int main()
{
    cout << "hello world!" << endl;
    auto* decodeFile = new DecodeFile("C:\\Users\\XztCloud\\Videos\\test.mp4");
    bool ret = decodeFile->InitDeCode();
    cout << "InitDecode ret is " << ret << endl;
    if (!ret) {
        return -1;
    }
    decodeFile->StartDecode();
    int num;
    cin >> num;
    return 0;
}
