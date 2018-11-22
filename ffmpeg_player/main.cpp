#include "Format.h"
#include "Streams.h"
#include "Packets.h"
#undef main

int init_codec(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx, AVCodec **codec, int stream_idx);
int decode1(AVFormatContext * fmt_ctx, AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt, Format *fmt);
int decode2(AVFormatContext * fmt_ctx, AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt, Format *fmt);
int init_SDLwindow(AVCodecContext *codec_ctx);
int init_audio(AVCodecContext * codec_ctx);

extern AVFrame *frame_yuv;
extern SDL_Window *sdl_window;
extern SDL_Event *sdlEvent;
HANDLE SEM;

DWORD WINAPI thread_main3(LPVOID format) 
{
	Format *fmt = (Format*)format;
	int ret = 0;
	//Sleep(50);
	while (1)
	{
		if (fmt->v_pkts.empty())
			continue;

		WaitForSingleObject(SEM, INFINITE);

		ret = decode2(fmt->fmt_ctx, fmt->vcodec_ctx, fmt->frame, fmt->v_pkts.front(), fmt);

		if (ret < 0)
		{
			cout << "decode ERR" << endl;
			fmt->play_status = -3;
			return -1;
		}
		fmt->v_pkts.pop();
		ReleaseSemaphore(SEM, 1, NULL);
	}

	return 0;
}

DWORD WINAPI thread_main2(LPVOID format)
{
	Format *fmt = (Format*)format;
	int ret = 0;
	//Sleep(500);
	while (1)
	{
		if (fmt->play_status == -3)
		{
			cout << "fmt->play_status  = " << fmt->play_status << endl;
			exit(1);
		}
		if (fmt->a_pkts.empty())
			continue;

		WaitForSingleObject(SEM, INFINITE);

		ret = decode1(fmt->fmt_ctx, fmt->acodec_ctx, fmt->frame, fmt->a_pkts.front(), fmt);

		if (ret < 0)
		{
			cout << "decode ERR" << endl;
			return -1;
		}

		fmt->a_pkts.pop();
		ReleaseSemaphore(SEM, 1, NULL);
	}
	return 0;
}

DWORD WINAPI thread_main1(LPVOID format)
{
	av_register_all();
	Format *fmt = (Format*)format;
	AVPacket *atem_pkt = nullptr ,*vtem_pkt = nullptr;
	HANDLE thread3;
	fmt->fmt_ctx = nullptr;
	 int i = 0;

	int ret = avformat_open_input(&fmt->fmt_ctx, fmt->filename, nullptr, nullptr);
	if (ret != 0)
	{
		cout << "Can`opne the fine" << endl;
		return -1;
	}
	if (ret = avformat_find_stream_info(fmt->fmt_ctx, NULL))
	{
		cout << "Can`find streams" << endl;
		return-1;
	}
	double time_base;
	for (unsigned int i = 0; i < fmt->fmt_ctx->nb_streams; ++i)
	{
		switch (fmt->fmt_ctx->streams[i]->codecpar->codec_type)
		{
		case AVMEDIA_TYPE_VIDEO:
			fmt->video_idx = i;
			fmt->time_base =  fmt->fmt_ctx->streams[i]->time_base;
			time_base = av_q2d(fmt->time_base);
			cout << "Video Time base" << time_base << endl;

			break;
		case AVMEDIA_TYPE_AUDIO:
			fmt->audio_idx = i;
			fmt->time_base = fmt->fmt_ctx->streams[i]->time_base;
			time_base = av_q2d(fmt->time_base);
			cout << " Audio Time base" << time_base << endl;
			break;
		default:
			break;
		}
	}
	av_dump_format(fmt->fmt_ctx, 0, fmt->filename, 0);
	fmt->acodec_ctx = avcodec_alloc_context3(NULL);
	fmt->vcodec_ctx = avcodec_alloc_context3(NULL);

	fmt->pkt = av_packet_alloc();
	atem_pkt = av_packet_alloc();
	vtem_pkt = av_packet_alloc();
	av_init_packet(fmt->pkt);

	if (!fmt->pkt)
	{
		cout << "packet init failuer" << endl;
		goto end;
	}
	if ((fmt->frame = av_frame_alloc()) == NULL)
	{
		cout << "frame alloc failuer" << endl;
		goto end;
	}
	init_codec(fmt->vcodec_ctx,fmt->fmt_ctx,&fmt->vcodec,fmt->video_idx);
	init_codec(fmt->acodec_ctx, fmt->fmt_ctx, &fmt->acodec, fmt->audio_idx); 
	init_SDLwindow(fmt->vcodec_ctx);
	init_audio(fmt->acodec_ctx);

	while (1)
	{
		if (ret = av_read_frame(fmt->fmt_ctx, fmt->pkt) < 0)
		{
			cout << "read err OR  end of file " << endl;
			Sleep(100*1000);
			goto end;
		}
		
		if (fmt->pkt->stream_index == fmt->video_idx)
		{
			WaitForSingleObject(SEM, INFINITE);
			if (av_packet_ref(vtem_pkt, fmt->pkt))
			{
				cout << "¿ËÂ¡Ê§°Ü" << endl;
				continue;
			}
			fmt->v_pkts.push(vtem_pkt);
			ReleaseSemaphore(SEM, 1, NULL);
		}

		if ((fmt->pkt->stream_index == fmt->audio_idx))
		{
			WaitForSingleObject(SEM, INFINITE);
			if (av_packet_ref(atem_pkt, fmt->pkt))
			{
				cout << "¿ËÂ¡Ê§°Ü" << endl;
				continue;
			}
			fmt->a_pkts.push(atem_pkt);
			ReleaseSemaphore(SEM, 1, NULL);
		}
		av_packet_unref(fmt->pkt);
	}

end:
	if (fmt->pkt)
		av_packet_free(&fmt->pkt);
	if (fmt->frame)
		av_frame_free(&fmt->frame);
	if (fmt->acodec_ctx)
		avcodec_free_context(&fmt->acodec_ctx);
	if(fmt->vcodec_ctx)
		avcodec_free_context(&fmt->vcodec_ctx);
	if (fmt->fmt_ctx)
		avformat_free_context(fmt->fmt_ctx);
	if (frame_yuv)
		av_frame_free(&frame_yuv);
	if (atem_pkt)
		av_packet_free(&atem_pkt);
	if(vtem_pkt)
		av_packet_free(&vtem_pkt);
	return 0;
}

int main() 
{
	Format format(__FILE_PATH__);
	Streams *streams;
	format.play_status = -1;
	HANDLE thread1,thread2, thread3;

	SEM = CreateSemaphore(NULL, 1, 1, NULL); 

	thread1 = CreateThread(0, 0, thread_main1, (LPVOID)&format, 0, 0);
	thread2 = CreateThread(0, 0, thread_main2, &format, 0, 0);
	thread3 = CreateThread(0, 0, thread_main3, &format, 0, 0);

	WaitForSingleObject(thread1, INFINITE);
	WaitForSingleObject(thread2, INFINITE);
	WaitForSingleObject(thread3, INFINITE);
	return 0;
}