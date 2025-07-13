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

    int sample_rate;
    int m_width;
    int m_height;


};


#endif //ENCODE_SAVEFRAMETOOL_H
