/*
 * AVRecord.cpp
 *
 *  Created on: 2015-10-13
 *      Author: lifeng
 */



#include <stdio.h>
#include <string.h>
#include "AVRecord.h"
//#include "utils/queue.h"
//#include "utils/ReadFrame.h"
#include "libavutil/mathematics.h"

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavformat/avformat.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#ifdef __cplusplus
};
#endif
#endif


/*
FIX: H.264 in some container format (FLV, MP4, MKV etc.) need
"h264_mp4toannexb" bitstream filter (BSF)
  *Add SPS,PPS in front of IDR frame
  *Add start code ("0,0,0,1") in front of NALU
H.264 in some container (MPEG2TS) don't need this BSF.
*/
//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 1

/*
FIX:AAC in some container format (FLV, MP4, MKV etc.) need
"aac_adtstoasc" bitstream filter (BSF)
*/
//'1': Use AAC Bitstream Filter
#define USE_AACBSF 1
#define OPTIMIZE 1
#define OPT_DEBUG 0
#define DUMMY_PTS 0



void AVRecorder::error_process() {
	avformat_close_input(&ifmt_ctx_v);
	avformat_close_input(&ifmt_ctx_a);
	/* close output */
	AVOutputFormat* ofmt = ofmt_ctx->oformat;
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	printf("eeeeeeeeeeeeeeeeeeeeeeeeeeee\n");

}


AVRecorder::AVRecorder() {
	ifmt_ctx_a = NULL;
	ifmt_ctx_v = NULL;
	ofmt_ctx = NULL;
	in_stream = NULL;
	out_stream = NULL;
	video_dump = "video_dump.h264";
	audio_dump = "audio_dump.aac";
	output = "output.mp4";
	video_dump_packets = 0;
	audio_dump_packets = 0;
	fp_dump_a = NULL;
	fp_dump_v = NULL;
	packet_cache = NULL;
	cached_packets = 0;
	cached_consumed = 0;
	aacbsfc = NULL;
	h264bsfc = NULL;

}

AVRecorder::~AVRecorder() {

}
int AVRecorder::dump_file(uint8_t *frame_data, uint32_t frame_size, uint8_t frame_type, const char *dump_file) {

	if (strstr(dump_file, "video") != NULL && !fp_dump_v) {
		fp_dump_v = fopen(video_dump, "wb");
		if (!fp_dump_v) return -1;
		//		queue = InitQueue();
	}
	else if (strstr(dump_file, "audio") != NULL && !fp_dump_a) {
		fp_dump_a = fopen(audio_dump, "wb");
		if (!fp_dump_a) return -1;
	}
	//dump xx 个音频包
	if (frame_type == 0 && audio_dump_packets < AUDIO_DUMP_PACKETS) {
		fwrite(frame_data, 1, frame_size, fp_dump_a);
		audio_dump_packets++;
		if (audio_dump_packets >= AUDIO_DUMP_PACKETS) {
			fflush(fp_dump_a);
			fclose(fp_dump_a);

			return 0;
		}
	}
	//dump xx 个视频包
	else if (video_dump_packets < VIDEO_DUMP_PACKETS /*&& flag == 1*/){
		fwrite(frame_data, 1, frame_size, fp_dump_v);
		video_dump_packets++;
		if (video_dump_packets >= VIDEO_DUMP_PACKETS) {
			fflush(fp_dump_v);
			fclose(fp_dump_v);

			return 0;
		}
	}

	return -1;
}
int AVRecorder::cache_packets(uint8_t *frame_data, uint32_t frame_size, uint64_t pts, uint64_t dts, uint8_t frame_type, int key_frame) {
	if (packet_cache == NULL)
		packet_cache = new AVPacket[COEFF*AUDIO_DUMP_PACKETS*VIDEO_DUMP_PACKETS*sizeof(AVPacket)];
	if (audio_dump_packets < AUDIO_DUMP_PACKETS || video_dump_packets < VIDEO_DUMP_PACKETS) {
		uint8_t *data = new uint8_t[frame_size];
		memcpy(data, frame_data, frame_size);
		packet_cache[cached_packets].data = data;
		packet_cache[cached_packets].size = frame_size;
		packet_cache[cached_packets].pts = pts;
		packet_cache[cached_packets].dts = dts;
		if (key_frame == 1) { //case i frame
			packet_cache[cached_packets].flags |= AV_PKT_FLAG_KEY;
		}
		packet_cache[cached_packets++].stream_index = frame_type;
		return -1;
	}
	else {
		return 0;
	}
}


int AVRecorder::open_input_file(AVFormatContext **ifmt_ctx, const char *input_file) {
	int ret;

	if (*ifmt_ctx == NULL) {
		if ((ret = avformat_open_input(ifmt_ctx, input_file, 0, 0)) < 0) {
			printf( "Could not open input file.");
			error_process();
			return -1;

		}
		if ((ret = avformat_find_stream_info(*ifmt_ctx, 0)) < 0) {
			printf( "Failed to retrieve input stream information");
			error_process();
			return -1;

		}
		printf("===========Input Information==========\n");
		av_dump_format(*ifmt_ctx, 0, input_file, 0);
		printf("======================================\n");
	}
	return 0;
}


int AVRecorder::open_output_file(int *v_indx_in, int *a_indx_in, int *v_indx_out, int *a_indx_out) {
	AVOutputFormat *ofmt;
	if (ofmt_ctx == NULL) {
		int ret;
		//Output
		avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, output);
		if (!ofmt_ctx) {
			printf( "Could not create output context\n");
			ret = AVERROR_UNKNOWN;
			error_process();
			return -1;

		}
		ofmt = ofmt_ctx->oformat;

		for (int i = 0; i < ifmt_ctx_v->nb_streams; i++) {
			//Create output AVStream according to input AVStream
			if(ifmt_ctx_v->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
				AVStream *in_stream = ifmt_ctx_v->streams[i];
				AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
				*v_indx_in=i;
				if (!out_stream) {
					printf( "Failed allocating output stream\n");
					ret = AVERROR_UNKNOWN;
					error_process();
					return -1;

				}

				*v_indx_out=out_stream->index;
				//Copy the settings of AVCodecContext
				if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
					printf( "Failed to copy context from input to output stream codec context\n");
					error_process();
					return -1;

				}

				out_stream->codec->codec_tag = 0;
				if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
					out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

				break;

			}
		}

		for (int i = 0; i < ifmt_ctx_a->nb_streams; i++) {
			//Create output AVStream according to input AVStream
			if(ifmt_ctx_a->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
				AVStream *in_stream = ifmt_ctx_a->streams[i];
				AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
				*a_indx_in=i;

				if (!out_stream) {
					printf( "Failed allocating output stream\n");
					ret = AVERROR_UNKNOWN;
					error_process();
					return -1;

				}
				*a_indx_out=out_stream->index;
				//Copy the settings of AVCodecContext
				if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
					printf( "Failed to copy context from input to output stream codec context\n");
					error_process();
					return -1;

				}
				out_stream->codec->codec_tag = 0;
				if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
					out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;

				break;
			}
		}

		printf("==========Output Information==========\n");
		av_dump_format(ofmt_ctx, 0, output, 1);
		printf("======================================\n");
		//Open output file
		if (!(ofmt->flags & AVFMT_NOFILE)) {
			if (avio_open(&ofmt_ctx->pb, output, AVIO_FLAG_WRITE) < 0) {
				printf( "Could not open output file '%s'", output);
				error_process();
				return -1;
			}
		}
		//Write file header
		if (avformat_write_header(ofmt_ctx, NULL) < 0) {
			printf( "Error occurred when opening output file\n");
			error_process();
			return -1;
		}
		//FIX
	#if USE_H264BSF
		h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb");
	#endif
	#if USE_AACBSF
		aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
	#endif
	}
	return 0;
}

int AVRecorder::flush_cached_packets(int v_indx_in, int a_indx_in, int v_indx_out, int a_indx_out, int *indx_out) {

	AVPacket *pkt;
	AVFormatContext *ifmt_ctx = NULL;
	AVStream *in_stream, *out_stream;
	/*** 将队列中缓存的音视频包先行mux，然后再处理当前的pFrameBuffer ***/
	if (cached_consumed == 0) {
		for (int i=0; i<cached_packets; i++) {
			pkt = &(packet_cache[i]);
			if (pkt->stream_index == 0) {	//audio

				ifmt_ctx=ifmt_ctx_a;
				*indx_out=a_indx_out;
				in_stream  = ifmt_ctx->streams[a_indx_in];
			}
			else {		//=1:video

				ifmt_ctx=ifmt_ctx_v;
				*indx_out=v_indx_out;
				in_stream  = ifmt_ctx->streams[v_indx_in];
			}

			out_stream = ofmt_ctx->streams[*indx_out];

			pkt->dts = pkt->pts;
			AVRational time_base1=in_stream->time_base;

			int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
			pkt->duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			//Convert PTS/DTS
			AVRational ar = {1, 10000};
			pkt->pts = av_rescale_q_rnd(pkt->pts, ar,out_stream->time_base,(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
			pkt->dts = av_rescale_q_rnd(pkt->dts, ar,out_stream->time_base,(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
//			ptr->duration = av_rescale_q_rnd(ptr->duration, ar,out_stream->time_base,	(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

			//FIX:Bitstream Filter
#if USE_H264BSF
			av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &(pkt->data), &(pkt->size), pkt->data, pkt->size, 0);
#endif
#if USE_AACBSF
			//		av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt1.data, &pkt1.size, pkt1.data, pkt1.size, 0);
			av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &(pkt->data), &(pkt->size), pkt->data, pkt->size, 0);
#endif

			pkt->pos = -1;
			pkt->stream_index=*indx_out;

			//Write
			if (av_interleaved_write_frame(ofmt_ctx, pkt) < 0) {
				printf( "Error muxing packet\n");
				return -1;
			}
//			free(pkt.data);
		}
//		free(packet_cache);
		cached_consumed = 1;
	}
	return 0;
}

/*** lifeng:
 * @parm ofile: output file format, the supported format are:
 * mp4, ts, mov, avi, mkv,wmv, asf
 */
int AVRecorder::prepare(const char *ofile) {
	output = ofile;
	return 0;
}

/****
 * lifeng:
 * write a video / audio frame into a output file
 * @parm frame_data encoded av data
 * @parm frame_size a/v frame size
 * @parm pts pts value
 * @parm dts dts value
 * @parm frame_type 0:audio 1:video
 * @parm flag 0: non-i frame video or audio, 1: i-frame video, 2: means stop the recording
 * @note
 */
int AVRecorder::record(uint8_t *frame_data, uint32_t frame_size, uint64_t  pts, uint64_t dts, uint8_t frame_type, uint8_t flag) {

	AVFormatContext *ifmt_ctx;
	AVPacket pkt;
	static int v_indx_in=-1,v_indx_out=-1;
	static int a_indx_in=-1,a_indx_out=-1;
	int indx_out=0;
	AVStream *in_stream = NULL, *out_stream = NULL;
//	static Queue *queue = NULL;
	if (flag == 2)
		done();
	//initiallize some context things
	if (frame_type == 1)
		dump_file(frame_data, frame_size, frame_type, video_dump);
	else
		dump_file(frame_data, frame_size, frame_type, audio_dump);
	if (cache_packets(frame_data, frame_size, pts, dts, frame_type, flag) < 0) {
		return -1;
	}
	if (open_input_file(&ifmt_ctx_v, video_dump) < 0) {
		return -1;
	}
	if (open_input_file(&ifmt_ctx_a, audio_dump) < 0) {
		return -1;
	}
	if (open_output_file(&v_indx_in, &a_indx_in, &v_indx_out, &a_indx_out)) {
		return -1;
	}
	if (flush_cached_packets(v_indx_in, a_indx_in, v_indx_out, a_indx_out, &indx_out) < 0)
		return -1;

	//Get an AVPacket
	if (frame_type == 1) {
		ifmt_ctx=ifmt_ctx_v;
		indx_out=v_indx_out;
		in_stream  = ifmt_ctx->streams[v_indx_in];

	}
	else if (frame_type == 0){
		ifmt_ctx=ifmt_ctx_a;
		indx_out=a_indx_out;
		in_stream  = ifmt_ctx->streams[a_indx_in];
	}
	out_stream = ofmt_ctx->streams[indx_out];
	av_init_packet(&pkt);

	pkt.data = frame_data;	//lifeng:hacking pkt.data by replacing with other source
	pkt.size = frame_size;

	pkt.pts = pts;
//	AVRational time_base1=in_stream->time_base;
//	int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_stream->r_frame_rate);
//	pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);

	//Convert PTS/DTS
	pkt.dts = pkt.pts;
	AVRational ar = {1, 1000};
	pkt.pts = av_rescale_q_rnd(pkt.pts, ar, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
	pkt.dts = av_rescale_q_rnd(pkt.dts, ar, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
	pkt.duration = av_rescale_q(pkt.duration, ar, out_stream->time_base);

	//FIX:Bitstream Filter
#if USE_H264BSF
	av_bitstream_filter_filter(h264bsfc, in_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
#if USE_AACBSF
	av_bitstream_filter_filter(aacbsfc, out_stream->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif

	pkt.pos = -1;
	pkt.stream_index=indx_out;
	//Write
	if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
		printf( "Error muxing packet\n");
	}
	av_free_packet(&pkt);
	return 0;
}

int AVRecorder::done() {

	//Write file trailer
	av_write_trailer(ofmt_ctx);

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
	av_bitstream_filter_close(aacbsfc);
#endif
	error_process();
	return 0;
}


int main(int argc, char* argv[]) {
	uint8_t*      pFrameBuffer = new unsigned char[512*1024];
	uint8_t*      pFrameBufferAudio = new unsigned char[16*1024];
	int  iRetVideo = 0xffff;
	int  iRetAudio = 0xffff;

	int      ulFrameSizeVideo = 0;
	uint64_t      ullTimeStampVideo = 0;
	int         iFrameTypeVideo = 0;

	int      ulFrameSizeAudio = 0;
	uint64_t      ullTimeStampAudio = 0;
	int         iFrameTypeAudio = 0;

	av_register_all();
	CReadFrame* pReadFrameVideo = new CReadFrame();
	CReadFrame* pReadFrameAudio = new CReadFrame();

	pReadFrameVideo->SetIndexFilePath("video.dat");
	pReadFrameVideo->SetMediaDataFilePath("video.h264");
    pReadFrameAudio->SetIndexFilePath("audio.dat");
	pReadFrameAudio->SetMediaDataFilePath("audio.aac");

	AVRecorder* recorder = new AVRecorder();
	iRetVideo = pReadFrameVideo->ReadFrame(pFrameBuffer, &ulFrameSizeVideo, &ullTimeStampVideo, 512*1024, &iFrameTypeVideo);
	iRetAudio = pReadFrameAudio->ReadFrame(pFrameBufferAudio, &ulFrameSizeAudio, &ullTimeStampAudio, 16*1024, &iFrameTypeAudio);
	while(iRetVideo == 0 || iRetAudio == 0)
	{

		if(((iRetAudio == 0 && iRetVideo == 0) && ullTimeStampVideo <ullTimeStampAudio) ||
			(iRetVideo == 0 && iRetAudio !=0 ))
		{
			ullTimeStampVideo -= PTS_OFFSET;
			recorder->record(pFrameBuffer, ulFrameSizeVideo, ullTimeStampVideo, ullTimeStampVideo, 1, iFrameTypeVideo);
			iRetVideo = pReadFrameVideo->ReadFrame(pFrameBuffer, &ulFrameSizeVideo, &ullTimeStampVideo, 512*1024, &iFrameTypeVideo);
		}
		else if(iRetAudio == 0) {
			ullTimeStampAudio -= PTS_OFFSET;
			recorder->record(pFrameBufferAudio, ulFrameSizeAudio, ullTimeStampAudio, ullTimeStampAudio, 0, iFrameTypeAudio);
			iRetAudio = pReadFrameAudio->ReadFrame(pFrameBufferAudio, &ulFrameSizeAudio, &ullTimeStampAudio, 16*1024, &iFrameTypeAudio);
		}
	}
	if (iRetAudio != 0 && iRetVideo != 0)
		recorder->record(pFrameBuffer, ulFrameSizeVideo, ullTimeStampVideo, ullTimeStampVideo, 1, 2);
}
