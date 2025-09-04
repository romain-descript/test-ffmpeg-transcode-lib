"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const ffi_rs_1 = require("ffi-rs");
(0, ffi_rs_1.open)({
    library: "deslib",
    path: "./libdeslib.dylib",
});
const { get_eof, get_eagain, get_strerror, init_handler, process_frame, flush, close_handler, } = (0, ffi_rs_1.define)({
    get_eof: {
        library: "deslib",
        retType: ffi_rs_1.DataType.I32,
        paramsType: [],
    },
    get_eagain: {
        library: "deslib",
        retType: ffi_rs_1.DataType.I32,
        paramsType: [],
    },
    get_strerror: {
        library: "deslib",
        retType: ffi_rs_1.DataType.I32,
        paramsType: [ffi_rs_1.DataType.I32, ffi_rs_1.DataType.U8Array, ffi_rs_1.DataType.I32],
    },
    init_handler: {
        library: "deslib",
        retType: ffi_rs_1.DataType.External,
        paramsType: [ffi_rs_1.DataType.String, ffi_rs_1.DataType.String],
    },
    process_frame: {
        library: "deslib",
        retType: ffi_rs_1.DataType.I32,
        paramsType: [ffi_rs_1.DataType.External],
    },
    flush: {
        library: "deslib",
        retType: ffi_rs_1.DataType.I32,
        paramsType: [ffi_rs_1.DataType.External],
    },
    close_handler: {
        library: "deslib",
        retType: ffi_rs_1.DataType.Void,
        paramsType: [ffi_rs_1.DataType.External],
    },
});
const eof = get_eof([]);
const eagain = get_eagain([]);
const strerr = (err) => {
    const str = Buffer.alloc(1024);
    const ret = get_strerror([err, str, 1024]);
    if (ret < 0)
        throw new Error(`Invalid error: ${err}`);
    return str.slice(0, ret).toString();
};
const run = (input, output) => {
    const handler = init_handler([input, output]);
    try {
        do {
            const ret = process_frame([handler]);
            if (ret === eof || ret === eagain)
                break;
            if (ret < 0)
                throw new Error(`Error: ${strerr(ret)}`);
        } while (true);
        flush([handler]);
    }
    finally {
        close_handler([handler]);
    }
};
run("/tmp/bla.mp4", "/tmp/blo.mp4");
