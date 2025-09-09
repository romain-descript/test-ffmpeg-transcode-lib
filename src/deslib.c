#include "deslib.h"

#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

struct handler {
  AVFormatContext *ifmt_ctx;
  AVFormatContext *ofmt_ctx;

  AVFilterContext *buffersink_ctx;
  AVFilterContext *buffersrc_ctx;
  AVFilterGraph *filter_graph;

  AVPacket *enc_pkt;
  AVFrame *filtered_frame;

  AVCodecContext *dec_ctx;
  AVCodecContext *enc_ctx;

  AVFrame *dec_frame;

  int width;
  int height;
  enum AVPixelFormat pix_fmt;
  AVRational sample_aspect_ratio;
  AVChannelLayout ch_layout;
  int sample_rate;

  int stream_idx;
  AVPacket *packet;
};

int get_strerror(int err, char *buf, size_t buflen) {
  int ret = av_strerror(err, buf, buflen);
  if (ret < 0)
    return ret;

  return strlen(buf);
}

void close_handler(handler_t *handler) {
  if (!handler)
    return;

  avcodec_free_context(&handler->dec_ctx);
  avcodec_free_context(&handler->enc_ctx);
  av_frame_free(&handler->dec_frame);
  avfilter_graph_free(&handler->filter_graph);
  av_packet_free(&handler->enc_pkt);
  av_frame_free(&handler->filtered_frame);
  avformat_close_input(&handler->ifmt_ctx);

  if (handler->ofmt_ctx && !(handler->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&handler->ofmt_ctx->pb);

  avformat_free_context(handler->ofmt_ctx);
  av_packet_free(&handler->packet);
  av_free(handler);
};

static int open_input_file(const handler_params_t *params, handler_t *handler) {
  int i, ret;
  int stream_type = params->is_video ? AVMEDIA_TYPE_VIDEO : AVMEDIA_TYPE_AUDIO;

  if ((ret = avformat_open_input(&handler->ifmt_ctx, params->input, NULL,
                                 NULL)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot open input\n");
    return ret;
  }

  if ((ret = avformat_find_stream_info(handler->ifmt_ctx, NULL)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
    return ret;
  }

  ret = av_find_best_stream(handler->ifmt_ctx, stream_type, -1, -1, NULL, 0);
  if (ret < 0)
    return ret;

  handler->stream_idx = ret;

  for (i = 0; i < handler->ifmt_ctx->nb_streams; i++)
    if (i != handler->stream_idx)
      handler->ifmt_ctx->streams[i]->discard = AVDISCARD_ALL;

  AVStream *stream = handler->ifmt_ctx->streams[handler->stream_idx];

  const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);

  if (!dec) {
    av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n",
           handler->stream_idx);
    ret = AVERROR_DECODER_NOT_FOUND;
    return ret;
  }

  handler->dec_ctx = avcodec_alloc_context3(dec);

  if (!handler->dec_ctx) {
    av_log(NULL, AV_LOG_ERROR,
           "Failed to allocate the decoder context for stream #%u\n",
           handler->stream_idx);
    ret = AVERROR(ENOMEM);
    return ret;
  }

  ret = avcodec_parameters_to_context(handler->dec_ctx, stream->codecpar);

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "Failed to copy decoder parameters to input decoder context "
           "for stream #%u\n",
           handler->stream_idx);
    return ret;
  }

  handler->dec_ctx->pkt_timebase = stream->time_base;
  handler->dec_ctx->framerate =
      av_guess_frame_rate(handler->ifmt_ctx, stream, NULL);

  ret = avcodec_open2(handler->dec_ctx, dec, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n",
           handler->stream_idx);
    return ret;
  }

  handler->dec_frame = av_frame_alloc();
  if (!handler->dec_frame) {
    ret = AVERROR(ENOMEM);
    return ret;
  }

  av_dump_format(handler->ifmt_ctx, 0, params->input, 0);
  return 0;
}

static int open_output_file(const handler_params_t *params,
                            handler_t *handler) {
  AVStream *out_stream;
  AVStream *in_stream;
  const AVCodec *encoder;
  int ret;

  avformat_alloc_output_context2(&handler->ofmt_ctx, NULL, params->format,
                                 params->output);
  if (!handler->ofmt_ctx) {
    av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
    return AVERROR_UNKNOWN;
  }

  out_stream = avformat_new_stream(handler->ofmt_ctx, NULL);
  if (!out_stream) {
    av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
    return AVERROR_UNKNOWN;
  }

  in_stream = handler->ifmt_ctx->streams[handler->stream_idx];

  encoder = avcodec_find_encoder_by_name(params->encoder);
  if (!encoder) {
    av_log(NULL, AV_LOG_FATAL, "Encoder not found!\n");
    return AVERROR_INVALIDDATA;
  }

  handler->enc_ctx = avcodec_alloc_context3(encoder);
  if (!handler->enc_ctx) {
    av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
    return AVERROR(ENOMEM);
  }

  if (params->is_video) {
    handler->enc_ctx->height = handler->height;
    handler->enc_ctx->width = handler->width;
    handler->enc_ctx->sample_aspect_ratio = handler->sample_aspect_ratio;
    handler->enc_ctx->pix_fmt = handler->pix_fmt;
    handler->enc_ctx->time_base = av_inv_q(handler->dec_ctx->framerate);
  } else {
    const enum AVSampleFormat *sample_fmts = NULL;

    handler->enc_ctx->sample_rate = handler->sample_rate;
    ret = av_channel_layout_copy(&handler->enc_ctx->ch_layout,
                                 &handler->ch_layout);
    if (ret < 0)
      return ret;

    ret = avcodec_get_supported_config(handler->dec_ctx, NULL,
                                       AV_CODEC_CONFIG_SAMPLE_FORMAT, 0,
                                       (const void **)&sample_fmts, NULL);

    handler->enc_ctx->sample_fmt = (ret >= 0 && sample_fmts)
                                       ? sample_fmts[0]
                                       : handler->dec_ctx->sample_fmt;

    handler->enc_ctx->time_base =
        (AVRational){1, handler->enc_ctx->sample_rate};
  }

  if (handler->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    handler->enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  AVDictionary *enc_opts = NULL;
  if (params->encoder_params) {
    ret = av_dict_parse_string(&enc_opts, params->encoder_params, " ", ",", 0);
    if (ret < 0)
      return ret;
  }

  ret = avcodec_open2(handler->enc_ctx, encoder, &enc_opts);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot open %s encoder for stream #%u\n",
           encoder->name, handler->stream_idx);
    return ret;
  }

  ret = avcodec_parameters_from_context(out_stream->codecpar, handler->enc_ctx);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "Failed to copy encoder parameters to output stream #%u\n",
           handler->stream_idx);
    return ret;
  }

  out_stream->time_base = handler->enc_ctx->time_base;

  av_dump_format(handler->ofmt_ctx, 0, params->output, 1);

  if (!(handler->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&handler->ofmt_ctx->pb, params->output, AVIO_FLAG_WRITE);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'",
             params->output);
      return ret;
    }
  }

  ret = avformat_write_header(handler->ofmt_ctx, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
    return ret;
  }

  return 0;
}

static int init_filter(const handler_params_t *params, handler_t *handler) {
  char args[512];
  int ret = 0;
  const AVFilter *buffersrc = NULL;
  const AVFilter *buffersink = NULL;
  AVFilterContext *buffersrc_ctx = NULL;
  AVFilterContext *buffersink_ctx = NULL;

  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs = avfilter_inout_alloc();
  AVFilterGraph *filter_graph = avfilter_graph_alloc();

  if (!outputs || !inputs || !filter_graph) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (params->is_video) {
    buffersrc = avfilter_get_by_name("buffer");
    buffersink = avfilter_get_by_name("buffersink");

    if (!buffersrc || !buffersink) {
      av_log(NULL, AV_LOG_ERROR,
             "filtering source or sink element not found\n");
      ret = AVERROR_UNKNOWN;
      goto end;
    }

    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             handler->dec_ctx->width, handler->dec_ctx->height,
             handler->dec_ctx->pix_fmt, handler->dec_ctx->pkt_timebase.num,
             handler->dec_ctx->pkt_timebase.den,
             handler->dec_ctx->sample_aspect_ratio.num,
             handler->dec_ctx->sample_aspect_ratio.den);
  } else {
    char buf[64];
    buffersrc = avfilter_get_by_name("abuffer");
    buffersink = avfilter_get_by_name("abuffersink");

    if (!buffersrc || !buffersink) {
      av_log(NULL, AV_LOG_ERROR,
             "filtering source or sink element not found\n");
      ret = AVERROR_UNKNOWN;
      goto end;
    }

    if (handler->dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
      av_channel_layout_default(&handler->dec_ctx->ch_layout,
                                handler->dec_ctx->ch_layout.nb_channels);
    av_channel_layout_describe(&handler->dec_ctx->ch_layout, buf, sizeof(buf));
    snprintf(args, sizeof(args),
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
             handler->dec_ctx->pkt_timebase.num,
             handler->dec_ctx->pkt_timebase.den, handler->dec_ctx->sample_rate,
             av_get_sample_fmt_name(handler->dec_ctx->sample_fmt), buf);
  }

  ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args,
                                     NULL, filter_graph);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
    goto end;
  }

  buffersink_ctx = avfilter_graph_alloc_filter(filter_graph, buffersink, "out");
  if (!buffersink_ctx) {
    av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (params->is_video) {
    ret =
        av_opt_set_bin(buffersink_ctx, "pix_fmts", (uint8_t *)&handler->pix_fmt,
                       sizeof(handler->pix_fmt), AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
      goto end;
    }
  }

  ret = avfilter_init_dict(buffersink_ctx, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot initialize buffer sink\n");
    goto end;
  }

  /* Endpoints for the filter graph. */
  outputs->name = av_strdup("in");
  outputs->filter_ctx = buffersrc_ctx;
  outputs->pad_idx = 0;
  outputs->next = NULL;

  inputs->name = av_strdup("out");
  inputs->filter_ctx = buffersink_ctx;
  inputs->pad_idx = 0;
  inputs->next = NULL;

  if (!outputs->name || !inputs->name) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if ((ret = avfilter_graph_parse_ptr(filter_graph, params->filters, &inputs,
                                      &outputs, NULL)) < 0)
    goto end;

  if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
    goto end;

  /* Fill FilteringContext */
  handler->buffersrc_ctx = buffersrc_ctx;
  handler->buffersink_ctx = buffersink_ctx;
  handler->filter_graph = filter_graph;

  handler->enc_pkt = av_packet_alloc();
  if (!handler->enc_pkt) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  handler->filtered_frame = av_frame_alloc();
  if (!handler->filtered_frame) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (params->is_video) {
    handler->width = handler->buffersink_ctx->inputs[0]->w;
    handler->height = handler->buffersink_ctx->inputs[0]->h;
    handler->sample_aspect_ratio = handler->buffersink_ctx->inputs[0]->sample_aspect_ratio;
  } else {
    handler->sample_rate = handler->buffersink_ctx->inputs[0]->sample_rate;
    ret = av_channel_layout_copy(
        &handler->ch_layout, &handler->buffersink_ctx->inputs[0]->ch_layout);
    if (ret < 0)
      return ret;
  }

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  return ret;
}

static int encode_write_frame(int flush, handler_t *handler) {
  AVFrame *filt_frame = flush ? NULL : handler->filtered_frame;
  AVPacket *enc_pkt = handler->enc_pkt;
  int ret;

  /* encode filtered frame */
  av_packet_unref(enc_pkt);

  if (filt_frame && filt_frame->pts != AV_NOPTS_VALUE)
    filt_frame->pts = av_rescale_q(filt_frame->pts, filt_frame->time_base,
                                   handler->enc_ctx->time_base);

  ret = avcodec_send_frame(handler->enc_ctx, filt_frame);

  if (ret < 0)
    return ret;

  while (ret >= 0) {
    ret = avcodec_receive_packet(handler->enc_ctx, enc_pkt);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return 0;

    /* prepare packet for muxing */
    enc_pkt->stream_index = handler->stream_idx;
    av_packet_rescale_ts(
        enc_pkt, handler->enc_ctx->time_base,
        handler->ofmt_ctx->streams[handler->stream_idx]->time_base);

    av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(handler->ofmt_ctx, enc_pkt);
  }

  return ret;
}

static int filter_encode_write_frame(AVFrame *frame, handler_t *handler) {
  int ret;

  ret = av_buffersrc_add_frame_flags(handler->buffersrc_ctx, frame, 0);

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
    return ret;
  }

  while (1) {
    ret = av_buffersink_get_frame(handler->buffersink_ctx,
                                  handler->filtered_frame);
    if (ret < 0) {
      /* if no more frames for output - returns AVERROR(EAGAIN)
       * if flushed and no more frames for output - returns AVERROR_EOF
       * rewrite retcode to 0 to show it as normal procedure completion
       */
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        ret = 0;
      break;
    }

    handler->filtered_frame->time_base =
        av_buffersink_get_time_base(handler->buffersink_ctx);
    ;
    handler->filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
    ret = encode_write_frame(0, handler);
    av_frame_unref(handler->filtered_frame);
    if (ret < 0)
      break;
  }

  return ret;
}

int flush_encoder(handler_t *handler) {
  if (!(handler->enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
    return 0;

  return encode_write_frame(1, handler);
}

int init_handler(const handler_params_t *params, handler_t **handler_ptr) {
  int ret;

  *handler_ptr = av_mallocz(sizeof(handler_t));
  if (!*handler_ptr)
    return AVERROR(ENOMEM);

  handler_t *handler = *handler_ptr;

  if (params->is_video) {
    handler->pix_fmt = av_get_pix_fmt(params->pixel_format);
    if (handler->pix_fmt == AV_PIX_FMT_NONE) {
      ret = AVERROR(EINVAL);
      goto end;
    }
  }

  if ((ret = open_input_file(params, handler)) < 0)
    goto end;

  if ((ret = init_filter(params, handler)) < 0)
    goto end;

  if ((ret = open_output_file(params, handler)) < 0)
    goto end;

  if (!(handler->packet = av_packet_alloc())) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  return 0;

end:
  close_handler(handler);
  return ret;
}

static int process_frame(handler_t *handler) {
  int ret, stream_index = -1;

  while (stream_index != handler->stream_idx) {
    if ((ret = av_read_frame(handler->ifmt_ctx, handler->packet)) < 0)
      return ret;

    stream_index = handler->packet->stream_index;
  }

  ret = avcodec_send_packet(handler->dec_ctx, handler->packet);
  if (ret < 0)
    return ret;

  while (ret >= 0) {
    ret = avcodec_receive_frame(handler->dec_ctx, handler->dec_frame);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
      break;
    else if (ret < 0)
      return ret;

    handler->dec_frame->pts = handler->dec_frame->best_effort_timestamp;
    ret = filter_encode_write_frame(handler->dec_frame, handler);
    if (ret < 0)
      return ret;
  }

  return 0;
}

int process_frames(handler_t *handler) {
  int ret = 0;

  do {
    ret = process_frame(handler);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
      break;

    if (ret < 0)
      return ret;
  } while (1);

  return 0;
}

int flush(handler_t *handler) {
  int ret;

  ret = avcodec_send_packet(handler->dec_ctx, NULL);
  if (ret < 0)
    return ret;

  while (ret >= 0) {
    ret = avcodec_receive_frame(handler->dec_ctx, handler->dec_frame);
    if (ret == AVERROR_EOF)
      break;
    else if (ret < 0)
      return ret;

    handler->dec_frame->pts = handler->dec_frame->best_effort_timestamp;

    ret = filter_encode_write_frame(handler->dec_frame, handler);
    if (ret < 0)
      return ret;
  }

  ret = filter_encode_write_frame(NULL, handler);
  if (ret < 0)
    return ret;

  ret = flush_encoder(handler);
  if (ret < 0)
    return ret;

  return av_write_trailer(handler->ofmt_ctx);
}

int seek(handler_t *handler, double pos) {
  AVStream *stream = handler->ifmt_ctx->streams[handler->stream_idx];
  int64_t seek_timestamp = pos * AV_TIME_BASE;

  return avformat_seek_file(handler->ifmt_ctx, -1, -INT64_MAX, seek_timestamp,
                            INT64_MAX, 0);
}
