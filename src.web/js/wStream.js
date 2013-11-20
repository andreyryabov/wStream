(function(exports){'use strict';

var msg = {
    HALT:  1,
    JSON:  2,
    PING:  3,
    FRAME: 4,
    PAUSE: 6
};

var RECONNECT_TIMEOUT = 3000;

function merge() {
    var result = {};
    for (var i = 0; i < arguments.length; i++ ) {
        for (var k in arguments[i]) {
            result[k] = arguments[i][k];
        }
    }
    return result;
}

function Traffic(interval/* = 5*/, time/* = 60*/) {
    this._interval  = (interval || 5);
    this._maxLength = time / this._interval;
    this._current = 0;
    this._history = [];
    this._timer   = setInterval(this._onTimer.bind(this), this._interval * 1000);
}

Traffic.prototype._onTimer = function() {
    this._history.push(this._current);
    this._current = 0;
    while (this._history.length >= this._maxLength) {
        this._history.shift();
    }
}

Traffic.prototype.update = function(bytes) {
    this._current += bytes;
}

Traffic.prototype.stop = function() {
    clearInterval(this._timer);
}

Traffic.prototype.history = function() {
    var res = this._history.slice(0);
    for (var i = 0; i < res.length; i++ ) {
        res[i] /= this._interval;
    }
    return res;
}

Traffic.prototype.average = function() {
    var result = 0;
    for (var i = 0; i < this._history.length; i++ ) {
        result = this._history[i];
    }
    return result / (this._history.length * this._interval);
}

function WStream(url) {
    this._url         = url;
    this._group       = null;
    this._name2str    = {};
    this._sid2str     = {};
    this._socket      = false;
    this._isConnected = false;
    this._closed      = false;
    this._reconnect   = null;
    this._traffic     = new Traffic(3, 30);
}

WStream.prototype.close = function() {
    this._closed = true;
    if (this._timer) {
        this._timer.close();
        this._timer = null;
    } 
    if (this._socket) {
        this._socket.close();
        this._socket = null;
    }
    if (this._reconnect) {
        clearTimeout(this._reconnect);
        this._reconnect = null;
    }
}

WStream.prototype.group = function(g, params) {
    this._groupParams = params || {width:144, height:108, fps:7, keyframeIntervalMs:25000};
    if (this._group && this._group != g) {
        console.error('group is already set');
        return;
    }
    this._group = g;
    if (this._isConnected) {
        this._sendJson({group: this._group, params:this._groupParams});
    }
}

WStream.prototype.stream = function(name, canvas, params) {
    if (!this._group) {
        throw new Error('group is not open');
    }
    if (!name) {
        throw new Error('strem name must not be null');
    }
    if (!canvas) {
        throw new Error('canvas must not be null');
    }
    if (this._name2str[name]) {
        throw new Error('stream alreay open: ' + name);
    }
    var str = new VideoStream(name, canvas);
    str._params = merge(this._groupParams, params);    
    this._name2str[name] = str;
    if (this._isConnected) {
        this._sendJson({stream: name, params:str._params});
    }
    return str;
}

WStream.prototype._connect = function() {
    var self = this;
    if (this._closed) {
        return;
    }
    console.log('wstream connecting to: ' + this._url);
    this._socket = new WebSocket(this._url);
    this._socket.binaryType = 'arraybuffer';
    this._socket.onopen = function() {
        console.log('wstream connected', self._socket);
        self._isConnected = true;
        if (self._group) {
            self._sendJson({group: self._group, params: self._groupParams});
        }
        for (var name in self._name2str) {
            self._sendJson({stream: name, params: self._name2str[name]._params});
        }
    }
    this._socket.onmessage = function(evt) {
        self._traffic.update(evt.data.byteLength);
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
        if (cmd == msg.PAUSE) {
            var sid = dec.parse();
            var str = self._sid2str[sid];
            if (str) {
                str._pause();
            } else {
                console.error('no such stream: ' + sid, self._sids);
            }
            return;
        }
        return console.error('invalid message type', cmd);
    }
    this._socket.onclose = function(evt) {
        if (self._closed) {
            return;
        }
        console.log('socket error', evt);
        self._isConnected = false;
        if (!self._closed) {
            setTimeout(function() { self._connect(); }, RECONNECT_TIMEOUT);
        }
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
            var streamData = json.streams[name];
            if (!str.sid) {
                str._init(streamData.sid, streamData.width, streamData.height);
                this._sid2str[streamData.sid] = str;
                continue;
            }
            if (str.sid != streamData.sid) {
                console.error('sid changed', str.sid, streamData.sid);
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
    this.name     = name;
    this.sid      = null;
    this.width    = null;
    this.height   = null;
    this._params  = null;
    this._canvas  = canvas;
    this._mpeg    = null;
    this.onStart  = null; // event on first key frame.
    this.onStop   = null;
    this.started  = false;
    this.playing  = false;
    this._frameTs = 0;
}

VideoStream.prototype._init = function(sid, width, height) {
    this.sid    = sid;
    this.width  = width;
    this.height = height;
    this._mpeg  = new MPEGDecoder(this._canvas, width, height);
    this._canvas.width  = width;
    this._canvas.height = height;
}

VideoStream.prototype._onFrame = function(isKey, frame) {
    if (!this.started && isKey) {
        this.started = true;
    }
    this._mpeg.decodeFrame(frame);
    this._frameTs = new Date().getTime();
    if (this.started) {
        if (!this.playing) {
            this.playing = true;
            console.log('fire video.onStart: ' + this.name);
            if (this.onStart) {
                this.onStart(this._canvas);
            }
        }
    }
}

VideoStream.prototype._pause = function() {
    if (!this.playing) {
        return;
    }
    this.playing = false;
    console.log('fire video.onStop: ' + this.name);
    if (this.onStop) {
        this.onStop();
    }
}

exports.wconnect = function(url) {
    var c = new WStream(url);
    c._connect();
    return c;
}

})(window);

