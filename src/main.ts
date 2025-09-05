import { DataType, open, define } from "ffi-rs";

open({
  library: "deslib",
  path: "/Users/romain/sources/test-ffmpeg-transcode-lib/src/libdeslib.dylib",
});

const lib = define({
  get_eof: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [],
  },
  get_eagain: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [],
  },
  get_strerror: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.I32, DataType.U8Array, DataType.I32],
  },
  init_handler: {
    library: "deslib",
    retType: DataType.External,
    paramsType: [DataType.String, DataType.String],
    runInNewThread: true,
  },
  seek: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.External, DataType.Double],
    runInNewThread: true,
  },
  last_position: {
    library: "deslib",
    retType: DataType.Double,
    paramsType: [DataType.External],
    runInNewThread: true,
  },
  process_frame: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.External],
    runInNewThread: true,
  },
  flush: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.External],
    runInNewThread: true,
  },
  close_handler: {
    library: "deslib",
    retType: DataType.Void,
    paramsType: [DataType.External],
    runInNewThread: true,
  },
});

const eof = lib.get_eof([]);
const eagain = lib.get_eagain([]);

const strerr = (err: number) => {
  const str = Buffer.alloc(1024);
  const ret = lib.get_strerror([err, str, 1024]);
  if (ret < 0) throw new Error(`Invalid error: ${err}`);
  return str.slice(0, ret).toString();
};

const init_handler = (input: string, output: string) =>
  lib.init_handler([input, output]);

const seek = (handler: unknown, position: number) =>
  lib.seek([handler, position]);

const last_position = (handler: unknown) => lib.last_position([handler]);

const process_frame = (handler: unknown) => lib.process_frame([handler]);

const flush = (handler: unknown) => lib.flush([handler]);

const close_handler = (handler: unknown) => lib.close_handler([handler]);

export const process = async (
  input: string,
  output: string,
  from: number,
  to: number,
) => {
  const handler = await init_handler(input, output);

  const ret = await seek(handler, from);
  if (ret < 0) console.error(`Error while seeking: ${strerr(ret)}`);

  try {
    do {
      if (to <= (await last_position(handler))) break;

      const ret = await process_frame(handler);
      if (ret === eof || ret === eagain) break;

      if (ret < 0) throw new Error(`Error: ${strerr(ret)}`);
    } while (true);

    await flush(handler);
  } finally {
    await close_handler(handler);
  }
};
