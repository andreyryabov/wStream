'use strict';

var ws    = require('ws'),
    fs    = require('fs'),
    pc    = require('./transcoder.js'),
    msg   = require('./_config.js').consts.msg,
    mpack = require('./mpack.js');

function Handler(socket) {
    var self = this;
    this._dumpFiles = null;
    this._first  = true;
    this._socket = socket;
    this._socket.on('message', function(data) {
        console.log('message', data);
        try {
            var msg = JSON.parse(data);
            if (msg.group) {
                self._group(msg.group, msg.params);
            }
            if (msg.stream) {
                self._stream(msg.stream, msg.params);
            }
        } catch(e) {
            console.error('failed while handling message', data, e, e.stack);
            self._socket.close();
        }
    });
    this._socket.on('close', function() {
        console.log('socket closed');
        self._cleanup();
    });
}

Handler.prototype._cleanup = function() {
    if (this._proc) {
        this._proc.close();
        this._proc = null;
    }
    if (this._dumpFiles) {
        for (var sid in this._dumpFiles) {
            fs.closeSync(this._dumpFiles[sid]);
        }
    }
}

Handler.prototype._message = function(type) {
    var pack = mpack.packer();
    pack.put(type);
    if (type == msg.JSON) {
        pack.put(JSON.stringify(arguments[1]));
    } else {
        for (var i = 1; i < arguments.length; i++ ) {
            pack.put(arguments[i]);
        }
    }
    this._socket.send(pack.buffer());
}

Handler.prototype._group = function(cid, params) {
    if (this._proc) {
        throw new Error('process already open');
    }
    this._proc = pc.open(cid, params);
    var self = this;
    this._proc.on('frame', function(sid, key, frame) {
        if (self._dumpFiles) {
            var fbuf = new Buffer(frame);
            var file = self._dumpFiles[sid];
            if (!file) {
                var fName = 'sid_' + sid + '.mpg';
                file = fs.openSync(fName, 'a');
                fs.ftruncateSync(file);
                self._dumpFiles[sid] = file;
            }
            fs.write(file, fbuf, 0, fbuf.length);
        }
        if (self._first) {
            if (!key) {
                return;
            }
            self._first = false;
        }
        var pack = mpack.packer();
        pack.put(msg.FRAME);
        pack.put(sid);
        pack.put(key);
        pack.put(frame);
        var data = pack.buffer();
        self._socket.send(data);
    });
}

Handler.prototype._stream = function(sid, params) {
    if (!this._proc) {
        throw new Error('invalid state, process not oopen');
    }
    this._proc.stream(sid, params);
    this._message(msg.JSON, {streams: this._proc.streams});
}

var processes = {};
var streams   = {};

var wsServer = new ws.Server({port: 8105});

wsServer.on('connection', function(socket) {
    new Handler(socket);
});
