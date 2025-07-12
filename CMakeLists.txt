cmake_minimum_required(VERSION 3.26)
project(Encode)

set(CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_SOURCE_DIR}/opensource/ffmpeg/include)
include_directories(src)

link_directories(${CMAKE_SOURCE_DIR}/opensource/ffmpeg/lib)

add_executable(Encode
        src/DecodeFile.cpp
        src/DecodeFile.h
        src/utils/SafeQueue.h)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
target_link_libraries(
        Encode
        avformat  # 对应avformat.lib
        avcodec   # 对应avcodec.lib
        avutil    # 对应avutil.lib
        avfilter  # 对应avfilter.lib（按需添加）
        swscale   # 对应swscale.lib（按需添加）
)
