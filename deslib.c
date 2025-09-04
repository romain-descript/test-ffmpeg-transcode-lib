#include "deslib.h"

void close_handler(handler_t *handler) {
  if (!handler)
    return;

  if (handler->stream_ctx) {
    avcodec_free_context(&handler->stream_ctx->dec_ctx);
    avcodec_free_context(&handler->stream_ctx->enc_ctx);
    av_frame_free(&handler->stream_ctx->dec_frame);
  }

  if (handler->filter_ctx && handler->filter_ctx->filter_graph) {
    avfilter_graph_free(&handler->filter_ctx->filter_graph);
    av_packet_free(&handler->filter_ctx->enc_pkt);
    av_frame_free(&handler->filter_ctx->filtered_frame);
  }

  av_free(handler->filter_ctx);
  av_free(handler->stream_ctx);
  avformat_close_input(&handler->ifmt_ctx);

  if (handler->ofmt_ctx && !(handler->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&handler->ofmt_ctx->pb);

  avformat_free_context(handler->ofmt_ctx);
  av_packet_free(&handler->packet);
  av_free(handler);
};

int open_input_file(const char *filename, handler_t *handler) {
  int err;

  if ((err = avformat_open_input(&handler->ifmt_ctx, filename, NULL, NULL)) <
      0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
    return err;
  }

  if ((err = avformat_find_stream_info(handler->ifmt_ctx, NULL)) < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
    return err;
  }

  err = av_find_best_stream(handler->ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL,
                            0);
  if (err < 0)
    return err;

  handler->stream_idx = err;

  handler->stream_ctx = av_mallocz(sizeof(StreamContext));

  if (!handler->stream_ctx) {
    err = AVERROR(ENOMEM);
    return err;
  }

  AVStream *stream = handler->ifmt_ctx->streams[handler->stream_idx];

  const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);

  if (!dec) {
    av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n",
           handler->stream_idx);
    err = AVERROR_DECODER_NOT_FOUND;
    return err;
  }

  handler->stream_ctx->dec_ctx = avcodec_alloc_context3(dec);

  if (!handler->stream_ctx->dec_ctx) {
    av_log(NULL, AV_LOG_ERROR,
           "Failed to allocate the decoder context for stream #%u\n",
           handler->stream_idx);
    err = AVERROR(ENOMEM);
    return err;
  }

  err = avcodec_parameters_to_context(handler->stream_ctx->dec_ctx,
                                      stream->codecpar);

  if (err < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "Failed to copy decoder parameters to input decoder context "
           "for stream #%u\n",
           handler->stream_idx);
    return err;
  }

  handler->stream_ctx->dec_ctx->pkt_timebase = stream->time_base;
  handler->stream_ctx->dec_ctx->framerate =
      av_guess_frame_rate(handler->ifmt_ctx, stream, NULL);

  err = avcodec_open2(handler->stream_ctx->dec_ctx, dec, NULL);
  if (err < 0) {
    av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n",
           handler->stream_idx);
    return err;
  }

  handler->stream_ctx->dec_frame = av_frame_alloc();
  if (!handler->stream_ctx->dec_frame) {
    err = AVERROR(ENOMEM);
    return err;
  }

  av_dump_format(handler->ifmt_ctx, 0, filename, 0);
  return 0;
}

int open_output_file(const char *filename, handler_t *handler) {
  AVStream *out_stream;
  AVStream *in_stream;
  AVCodecContext *dec_ctx;
  const AVCodec *encoder;
  int err;

  avformat_alloc_output_context2(&handler->ofmt_ctx, NULL, NULL, filename);
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
  dec_ctx = handler->stream_ctx->dec_ctx;

  /* in this example, we choose transcoding to same codec */
  encoder = avcodec_find_encoder(dec_ctx->codec_id);
  if (!encoder) {
    av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
    return AVERROR_INVALIDDATA;
  }

  handler->stream_ctx->enc_ctx = avcodec_alloc_context3(encoder);
  if (!handler->stream_ctx->enc_ctx) {
    av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
    return AVERROR(ENOMEM);
  }

  handler->stream_ctx->enc_ctx->height = dec_ctx->height;
  handler->stream_ctx->enc_ctx->width = dec_ctx->width;
  handler->stream_ctx->enc_ctx->sample_aspect_ratio =
      dec_ctx->sample_aspect_ratio;
  /* take first format from list of supported formats */
  if (encoder->pix_fmts)
    handler->stream_ctx->enc_ctx->pix_fmt = encoder->pix_fmts[0];
  else
    handler->stream_ctx->enc_ctx->pix_fmt = dec_ctx->pix_fmt;
  /* video time_base can be set to whatever is handy and supported by
   * encoder */
  handler->stream_ctx->enc_ctx->time_base = av_inv_q(dec_ctx->framerate);

  if (handler->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    handler->stream_ctx->enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  /* Third parameter can be used to pass settings to encoder */
  err = avcodec_open2(handler->stream_ctx->enc_ctx, encoder, NULL);
  if (err < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot open %s encoder for stream #%u\n",
           encoder->name, handler->stream_idx);
    return err;
  }

  err = avcodec_parameters_from_context(out_stream->codecpar,
                                        handler->stream_ctx->enc_ctx);
  if (err < 0) {
    av_log(NULL, AV_LOG_ERROR,
           "Failed to copy encoder parameters to output stream #%u\n",
           handler->stream_idx);
    return err;
  }

  out_stream->time_base = handler->stream_ctx->enc_ctx->time_base;

  av_dump_format(handler->ofmt_ctx, 0, filename, 1);

  if (!(handler->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
    err = avio_open(&handler->ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (err < 0) {
      av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
      return err;
    }
  }

  /* init muxer, write output file header */
  err = avformat_write_header(handler->ofmt_ctx, NULL);
  if (err < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
    return err;
  }

  return 0;
}

int init_filter(handler_t *handler) {
  char args[512];
  int ret = 0;
  const AVFilter *buffersrc = NULL;
  const AVFilter *buffersink = NULL;
  AVFilterContext *buffersrc_ctx = NULL;
  AVFilterContext *buffersink_ctx = NULL;
  FilteringContext *fctx;
  AVCodecContext *dec_ctx;
  AVCodecContext *enc_ctx;
  const char *filter_spec = "null";

  handler->filter_ctx = av_mallocz(sizeof(FilteringContext));
  if (!handler->filter_ctx)
    return AVERROR(ENOMEM);

  fctx = handler->filter_ctx;
  dec_ctx = handler->stream_ctx->dec_ctx;
  enc_ctx = handler->stream_ctx->enc_ctx;

  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs = avfilter_inout_alloc();
  AVFilterGraph *filter_graph = avfilter_graph_alloc();

  if (!outputs || !inputs || !filter_graph) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

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
           dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den,
           dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

  ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args,
                                     NULL, filter_graph);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
    goto end;
  }

  ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL,
                                     NULL, filter_graph);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
    goto end;
  }

  ret = av_opt_set_bin(buffersink_ctx, "pix_fmts", (uint8_t *)&enc_ctx->pix_fmt,
                       sizeof(enc_ctx->pix_fmt), AV_OPT_SEARCH_CHILDREN);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
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

  if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec, &inputs,
                                      &outputs, NULL)) < 0)
    goto end;

  if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
    goto end;

  /* Fill FilteringContext */
  fctx->buffersrc_ctx = buffersrc_ctx;
  fctx->buffersink_ctx = buffersink_ctx;
  fctx->filter_graph = filter_graph;

  fctx->enc_pkt = av_packet_alloc();
  if (!fctx->enc_pkt) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  fctx->filtered_frame = av_frame_alloc();
  if (!fctx->filtered_frame) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  return ret;
}

static int encode_write_frame(int flush, handler_t *handler) {
  StreamContext *stream = handler->stream_ctx;
  FilteringContext *filter = handler->filter_ctx;
  AVFrame *filt_frame = flush ? NULL : filter->filtered_frame;
  AVPacket *enc_pkt = filter->enc_pkt;
  int ret;

  /* encode filtered frame */
  av_packet_unref(enc_pkt);

  if (filt_frame && filt_frame->pts != AV_NOPTS_VALUE)
    filt_frame->pts = av_rescale_q(filt_frame->pts, filt_frame->time_base,
                                   stream->enc_ctx->time_base);

  ret = avcodec_send_frame(stream->enc_ctx, filt_frame);

  if (ret < 0)
    return ret;

  while (ret >= 0) {
    ret = avcodec_receive_packet(stream->enc_ctx, enc_pkt);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      return 0;

    /* prepare packet for muxing */
    enc_pkt->stream_index = handler->stream_idx;
    av_packet_rescale_ts(
        enc_pkt, stream->enc_ctx->time_base,
        handler->ofmt_ctx->streams[handler->stream_idx]->time_base);

    av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(handler->ofmt_ctx, enc_pkt);
  }

  return ret;
}

static int filter_encode_write_frame(AVFrame *frame, handler_t *handler) {
  FilteringContext *filter = handler->filter_ctx;
  int ret;

  ret = av_buffersrc_add_frame_flags(filter->buffersrc_ctx, frame, 0);

  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
    return ret;
  }

  while (1) {
    ret =
        av_buffersink_get_frame(filter->buffersink_ctx, filter->filtered_frame);
    if (ret < 0) {
      /* if no more frames for output - returns AVERROR(EAGAIN)
       * if flushed and no more frames for output - returns AVERROR_EOF
       * rewrite retcode to 0 to show it as normal procedure completion
       */
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        ret = 0;
      break;
    }

    filter->filtered_frame->time_base =
        av_buffersink_get_time_base(filter->buffersink_ctx);
    ;
    filter->filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
    ret = encode_write_frame(0, handler);
    av_frame_unref(filter->filtered_frame);
    if (ret < 0)
      break;
  }

  return ret;
}

static int flush_encoder(handler_t *handler) {
  if (!(handler->stream_ctx->enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
    return 0;

  return encode_write_frame(1, handler);
}

int init_handler(const char *input, const char *output, handler_t **handler) {
  int ret;

  *handler = av_mallocz(sizeof(handler_t));
  if (!*handler)
    return AVERROR(ENOMEM);

  if ((ret = open_input_file(input, *handler)) < 0)
    goto end;

  if ((ret = open_output_file(output, *handler)) < 0)
    goto end;

  if ((ret = init_filter(*handler)) < 0)
    goto end;

  if (!((*handler)->packet = av_packet_alloc())) {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  return 0;

end:
  close_handler(*handler);
  return ret;
}

int process_frame(handler_t *handler) {
  int ret, stream_index = -1;

  while (stream_index != handler->stream_idx) {
    if ((ret = av_read_frame(handler->ifmt_ctx, handler->packet)) < 0)
      return ret;

    stream_index = handler->packet->stream_index;
  }

  ret = avcodec_send_packet(handler->stream_ctx->dec_ctx, handler->packet);
  if (ret < 0)
    return ret;

  while (ret >= 0) {
    ret = avcodec_receive_frame(handler->stream_ctx->dec_ctx,
                                handler->stream_ctx->dec_frame);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
      break;
    else if (ret < 0)
      return ret;

    handler->stream_ctx->dec_frame->pts =
        handler->stream_ctx->dec_frame->best_effort_timestamp;
    ret = filter_encode_write_frame(handler->stream_ctx->dec_frame, handler);
    if (ret < 0)
      return ret;
  }

  return 0;
}

int flush(handler_t *handler) {
  int ret;

  ret = avcodec_send_packet(handler->stream_ctx->dec_ctx, NULL);
  if (ret < 0)
    return ret;

  while (ret >= 0) {
    ret = avcodec_receive_frame(handler->stream_ctx->dec_ctx,
                                handler->stream_ctx->dec_frame);
    if (ret == AVERROR_EOF)
      break;
    else if (ret < 0)
      return ret;

    handler->stream_ctx->dec_frame->pts =
        handler->stream_ctx->dec_frame->best_effort_timestamp;

    ret = filter_encode_write_frame(handler->stream_ctx->dec_frame, handler);
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

/*
int main(int argc, char **argv) {
  int ret;
  handler_t *handler;

  if (argc != 3) {
    av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n",
           argv[0]);
    return 1;
  }

  if ((ret = init_handler(argv[1], argv[2], &handler)) < 0)
    return ret;

  do {
    ret = process_frame(handler);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
      break;

    if (ret < 0)
      goto end;
  } while (1);

  ret = flush(handler);

end:
  close_handler(handler);

  if (ret < 0)
    av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

  return ret ? 1 : 0;
}
*/
