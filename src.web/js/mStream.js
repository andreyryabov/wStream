

function WStream(url) {
    this._url = url;
    this._group = null;
    this._streams = [];
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
            console.error('stream: ' + s + ', already in');
            return;
        }
    }
    if (!this._isConnected) {
        return;
    }
    this._streams.push(s);
    this._sendJson({stream: this._streams[i]});
}

WStream.prototype.connect = function() {
    var self = this;
    console.log('wstream connecting to: ' + this._url);
    this._socket = new WebSocket(this._url);
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
}

WStream.prototype._sendJson = function(obj) {
    console.log('sendJson', obj);
    this._socket.send(JSON.stringify(obj));
}

WStream.prototype.close = function() {
    console.log('close wstream, ' + this._logId);
    this._socket.close();
}



