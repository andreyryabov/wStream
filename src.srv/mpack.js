'use strict';

var msgpack = require('msgpack');

function Unpacker(buf) {
    this._buf = buf;    
}

Unpacker.prototype.has = function() {
    return _buf && _buf.length > 0;
}

Unpacker.prototype.get = function() {
    var msg = msgpack.unpack(this._buf);
    if (!msg) {
        throw new Error('buffer underflow');
    }
    if (msgpack.unpack.bytes_remaining > 0) {
        this._buf = this._buf.slice(
                    this._buf.length - msgpack.unpack.bytes_remaining,
                    this._buf.length);
    } else {
        this._buf = null;
    }
    return msg;
}

function Packer() {
    this._bufs = [];
}

Packer.prototype.put = function(obj) {
    this._bufs.push(msgpack.pack(obj));
}

Packer.prototype.buffer = function() {
    return Buffer.concat(this._bufs);
}

exports.unpacker = function(buf) { return new Unpacker(buf); }
exports.packer   = function(buf) { return new Packer(buf);   }

