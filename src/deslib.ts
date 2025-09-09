import {
  DataType,
  JsExternal,
  PointerType,
  open,
  define,
  createPointer,
  freePointer,
  unwrapPointer,
} from "ffi-rs";

const paramsType = {
  input: DataType.String,
  output: DataType.String,
  filters: DataType.String,
  format: DataType.String,
  encoder: DataType.String,
  encoderParams: DataType.String,
  pixelFormat: DataType.String,
  isVideo: DataType.Boolean,
};

export type Params = {
  input: string;
  output: string;
  filters: string;
  format: string;
  encoder: string;
  encoderParams: string;
  pixelFormat: string;
  isVideo: boolean;
};

open({
  library: "deslib",
  path: "/Users/romain/sources/test-ffmpeg-transcode-lib/dist/libdeslib.dylib",
});

const lib = define({
  get_strerror: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.I32, DataType.U8Array, DataType.I32],
  },
  alloc_handler: {
    library: "deslib",
    retType: DataType.External,
    paramsType: [],
  },
  init_handler: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [paramsType, DataType.External],
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
  },
});

export const strerr = (err: number) => {
  const str = Buffer.alloc(1024);
  const ret = lib.get_strerror([err, str, 1024]);
  if (ret < 0) throw new Error(`Invalid error: ${err}`);
  return str.slice(0, ret).toString();
};

export const init_handler = async (params: Params) => {
  const handler = lib.alloc_handler([]);

  const ret = await lib.init_handler([params, handler]);

  if (ret < 0) {
    lib.close_handler([handler]);
    throw new Error(`Error while creating handler: ${strerr(ret)}`);
  }

  return handler;
};

export const seek = (handler: JsExternal, position: number) =>
  lib.seek([handler, position]);

export const process_frames = (handler: JsExternal) =>
  lib.process_frames([handler]);

export const flush = (handler: JsExternal) => lib.flush([handler]);

export const close_handler = (handler: JsExternal) =>
  lib.close_handler([handler]);
