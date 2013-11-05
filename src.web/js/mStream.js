'use strict';

var msg = {
    HALT:  1,
    JSON:  2,
    PING:  3,
    FRAME: 4
};

function WStream(url) {
    this._url = url;
    this._group = null;
    this._streams = [];
    this._sids    = {};
    this._isConnected = false;
}

WStream.prototype.group = function(g) {
    if (this._group && this._group != g) {
        console.error('group is already set');
        return;
    }
    this._group = g;
    if (!this._isConnected) {
        return;
    }
    this._sendJson({group: this._group});
}

WStream.prototype.stream = function(s) {
    for (var i = 0; i < this._streams.length; i++ ) {
        if (this._streams[i] == s) {
            console.error('stream: ' + s + ', already on');
            return;
        }
    }
    this._streams.push(s);
    if (!this._isConnected) {
        return;
    }
    this._sendJson({stream: this._streams[i]});
}

WStream.prototype.connect = function() {
    var self = this;
    console.log('wstream connecting to: ' + this._url);
    this._socket = new WebSocket(this._url);
    this._socket.binaryType = 'arraybuffer';
    this._socket.onopen = function() {
        console.log('wstream connected', self._socket);
        self._isConnected = true;
        if (self._group) {
            self._sendJson({group: self._group});
        }
        if (self._streams.length > 0) {
            for (var i = 0; i < self._streams.length; i++ ) {
                self._sendJson({stream: self._streams[i]});
            }
        }
    }
    this._socket.onmessage = function(evt) {
        console.log('onmessage', evt, evt.data);
        var dec = msgpack.decoder(evt.data);
        var cmd = dec.parse();
        if (cmd == msg.JSON) {
            var json = JSON.parse(dec.parse());
            console.log('received json', json);
            self._handleJson(json);
            return;
        }
        if (cmd == msg.FRAME) {
            var sid = dec.parse();
            var frame = dec.parse();
            var name = self._sids[sid];
            if (name) {
                console.log('frame', sid, name, frame);
            } else {
                console.error('stream does not exists for sid: ' + sid, self._sids);
            }
            return;
        }
        return console.error('invalid message type', cmd);
    }
}

WStream.prototype._handleJson = function(json) {
    if (json.streams) {
        for (var name in json.streams) {
            this._sids[json.streams[name]] = name;
        }
        return;
    }
    console.log('invalid json', json);
}

WStream.prototype._sendJson = function(obj) {
    console.log('sendJson', obj);
    this._socket.send(JSON.stringify(obj));
}

WStream.prototype.close = function() {
    console.log('close wstream, ' + this._logId);
    this._socket.close();
}



