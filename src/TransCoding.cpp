/*
 * TransCoding.cpp
 *
 *  Created on: 2015-10-14
 *      Author: lifeng
 */
#include "TransCoding.h"

AVRecorder::TransCoding::TransCoding() {
	aud_codec_id = AV_CODEC_ID_AAC;
	owner = NULL;
	filter_ctx = NULL;

}
AVRecorder::TransCoding::TransCoding(AVRecorder *owner_ptr) {
	aud_codec_id = AV_CODEC_ID_AAC;
	owner = owner_ptr;
	filter_ctx = NULL;
}
AVRecorder::TransCoding::~TransCoding() {

}

AVCodecID AVRecorder::TransCoding::get_codec_id() {
	return aud_codec_id;
}
bool AVRecorder::TransCoding::is_filter_ctx_initialized() {
	return (filter_ctx != NULL);
}
int AVRecorder::TransCoding::init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    AVFilter *buffersrc = NULL;
    AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        snprintf(args, sizeof(args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                dec_ctx->time_base.num, dec_ctx->time_base.den,
                dec_ctx->sample_aspect_ratio.num,
                dec_ctx->sample_aspect_ratio.den);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",  args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        if (!dec_ctx->channel_layout)
            dec_ctx->channel_layout = av_get_default_channel_layout(dec_ctx->channels);
        snprintf(args, sizeof(args),
        		"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
        		dec_ctx->time_base.num, /*dec_ctx->time_base.den*/8000, dec_ctx->sample_rate,
        		av_get_sample_fmt_name(dec_ctx->sample_fmt),
        		dec_ctx->channel_layout);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }
        ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
                (uint8_t*)&enc_ctx->channel_layout,
                sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
            goto end;
        }

    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}


int AVRecorder::TransCoding::init_filters(AVFormatContext *ifmt_ctx)
{
    const char *filter_spec;
    unsigned int i;
    int ret;
    AVCodecContext *in_codec_ctx;
    AVCodecContext *out_codec_ctx;
    filter_ctx = (FilteringContext*)av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx  = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph   = NULL;

        in_codec_ctx = ifmt_ctx->streams[i]->codec;
        if (!(in_codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO
                || in_codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;
        if (in_codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            filter_spec = "null"; /* passthrough (dummy) filter for video */
            out_codec_ctx = owner->ofmt_ctx->streams[owner->out_video_index]->codec;
        }
        else {
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
            out_codec_ctx = owner->ofmt_ctx->streams[owner->out_audio_index]->codec;
        }
        ret = init_filter(&filter_ctx[i], in_codec_ctx, /*owner->ofmt_ctx->streams[i]->codec*/out_codec_ctx, filter_spec);
        if (ret)
            return ret;
    }
    return 0;
}


int AVRecorder::TransCoding::encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int* got_frame) {
	int ret;
	int got_frame_local;
	AVPacket enc_pkt;
	if (!got_frame)
		got_frame =&got_frame_local;
	av_log(NULL,AV_LOG_INFO, "Encoding frame\n");
	/* encode filtered frame */
	enc_pkt.data =NULL;
	enc_pkt.size =0;
	av_init_packet(&enc_pkt);
	ret = avcodec_encode_audio2(owner->ofmt_ctx->streams[stream_index]->codec, &enc_pkt, filt_frame, got_frame);
	av_frame_free(&filt_frame);
	if (ret < 0)
		return ret;
	if (!(*got_frame))
		return 0;
	/* prepare packet for muxing */
	enc_pkt.stream_index = stream_index;
	enc_pkt.dts =av_rescale_q_rnd(enc_pkt.dts,
			owner->ofmt_ctx->streams[stream_index]->codec->time_base,
			owner->ofmt_ctx->streams[stream_index]->time_base,
			(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
	enc_pkt.pts =av_rescale_q_rnd(enc_pkt.pts,
			owner->ofmt_ctx->streams[stream_index]->codec->time_base,
			owner->ofmt_ctx->streams[stream_index]->time_base,
			(AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
	enc_pkt.duration = av_rescale_q(enc_pkt.duration,
			owner->ofmt_ctx->streams[stream_index]->codec->time_base,
			owner->ofmt_ctx->streams[stream_index]->time_base);
	av_log(NULL,AV_LOG_DEBUG, "Muxing frame\n");
	/* mux encoded frame */
	ret =av_interleaved_write_frame(owner->ofmt_ctx, &enc_pkt);
	return ret;
}

int AVRecorder::TransCoding::filter_encode_write_frame(AVFrame *frame, unsigned int out_index)
{
	int ret;
	AVFrame *filt_frame;
	av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
	/* push the decoded frame into the filter graph */
	ret = av_buffersrc_add_frame_flags(filter_ctx[out_index].buffersrc_ctx,frame,0);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error while feeding the filter graph\n");
		return ret;
	}
	/* pull filtered frames from the filter graph */
	while (1) {
		filt_frame= av_frame_alloc();
		if (!filt_frame) {
			ret =AVERROR(ENOMEM);
			break;
		}
		av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
		ret =av_buffersink_get_frame(filter_ctx[out_index].buffersink_ctx, filt_frame);
		if (ret < 0) {
			/* if nomore frames for output - returns AVERROR(EAGAIN)
			 * if flushed and no more frames for output - returns AVERROR_EOF
			 * rewrite retcode to 0 to show it as normal procedure completion
			 */
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				ret= 0;
			av_frame_free(&filt_frame);
			break;
		}
		filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
		ret =encode_write_frame(filt_frame, out_index, NULL);
		if (ret < 0)
			break;
	}
    return ret;
}

int AVRecorder::TransCoding::flush_encoder(unsigned int in_indx)
{
    int ret;
    int got_frame;

    if (!(owner->ofmt_ctx->streams[in_indx]->codec->codec->capabilities & CODEC_CAP_DELAY))
        return 0;

    while (1) {
        av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", in_indx);
        ret = encode_write_frame(NULL, in_indx, &got_frame);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}

int AVRecorder::TransCoding::do_transcoding(AVFormatContext *ifmt_ctx, AVPacket *pkt, int in_indx, int out_indx) {
	int got_frame;
	int ret;
    if (filter_ctx[in_indx].filter_graph) {
        av_log(NULL, AV_LOG_DEBUG, "Going to re-encode&filter the frame\n");
    	AVFrame *frame = av_frame_alloc();
        if (!frame) {
            ret = AVERROR(ENOMEM);
            return -1;
        }
        AVRational ar = {1, 1000};
        pkt->dts = av_rescale_q_rnd(pkt->dts,
                /*ifmt_ctx->streams[in_indx]->time_base,*/ar,
                ifmt_ctx->streams[in_indx]->codec->time_base,
                 (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt->pts = av_rescale_q_rnd(pkt->pts,
                /*ifmt_ctx->streams[in_indx]->time_base*/ar,
                ifmt_ctx->streams[in_indx]->codec->time_base,
                (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));

        ret = avcodec_decode_audio4(owner->in_stream->codec, frame, &got_frame, pkt);
        if (ret < 0) {
           av_frame_free(&frame);
           av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
        }
        printf("ccccccccccc....\n");

        if (got_frame) {
        	frame->pts = av_frame_get_best_effort_timestamp(frame);
        	ret= filter_encode_write_frame(frame, out_indx);
        	av_frame_free(&frame);
        	if (ret< 0)
        		return -1;	//todo:这里缺少后续处理主要是一些释放过程
        }
        else {
        	av_frame_free(&frame);
        }

        /* flush filters and encoders */
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
            /* flush filter */
            if (!filter_ctx[i].filter_graph)
                continue;
            ret = filter_encode_write_frame(NULL, i);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
                return -1;
            }

            /* flush encoder */
            ret = flush_encoder(i);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
                return -1;
            }
        }
    	return 0;

    }
    return 0;
}

