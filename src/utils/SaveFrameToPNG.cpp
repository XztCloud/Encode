#include "SaveFrameToPNG.h"
#include <string>

void SaveFrameToPNG(AVFrame* frame) {
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
    enc_ctx->time_base = AVRational{ 1, 25 };  // 无关紧要，但必须设置

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
    string out_file = "pic/output" + std::to_string(cnt) + ".png";
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