#include "Format.h"

SDL_Renderer *renderer = nullptr;
SDL_Texture *texture = nullptr;
SDL_Rect rect;
SDL_Window *sdl_window = nullptr;
SDL_AudioSpec audio_spec;
int64_t  in_channel_layout;
SwrContext  *au_convert_ctx = nullptr;
uint8_t * audio_buffer;
static  Uint32  audio_len;
static  Uint8  *audio_pos;
static Uint8 * audio_chunk;
AVRational  *time_base;

SwsContext * swsctx = nullptr;
uint8_t * buffer;
AVFrame *frame_yuv = av_frame_alloc();
SDL_Event *sdlEvent = new SDL_Event;
int audio_buffer_size;

Format::Format(const char* filename)
{
	this->filename = filename;
}

int init_codec(AVCodecContext *codec_ctx, AVFormatContext *fmt_ctx, AVCodec **codec, int stream_idx)
{
	static int ret;
	if ((ret = avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[stream_idx]->codecpar)) <0)
	{
		cout << "Can`t get codec parameters" << endl;
		return -1;
	}
	if ((*codec = avcodec_find_decoder(codec_ctx->codec_id)) == NULL)
	{
		cout << "Can`t find coder" << endl;
		return -1;
	}
	if (avcodec_open2(codec_ctx, *codec, NULL) < 0)
	{
		cout << "Err cant open deCodec" << endl;
		return -1;
	}
	return ret;
}

int init_SDLwindow(AVCodecContext *codec_ctx)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Could`t Initialize SDL - %s\n", SDL_GetError());
		exit(0);
	}

	SDL_setenv(SDL_HINT_RENDER_SCALE_QUALITY, "linear", 0); //linear: Smooth picture quality

	sdl_window = SDL_CreateWindow(__FILE_PATH__, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, codec_ctx->width, codec_ctx->height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

	rect.x = 0;
	rect.y = 0;
	rect.w = codec_ctx->width;
	rect.h = codec_ctx->height;

	renderer = SDL_CreateRenderer(sdl_window, -1, 0);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, codec_ctx->width, codec_ctx->height);



	buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1));

	av_image_fill_arrays(frame_yuv->data, frame_yuv->linesize, buffer, AV_PIX_FMT_YUV420P, codec_ctx->width, codec_ctx->height, 1);
	//Resize the image
	swsctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	return 0;
}


int play_Frame_YUV(AVCodecContext *codec_ctx, AVFrame *frame)
{
	//printf(
	//	"Frame %c (%d) pts %d dts %d key_frame %d [coded_picture_number %d, display_picture_number %d] \n",
	//	av_get_picture_type_char(frame->pict_type), //I 关键帧 P 前向预测帧  B 前后预测帧
	//	codec_ctx->frame_number,//解码的帧数编号
	//	frame->pts,// 帧数解码时间戳
	//	frame->pkt_dts,
	//	frame->key_frame,// 关键帧 为 1 其他为0
	//	frame->coded_picture_number, // 比特流的图片编号
	//	frame->display_picture_number //显示中的图片编号
	//);
	sws_scale(swsctx, (const unsigned char* const*)frame->data, frame->linesize, 0, codec_ctx->height, frame_yuv->data, frame_yuv->linesize);

	SDL_UpdateYUVTexture(texture, &rect,
		frame_yuv->data[0], frame_yuv->linesize[0],
		frame_yuv->data[1], frame_yuv->linesize[1],
		frame_yuv->data[2], frame_yuv->linesize[2]);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
	SDL_Delay(24);
	SDL_PollEvent(sdlEvent);
	switch (sdlEvent->type)
	{
	case SDL_QUIT:
		return -1;
	case SDL_KEYDOWN:
		switch (sdlEvent->key.keysym.sym)
		{
		case SDLK_q:
			cout << "You pressed Q and quit" << endl;
			return -1;
		case SDLK_r:
			SDL_SetWindowSize(sdl_window, 1280, 720);
			cout << "Reset windows size" << endl;
		}
	case SDL_MOUSEBUTTONDOWN:
		switch (sdlEvent->button.button)
		{
		case SDL_BUTTON_LEFT:
			cout << "You pressed the left mouse button" << endl;

			SDL_DisplayMode  display_mode;
			SDL_GetWindowDisplayMode(sdl_window, &display_mode); //Get the current window property

			if ((display_mode.w >= 1920))
			{
				SDL_SetWindowFullscreen(sdl_window, 0);
				display_mode.w = 1280;
				display_mode.h = 720;
				SDL_SetWindowDisplayMode(sdl_window, &display_mode);

			}
			else
			{
				//SDL_WINDOW_FULLSCREEN, for "real" fullscreen with a videomode change; 
				//SDL_WINDOW_FULLSCREEN_DESKTOP for "fake" fullscreen that takes the size of the desktop;
				//and 0 for windowed mode.
				SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			}
		}
		break;
	}
	return 0;
}

int play_frame_sound(AVFrame *frame, int ret)
{
	audio_chunk = (Uint8 *)audio_buffer;
	audio_len = (Uint32)((ret * 4));
	audio_pos = audio_chunk;
	while (audio_len >1)
	{
		SDL_Delay(1);
	}
	return ret;
}

void fill_audio_callback(void *udata, Uint8 *stream, int len) {
	SDL_memset(stream, 0, len);
	if (audio_len == 0)
	{
		return;
	}
	len = (len > audio_len ? audio_len : len);

	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	audio_pos += len;
	audio_len -= len;
}


int init_audio(AVCodecContext * codec_ctx)
{
	audio_buffer_size = av_samples_get_buffer_size(0, codec_ctx->channels, codec_ctx->frame_size, codec_ctx->sample_fmt, 0);
	audio_buffer = new uint8_t[audio_buffer_size];

	audio_spec.freq = 48000;
	audio_spec.format = AUDIO_S16SYS;
	audio_spec.channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
	audio_spec.silence = 0;
	audio_spec.samples = codec_ctx->frame_size;
	audio_spec.callback = fill_audio_callback;
	audio_spec.userdata = codec_ctx;

	if (SDL_OpenAudio(&audio_spec, NULL)<0) {
		printf("can't open audio.\n");
		return -1;
	}

	in_channel_layout = av_get_default_channel_layout(codec_ctx->channels);
	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 48000, in_channel_layout, codec_ctx->sample_fmt, codec_ctx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);

	SDL_PauseAudio(0);

	return 0;
}

 int decode1(AVFormatContext * fmt_ctx, AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt, Format *fmt)
{
	int ret;
	int64_t pts;
	ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error sending a packet for decoding\n");
		exit(1);
	}
	while (ret >= 0) {
		ret = avcodec_receive_frame(dec_ctx, frame);

		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if (ret < 0) {
			fprintf(stderr, "Error during decoding\n");
			exit(1);
		}

		if (pkt->stream_index == fmt->audio_idx)
		{
			int ret = swr_convert(au_convert_ctx, &audio_buffer, 192000, (const uint8_t**)frame->data, frame->nb_samples);
			if (ret < 0)
			{
				cout << "小于0" << ret << endl;
				continue;
			}
			play_frame_sound(frame, ret);
		}
	}

	return 0;
}

int decode2(AVFormatContext * fmt_ctx, AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt, Format *fmt)
{
	int ret;
	int64_t pts;
	ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error sending a packet for decoding\n");
		exit(1);
	}
	while (ret >= 0) {
		ret = avcodec_receive_frame(dec_ctx, frame);

		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if (ret < 0) {
			fprintf(stderr, "Error during decoding\n");
			exit(1);
		}

		if (pkt->stream_index == fmt->video_idx)
		{
			if (play_Frame_YUV(dec_ctx, frame) == -1)
			{
				return -2;
			}
		}
	}
	return 0;
}

Format::~Format()
{
}
