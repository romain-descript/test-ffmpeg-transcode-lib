#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

typedef struct FilteringContext {
  AVFilterContext *buffersink_ctx;
  AVFilterContext *buffersrc_ctx;
  AVFilterGraph *filter_graph;

  AVPacket *enc_pkt;
  AVFrame *filtered_frame;
} FilteringContext;

typedef struct StreamContext {
  AVCodecContext *dec_ctx;
  AVCodecContext *enc_ctx;

  AVFrame *dec_frame;
} StreamContext;

typedef struct handler {
  AVFormatContext *ifmt_ctx;
  AVFormatContext *ofmt_ctx;
  FilteringContext *filter_ctx;
  StreamContext *stream_ctx;
  int stream_idx;
  AVPacket *packet;
} handler_t;

int get_eof();
int get_eagain();
int get_strerror(int err, char *buf, size_t buflen);

handler_t *init_handler(const char *input, const char *output);
int process_frame(handler_t *handler);
int flush(handler_t *handler);
void close_handler(handler_t *handler);
