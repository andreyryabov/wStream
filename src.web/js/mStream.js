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
    this._name2str = {};
    this._sid2str  = {};
    this._isConnected =  false;
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

WStream.prototype.stream = function(name, canvas) {
    if (this._name2str[name]) {
        throw Error('stream alreay open: ' + name);
    }
    var str = new VideoStream(name, canvas);
    this._name2str[name] = str;
    if (this._isConnected) {
        this._sendJson({stream: this._streams[i]});
    }
    return str;
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
        for (var name in self._name2str) {
            self._sendJson({stream: name});
        }
    }
    this._socket.onmessage = function(evt) {
        //console.log('onmessage', evt, evt.data);
        var dec = msgpack.decoder(evt.data);
        var cmd = dec.parse();
        if (cmd == msg.JSON) {
            var json = JSON.parse(dec.parse());
            console.log('received json', json);
            self._handleJson(json);
            return;
        }
        if (cmd == msg.FRAME) {
            var sid   = dec.parse();
            var key   = dec.parse();
            var frame = dec.parse(true);
            var str   = self._sid2str[sid];
            if (str) {
                str._onFrame(key, frame);
            } else {
                console.error('no such stream: ' + sid, self._sids);
            }
            return;
        }
        return console.error('invalid message type', cmd);
    }
}

WStream.prototype._handleJson = function(json) {
    if (json.streams) {
        for (var name in json.streams) {
            var str = this._name2str[name];
            if (!str) {
                console.error('not such stream', name, sid);
                continue;
            }
            var sid = json.streams[name];
            if (!str.sid) {
                str.sid = sid;
                this._sid2str[sid] = str;
                continue;
            }
            if (str.sid != sid) {
                console.error('sid changed', str.sid, sid);
                continue;
            }
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

function VideoStream(name, canvas) {
    this.name    = name;
    this.sid     = null;
    this._canvas = null;
    this._mpeg   = new jsmpeg(canvas);
}

VideoStream.prototype._onFrame = function(isKey, frame) {
    //console.log('stream[' + this.sid + '] ' + this.name + ', isKey ' + isKey + ', frame: ', frame.byteLength);
    this._mpeg.receiveSocketMessage(frame);
}



