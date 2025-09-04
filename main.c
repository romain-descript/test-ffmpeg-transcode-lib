#include "deslib.h"

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
