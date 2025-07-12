#include "DecodeFile.h"


bool DecodeFile::InitDeCode() {
    if (avformat_open_input(&format_ctx, file_path.c_str(), nullptr, nullptr) < 0) {
        std:cerr << "cloud not open file:" << file_path << std::endl;
        return false;
    }
    return true;
}

int main()
{
    cout << "hello world!" << endl;
    int num;
    cin >> num;
    return 0;
}
