#include "deslib.h"

#define STRERR_LEN 1024

int main(int argc, char **argv) {
  int ret;
  handler_t *handler;
  char strerr[STRERR_LEN];
  int eof = get_eof();
  int eagain = get_eagain();

  if (argc != 3) {
    av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n",
           argv[0]);
    return 1;
  }

  handler = init_handler(argv[1], argv[2]);

  if (!handler) {
    av_log(NULL, AV_LOG_ERROR, "Cound not initialize handler!\n");
    return 1;
  }

  do {
    ret = process_frame(handler);
    if (ret == eof || ret == eagain)
      break;

    if (ret < 0)
      goto end;
  } while (1);

  ret = flush(handler);

end:
  close_handler(handler);

  if (ret < 0) {
    ret = get_strerror(ret, strerr, STRERR_LEN);
    if (ret < 0)
      av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", strerr);
  }

  return ret ? 1 : 0;
}
