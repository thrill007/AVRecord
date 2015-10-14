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
#define AUDIO_DUMP_PACKETS 10
#define VIDEO_DUMP_PACKETS 15
#define COEFF 5

class AVRecorder
{
private:
	AVFormatContext *ifmt_ctx_a;
	AVFormatContext *ifmt_ctx_v;
	AVFormatContext *ofmt_ctx;
	AVStream *in_stream;
	AVStream *out_stream;
	const char *video_dump;
	const char* audio_dump;
	const char* output;

	FILE *fp_dump_v;
	FILE *fp_dump_a;

	int video_dump_packets;
	int audio_dump_packets;
	uint16_t cached_packets;

	AVBitStreamFilterContext* aacbsfc;
	AVBitStreamFilterContext* h264bsfc;

	AVPacket *packet_cache;
	int cached_consumed;

public:
	AVRecorder();
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
