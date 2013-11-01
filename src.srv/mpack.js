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

exports.unpacker = function(buf) { return new Unpacker(buf); }
