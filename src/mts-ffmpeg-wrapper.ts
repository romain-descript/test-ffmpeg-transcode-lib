import { DataType, JsExternal, open as openLib, define } from "ffi-rs";
import path from "node:path";
import os from "node:os";

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

interface BaseParams {
  input: string;
  output: string;
  filters: string;
  format: string;
  encoder: string;
  encoderParams: string;
}

interface AudioParams extends BaseParams {
  type: "audio";
}

interface VideoParams extends BaseParams {
  type: "video";
  pixelFormat: string;
}

export type Params = AudioParams | VideoParams;

const sharedLibExt = os.platform() === "darwin" ? ".dylib" : ".so";

openLib({
  library: "mts-ffmpeg-wrapper",
  path: path.resolve(__dirname, `libmts-ffmpeg-wrapper${sharedLibExt}`),
});

const lib = define({
  get_strerror: {
    library: "mts-ffmpeg-wrapper",
    retType: DataType.I32,
    paramsType: [DataType.I32, DataType.U8Array, DataType.I32],
  },
  alloc_handler: {
    library: "mts-ffmpeg-wrapper",
    retType: DataType.External,
    paramsType: [],
  },
  init_handler: {
    library: "mts-ffmpeg-wrapper",
    retType: DataType.I32,
    paramsType: [paramsType, DataType.External],
    runInNewThread: true,
  },
  seek: {
    library: "mts-ffmpeg-wrapper",
    retType: DataType.I32,
    paramsType: [DataType.External, DataType.Double],
    runInNewThread: true,
  },
  process_frames: {
    library: "mts-ffmpeg-wrapper",
    retType: DataType.I32,
    paramsType: [DataType.External],
    runInNewThread: true,
  },
  flush: {
    library: "mts-ffmpeg-wrapper",
    retType: DataType.I32,
    paramsType: [DataType.External],
    runInNewThread: true,
  },
  close_handler: {
    library: "mts-ffmpeg-wrapper",
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

export const open = async (params: Params) => {
  const handler = lib.alloc_handler([]);

  let { type, ...effectiveParams } = params;

  const ret = await lib.init_handler([
    {
      isVideo: type == "video",
      // ffi-rs requires it all the time.
      ...(type == "audio" ? { pixelFormat: "dummy" } : {}),
      ...effectiveParams,
    },
    handler,
  ]);

  if (ret < 0) {
    lib.close_handler([handler]);
    throw new Error(`Error while creating handler: ${strerr(ret)}`);
  }

  return handler;
};

export const seek = (handler: JsExternal, position: number) =>
  lib.seek([handler, position]);

export const process = (handler: JsExternal) => lib.process_frames([handler]);

export const flush = (handler: JsExternal) => lib.flush([handler]);

export const close = (handler: JsExternal) => lib.close_handler([handler]);
