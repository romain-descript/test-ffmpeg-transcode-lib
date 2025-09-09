#include <stddef.h>

typedef struct handler handler_t;

typedef struct handler_params {
  const char *input;
  const char *output;
  const char *filters;
  const char *format;
  const char *encoder;
  const char *encoder_params;
  const char *pixel_format;
  const int is_video;
} handler_params_t;

int get_strerror(int err, char *buf, size_t buflen);

int init_handler(const handler_params_t *params, handler_t **handler);
int seek(handler_t *handler, double pos);
int process_frames(handler_t *handler);
int flush(handler_t *handler);
void close_handler(handler_t *handler);
