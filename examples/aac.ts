import {
  Params,
  open,
  seek,
  process,
  flush,
  close,
  strerr,
} from "../src/deslib";

export const exec = async (params: Params) => {
  const handler = await open(params);

  try {
    const ret = await seek(handler, 2.2);
    if (ret < 0) console.error(`Error while seeking: ${strerr(ret)}`);

    await process(handler);
    await flush(handler);
  } finally {
    await close(handler);
  }
};

exec({
  type: "audio",
  input: "/tmp/bla.mp4",
  output: "/tmp/blo.mp4",
  filters: "aphaser",
  format: "mp4",
  encoder: "aac",
  encoderParams: "b 64k",
});
