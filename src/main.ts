import { DataType, open, define } from "ffi-rs";

open({
  library: "deslib",
  path: "/Users/romain/sources/test-ffmpeg-transcode-lib/src/libdeslib.dylib",
});

const lib = define({
  get_strerror: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.I32, DataType.U8Array, DataType.I32],
  },
  init_handler: {
    library: "deslib",
    retType: DataType.External,
    paramsType: [DataType.String, DataType.String, DataType.Double],
    runInNewThread: true,
  },
  seek: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.External, DataType.Double],
    runInNewThread: true,
  },
  process_frames: {
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

const strerr = (err: number) => {
  const str = Buffer.alloc(1024);
  const ret = lib.get_strerror([err, str, 1024]);
  if (ret < 0) throw new Error(`Invalid error: ${err}`);
  return str.slice(0, ret).toString();
};

const init_handler = ({
  input,
  output,
  stop = 0,
}: {
  input: string;
  output: string;
  stop?: number;
}) => lib.init_handler([input, output, stop]);

const seek = (handler: unknown, position: number) =>
  lib.seek([handler, position]);

const process_frames = (handler: unknown) => lib.process_frames([handler]);

const flush = (handler: unknown) => lib.flush([handler]);

const close_handler = (handler: unknown) => lib.close_handler([handler]);

export const process = async ({
  input,
  output,
  from,
  to,
}: {
  input: string;
  output: string;
  from: number;
  to: number;
}) => {
  const handler = await init_handler({ input, output, stop: to });

  try {
    const ret = await seek(handler, from);
    if (ret < 0) console.error(`Error while seeking: ${strerr(ret)}`);

    await process_frames(handler);
    await flush(handler);
  } finally {
    await close_handler(handler);
  }
};

process({ input: "/tmp/bla.mp4", output: "/tmp/blo.mp4", from: 2.2, to: 4.4 });
