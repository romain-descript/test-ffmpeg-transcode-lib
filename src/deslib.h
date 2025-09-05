#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

typedef struct handler {
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

  int stream_idx;
  AVPacket *packet;
  int64_t last_position;
  int64_t stop;
} handler_t;

int get_strerror(int err, char *buf, size_t buflen);

handler_t *init_handler(const char *input, const char *output, double stop);
int seek(handler_t *handler, double pos);
int process_frames(handler_t *handler);
int flush(handler_t *handler);
void close_handler(handler_t *handler);
