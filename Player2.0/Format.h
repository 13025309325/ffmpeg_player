#pragma once
#include "common.h"
class Format
{
public:
	Format(const char* filename);

	~Format();
public:
	AVFormatContext *fmt_ctx;
	AVCodecContext *acodec_ctx,*vcodec_ctx;
	AVCodec *acodec,*vcodec;
	AVFrame * aframe, *vframe,*frame;
	AVPacket *pkt;
	AVRational time_base;
	queue<AVPacket *> a_pkts;
	queue<AVPacket *> v_pkts;
	int audio_idx, video_idx;
	int play_status;
	const char *filename;
};

