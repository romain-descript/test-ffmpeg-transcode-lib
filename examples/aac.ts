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
  type: "audio",
  input: "/tmp/bla.mp4",
  output: "/tmp/blo.mp4",
  filters: "aphaser",
  format: "mp4",
  encoder: "aac",
  encoderParams: "b 64k",
});
