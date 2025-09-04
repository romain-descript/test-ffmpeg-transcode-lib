import { load, DataType, open, close, define } from "ffi-rs";

open({
  library: "deslib",
  path: "./libdeslib.dylib",
});

const {
  get_eof,
  get_eagain,
  get_strerror,
  init_handler,
  process_frame,
  flush,
  close_handler,
} = define({
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
  },
  process_frame: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.External],
  },
  flush: {
    library: "deslib",
    retType: DataType.I32,
    paramsType: [DataType.External],
  },
  close_handler: {
    library: "deslib",
    retType: DataType.Void,
    paramsType: [DataType.External],
  },
});

const eof = get_eof([]);
const eagain = get_eagain([]);

const strerr = (err: number) => {
  const str = Buffer.alloc(1024);
  const ret = get_strerror([err, str, 1024]);
  if (ret < 0) throw new Error(`Invalid error: ${err}`);
  return str.slice(0, ret).toString();
};

const run = (input: string, output: string) => {
  const handler = init_handler([input, output]);

  try {
    do {
      const ret = process_frame([handler]);
      if (ret === eof || ret === eagain) break;

      if (ret < 0) throw new Error(`Error: ${strerr(ret)}`);
    } while (true);

    flush([handler]);
  } finally {
    close_handler([handler]);
  }
};

run("/tmp/bla.mp4", "/tmp/blo.mp4");
