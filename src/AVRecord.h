/*
 * AVRecord.h
 *
 *  Created on: 2015-10-13
 *      Author: lifeng
 */

#ifndef AVRECORD_H_
#define AVRECORD_H_
#ifdef __cplusplus
extern "C"
{
#endif
#include "ReadFrame.h"
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#ifdef __cplusplus
};
#endif

/*
 * AVRecorder.h
 *
 *  Created on: 2015-9-26
 *      Author: lifeng
 */
#define PTS_OFFSET 38722865
#define AUDIO_DUMP_PACKETS 20
#define VIDEO_DUMP_PACKETS 15
#define COEFF 3

class AVRecorder
{
private:
	AVFormatContext *ifmt_ctx_a;
	AVFormatContext *ifmt_ctx_v;
	AVFormatContext *ofmt_ctx;
	AVStream *in_stream;
	AVStream *out_stream;
	char *video_index;
	char *audio_index;
	char *video_dump;
	char* audio_dump;
	const char* output;
	uint8_t out_video_index;
	uint8_t out_audio_index;
	FILE *fp_dump_v;
	FILE *fp_dump_a;

	int video_dump_packets;
	int audio_dump_packets;
	uint16_t cached_packets;

	AVBitStreamFilterContext* aacbsfc;
	AVBitStreamFilterContext* h264bsfc;

	AVPacket *packet_cacher;
	int cached_consumed;
	int frame_index;

	class TransCoding {
	private:
		typedef struct FilteringContext {
			AVFilterContext *buffersink_ctx;
			AVFilterContext *buffersrc_ctx;
			AVFilterGraph *filter_graph;
		} FilteringContext;
		FilteringContext *filter_ctx;
		AVRecorder *owner;
	    AVCodecID aud_codec_id;
	    AVCodecID vid_codec_id;

	private:
		int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, const char *filter_spec);
		int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index);
		int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int*got_frame);
		int flush_encoder(unsigned int in_indx);
	public:
		TransCoding();
		TransCoding(AVRecorder *owner);
		~TransCoding();
		AVCodecID get_codec_id(AVMediaType codec_type);
		int set_codec_id(AVMediaType codec_type, AVCodecID codec_id);
		int init_filters(AVFormatContext *ifmt_ctx);
		int do_transcoding(AVFormatContext *ifmt_ctx, AVPacket *pkt,int in_index, int out_index);
		int flush_filter_and_encoder(AVFormatContext *ifmt_ctx);
		bool is_filter_ctx_initialized();
	};
	TransCoding *transcoding;

public:
	AVRecorder(const char *video_input, const char *audio_input, const char *output);
	~AVRecorder();
	int prepare(const char *ofile);
	int record(uint8_t *frame_data, uint32_t frame_size, uint64_t pts, uint64_t dts, uint8_t frame_type, uint8_t flag);
	int done();
private:
	int open_input_file(AVFormatContext **ifmt_ctx, const char *input_file);
	int open_output_file(int *v_indx_in, int *a_indx_in, int *v_indx_out, int *a_indx_out);
	int dump_file(uint8_t *frame_data, uint32_t frame_size, uint8_t frame_type, const char *dump_file);
	int cache_packets(uint8_t *frame_data, uint32_t frame_size, uint64_t pts, uint64_t dts, uint8_t frame_type, int key_frame);
	int flush_cached_packets(int v_indx_in, int a_indx_in, int v_indx_out, int a_indx_out, int *indx_out);
	void error_process();
};



#endif /* AVRECORD_H_ */
