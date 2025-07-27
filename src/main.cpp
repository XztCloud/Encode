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
    decodeFile->StartDecode();

    auto* encodeFile = new EncodeFile("./output.mp4", 1920, 1080, 24, audio_queue, video_queue);
    if (!encodeFile->InitFormat()) {
        fprintf(stderr, "init format failed.");
        return -1;
    }
    encodeFile->StartEncode();
    int num;
    cin >> num;
    return 0;
}