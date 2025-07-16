#pragma once
#include <iostream>
#include "utils/SafeQueue.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

void SaveFrameToPNG(AVFrame* frame);