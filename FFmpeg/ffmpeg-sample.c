#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

#define false 0
#define true 1

void save_frame(AVFrame *frame, int width, int height, int i_frame)
{
	FILE *file;
	char sz_filename[32];
	int  y;

	/* Open file */
	sprintf(sz_filename, "frame%d.ppm", i_frame);
	file = fopen(sz_filename, "wb");
	if (file==NULL) {
		printf("Cant open file to save frame\n");
		return;
	}

	/* Write header */
	fprintf(file, "P6\n%d %d\n255\n", width, height);

	/* Write pixel data */
	for(y=0;  y<height;  y++)
		fwrite(frame->data[0] + y * frame->linesize[0], 1, width * 3, file);

	/* Close file */
	fclose(file);
}

struct video {
	AVFormatContext *fmt_ctx;
	AVCodecParserContext *parser;
	AVCodecContext *codec;
	AVPacket *pkt;
	int stream_index;

	//struct buffer_pool pool;
};

#if 0
static struct video *video_open(const char *filename)
{
	AVCodec *codec = NULL;
	AVStream *stream;
	int r;
	char buf[4096] = {};
	struct video *s = malloc(sizeof(struct video));

	av_register_all();

	r = avformat_open_input(&s->fmt_ctx, filename, NULL, NULL);
	if (r < 0) {
		printf("Error opening input\n");
		goto err;
	}

	r = avformat_find_stream_info(s->fmt_ctx, NULL);
	if (r < 0) {
		printf("Error opening stream info\n");
		goto err;
	}

	r = av_find_best_stream(s->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
	if (r < 0) {
		printf("Error opening best stream\n");
		goto err;
	}

	s->stream_index = r;
	stream = s->fmt_ctx->streams[r];
	if (!stream || !stream->codecpar){
		printf("Error no stream or par\n");
		goto err;
	}

	s->codec = avcodec_alloc_context3(codec);
	if (!s->codec) {
		printf("Error context\n");
		goto err;
	}
#if 0
	s->codec->get_buffer2 = video_get_buffer2;
	s->pool.display = display;
	s->codec->opaque = &s->pool;
#endif

	r = avcodec_parameters_to_context(s->codec, stream->codecpar);
	if (r < 0) {
		printf("Error param to context\n");
		goto err;
	}

	r = avcodec_open2(s->codec, codec, NULL);
	if (r < 0) {
		printf("Error codec open\n");
		goto err;
	}

	s->parser = av_parser_init(codec->id);
	if (!s->parser) {
		printf("Error parser\n");
		goto err;
	}

	avcodec_string(buf, sizeof(buf), s->codec, false);
	buf[sizeof(buf)-1] = '\0';
	puts(buf);
	s->pkt = av_packet_alloc();
	return s;

err:
	free(s);
	return NULL;
}
#endif

void usage()
{
	printf("\n Usages:\n");
	printf("./sample_player <file_name>\n");
}

int main(int argc, char *argv[])
{
	AVCodec *codec = NULL;
	AVStream *stream = NULL;
	AVFormatContext *fmtctx = NULL;
	AVCodecContext *codec_ctx = NULL, *codec_ctx_org = NULL;
	AVCodecParameters *origin_par = NULL;
	AVFrame *frame, *frame_rgb;
	AVPacket packet= {0, };
	struct video *s = NULL;
	AVCodec* current_codec = NULL;
	const char *filename;
	struct SwsContext *sws_ctx;
	int i, ret, nbytes, frame_finished;
	uint8_t *buffer;

	if (argc < 2 || !argv || !argv[1]) {
		usage();
		return -1;
	}

	filename = argv[1];
	av_register_all();
	avcodec_register_all();

	ret = avformat_open_input(&fmtctx, filename, NULL, NULL);
	if (ret) {
		printf("open input error");
		return -1;
	}

	ret = avformat_find_stream_info(fmtctx, NULL);
	if (ret < 0) {
		printf("Cant find stream info\n");
		goto err_find;
	}

	av_dump_format(fmtctx, 0, filename, 0);

#if 0
    while ((current_codec = av_codec_next(current_codec)) != NULL) {
		printf("Found codec: %s\n", current_codec->name);
		if(strncmp(current_codec->name, "h264_vaapi", strlen("h264_vaapi")))
			break;
    }

	for (i = 0; i < fmtctx->nb_streams; i++) {
        	if (fmtctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO
				&& !stream)
				stream = fmtctx->streams[i];
		else
			fmtctx->streams[i]->discard = AVDISCARD_ALL;
	}

	
	i = 0;
	if (av_codec_get_tag2(fmtctx->oformat->codec_tag, origin_par->codec_id, &i) == 0)
		printf("could not find codec tag, default to 0.\n");

	origin_par->codec_tag = i;
	codec = avcodec_find_decoder(origin_par->codec_id);


	origin_par = stream->codecpar;
	if (!origin_par) {
		printf("Can't find video stream parameters\n");
		ret = -1;
		goto error_no_codec_ctx;
	}

	if (avcodec_parameters_to_context(codec_ctx, origin_par)) {
		printf("Can't copy decoder context\n");
		ret = -1;
		goto error_no_codec_ctx;
	}
	
	s = video_open(filename);
	if (!s) {
		printf("Video open failed");
		return -1;
	}	
#endif

	/* Find the video stream and identify the codec */
	i = av_find_best_stream(fmtctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (i < 0) {
		printf("Can't find video stream in input file\n");
		ret = -1;
		goto error_no_codec_ctx;
	}

	stream = fmtctx->streams[i];
	if (!stream) {
		ret = -1;
		printf("Cant find stream\n");
		goto error_no_stream;
	}

	codec_ctx_org = stream->codec;
	codec = avcodec_find_decoder(codec_ctx_org->codec_id);
	if (codec == NULL) {
		printf("Unsupported codec!\n");
		ret = -1;
		goto error_no_decoder;
	}

	// Copy context
	codec_ctx = avcodec_alloc_context3(codec);
	if (!codec_ctx) {
		printf("Couldn't create codec context\n");
		ret = -1;
		goto error_no_codec_ctx;
	}

	if (avcodec_copy_context(codec_ctx, codec_ctx_org) != 0) {
		printf("Couldn't copy codec context\n");
		ret = -1;
		goto error_no_copy;
	}

	// Open codec
	if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
		printf("Couldn't open codec \n");
		ret = -1;
		goto error_no_codec;
	}

	// Allocate video frame
	frame = av_frame_alloc();
	if (!frame) {
		printf("OOM frame \n");
		ret = -1;
		goto error_OOM;
	}

	// Allocate another video frame for RGB
	frame_rgb = av_frame_alloc();
	if (!frame_rgb) {
		printf("OOM frame 2\n");
		ret = -1;
		goto error_free_pframe;
	}

	// Determine required buffer size and allocate buffer
	nbytes = avpicture_get_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height);
	buffer = (uint8_t *) av_malloc(nbytes * sizeof(uint8_t));
	if (!buffer) {
		printf("OOM frame 2\n");
		ret = -1;
		goto error_free_pframe_rgb;
	}

	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
			codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL,
			NULL, NULL);
	if (!sws_ctx) {
		printf("No SW scaling ctx\n");
		ret = -1;
		goto error_no_sws;
	}

	while (av_read_frame(fmtctx, &packet) >= 0) {

		/* Is this a packet from the video stream? */
		if (packet.stream_index != stream->index) 
			continue;

		/* Decode video frame */
		avcodec_decode_video2(codec_ctx, frame, &frame_finished, &packet);

		/* Did we get a video frame? */
		if (frame_finished) {
			/* Convert the image from its native format to RGB */
			sws_scale(sws_ctx, (uint8_t const * const *)frame->data,
					frame->linesize, 0, codec_ctx->height,
					frame_rgb->data, frame_rgb->linesize);

			/* Save the frame to disk */
			if (++i <= 5)
				save_frame(frame_rgb, codec_ctx->width, codec_ctx->height, i);
		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

error_no_sws:
	av_free(buffer);
error_free_pframe_rgb:
	av_frame_free(&frame_rgb);
error_free_pframe:
	av_frame_free(&frame);
error_OOM:
	avcodec_close(codec_ctx);
	avcodec_close(codec_ctx_org);
error_no_codec:
error_no_copy:
	avcodec_free_context(&codec_ctx);
error_no_codec_ctx:
error_no_stream:
error_no_decoder:
err_find:
	printf("Closing input\n");
	avformat_close_input(&fmtctx);
	return ret;
}
