import {
  Params,
  init_handler,
  seek,
  process_frames,
  flush,
  close_handler,
  strerr,
} from "../src/deslib";

export const process = async (params: Params) => {
  const handler = await init_handler(params);

  try {
    const ret = await seek(handler, 2.2);
    if (ret < 0) console.error(`Error while seeking: ${strerr(ret)}`);

    await process_frames(handler);
    await flush(handler);
  } finally {
    await close_handler(handler);
  }
};

process({
  input: "/tmp/bla.mp4",
  output: "/tmp/blo.mp4",
  filters:
    "scale=w=780:h=-1:force_original_aspect_ratio=decrease:force_divisible_by=2,dblur",
  format: "mp4",
  pixelFormat: "yuv420p",
  encoder: "libx264",
  encoderParams:
    "x264-params keyint=25:min-keyint=25:scenecut=-1,preset ultrafast",
  isVideo: true,
});
