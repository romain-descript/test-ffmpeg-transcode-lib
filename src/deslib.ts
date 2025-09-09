import { DataType, JsExternal, open, define } from "ffi-rs";
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

type BaseParams = {
  input: string;
  output: string;
  filters: string;
  format: string;
  encoder: string;
  encoderParams: string;
};

export type Params = BaseParams &
  (
    | {
        type: "video";
        pixelFormat: string;
      }
    | { type: "audio" }
  );

const sharedLibExt = os.platform() === "darwin" ? ".dylib" : ".so";

open({
  library: "deslib",
  path: path.resolve(__dirname, `../libdeslib${sharedLibExt}`),
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

export const process_frames = (handler: JsExternal) =>
  lib.process_frames([handler]);

export const flush = (handler: JsExternal) => lib.flush([handler]);

export const close_handler = (handler: JsExternal) =>
  lib.close_handler([handler]);
