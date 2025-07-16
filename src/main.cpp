#include "DecodeFile.h"
int main()
{
    cout << "hello world!" << endl;
    SafeQueue<AVFrame*>* video_queue = new SafeQueue<AVFrame*>();
    SafeQueue<AVFrame*>* audio_queue = new SafeQueue<AVFrame*>();
    auto* decodeFile = new DecodeFile("F:\\project_codec\\Encode\\13389104780110970.mp4", video_queue, audio_queue);
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