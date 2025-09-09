#include "deslib.h"
#include <libavutil/avutil.h>

#define STRERR_LEN 1024

int main(int argc, char **argv) {
  int ret;
  handler_t *handler;
  char strerr[STRERR_LEN];

  if (argc != 3) {
    av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n",
           argv[0]);
    return 1;
  }

  const handler_params_t params = {
      .input = argv[1],
      .output = argv[2],
      .filters = "dblur",
      .format = "mp4",
      .pixel_format = "yuv420p",
      .encoder = "libx264",
      .encoder_params = "x264-params keyint=25:min-keyint=25:scenecut=-1,preset ultrafast",
      .is_video = 1};

  ret = init_handler(&params, &handler);

  if (ret < 0) {
    ret = get_strerror(ret, strerr, STRERR_LEN);
    if (ret == 0)
      av_log(NULL, AV_LOG_ERROR, "Cound not initialize handler: %s\n", strerr);
    return 1;
  }

  ret = seek(handler, 2.2);
  if (ret < 0) {
    ret = get_strerror(ret, strerr, STRERR_LEN);
    if (ret == 0)
      av_log(NULL, AV_LOG_ERROR, "Seek failed: %s\n", strerr);
  }

  ret = process_frames(handler);
  if (ret < 0)
    goto end;

  ret = flush(handler);

end:
  close_handler(handler);

  if (ret < 0) {
    ret = get_strerror(ret, strerr, STRERR_LEN);
    if (ret == 0)
      av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", strerr);
  }

  return ret ? 1 : 0;
}
