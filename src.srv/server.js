'use strict';

var ws = require('ws'),
    pc = require('./process.js');

function Handler(socket) {
    var self = this;
    this._socket = socket;
    this._socket.on('message', function(data) {
        console.log('message', data);
        try {
            var msg = JSON.parse(data);
            if (msg.group) {
                self._group(msg.group);
            }
            if (msg.stream) {
                self._stream(msg.stream);
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
}

Handler.prototype._group = function(cid) {
    if (this._proc) {
        throw new Error('process already open');
    }
    this._proc = pc.open(cid);
    var self = this;
    this._proc.on('frame', function(sid, frame) {
        self._socket.send(JSON.stringify({frame:frame, sid:sid}));
    });
}

Handler.prototype._stream = function(sid) {
    if (!this._proc) {
        throw new Error('invalid state, process not oopen');
    }
    this._proc.stream(sid);
    this._socket.send(JSON.stringify({streams: this._proc.streams}));
}

var processes = {};
var streams = {};

var wsServer = new ws.Server({port: 8105});

wsServer.on('connection', function(socket) {
    new Handler(socket);
});