#include "DecodeFile.h"
#include "EncodeFile.h"
int main()
{
    cout << "hello world!" << endl;
    auto* video_queue = new SafeQueue<AVFrame*>();
    auto* audio_queue = new SafeQueue<AVFrame*>();
    auto* decodeFile = new DecodeFile("C:\\Users\\XztCloud\\Videos\\test.mp4", video_queue, audio_queue);
    bool ret = decodeFile->InitDeCode();
    cout << "InitDecode ret is " << ret << endl;
    if (!ret) {
        return -1;
    }
    EncoderParams params;
    decodeFile->GetVideoInfo(params.width, params.height, params.fps, params.pix_fmt, params.video_codec_id);
    decodeFile->GetAudioInfo(params.sample_rate, params.channels, params.audio_bit_rate, params.audio_codec_id);
    decodeFile->StartDecode();

    auto* encodeFile = new EncodeFile("./output.mp4", params, audio_queue, video_queue);
    if (!encodeFile->InitFormat()) {
        fprintf(stderr, "init format failed.");
        return -1;
    }
    encodeFile->StartEncode();

    delete decodeFile;
    delete encodeFile;
    delete audio_queue;
    delete video_queue;
    int num;
    cin >> num;


    return 0;
}