#pragma once
#pragma warning(disable:4819) // 屏蔽 "该文件包含不能在当前代码页(936)中表示的字符"
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <sstream>
#include <string>
#include <queue>
#include <memory>

extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h> 
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h> 
#include <SDL2/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <sys/types.h>

#include <signal.h>
#else
#include <unistd.h>
#endif
}

#define __FILE_PATH__ "C:/Users/zhang/Documents/Tencent Files/849162565/FileRecv/楚乔传第一集.mp4"
using namespace std;