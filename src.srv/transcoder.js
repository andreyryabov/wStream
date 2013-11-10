'use strict';
 
var exec   = require('child_process'),
    util   = require('util'),
    events = require('events'),
    fs     = require('fs'),
    zmq    = require('zmq'),
    os     = require('os'),
    proc   = require('process'),
    conf   = require('./_config.js').transcoder,
    msg    = require('./_config.js').consts.msg,
    mpack  = require('./mpack.js');

    
var MSG = '_msg_';

function getAddressByInterface(name) {
    var iface = os.networkInterfaces()[name];
    if (!iface) {
        throw new Error('inteface not found: ' + name);
    }
    for (var i = 0; i < iface.length; i++ ) {
        if (iface[i].family == 'IPv4') {
            return iface[i].address;
        }
    }
    throw new Error('interface does not have ip4 addres: ' + name);
}

var _runtimeIds   = Math.floor((Math.random() * 1000000) + 1); 
var _id2runtime   = {};
var _name2runtime = {};

setInterval(function() {
    for (var id in _id2runtime) {
        var rt = _id2runtime[id];
        if (rt._refs == 0) {
            rt.stop();
            delete _name2runtime[rt._name];
        }
    }
}, conf.refcheckInterval);


var pullSock = zmq.socket('pull');
pullSock.bindSync('tcp://*:*');

var epRx     = /(\w{2,8}):\/\/([\d\w\.\/]+|\*):(\d+|\*)/;
var lastEp   = pullSock.getsockopt(zmq.ZMQ_LAST_ENDPOINT);
var rxRes    = epRx.exec(lastEp);

var pullAddr = getAddressByInterface(conf.interface);
var pullPort = parseInt(rxRes[3]);
var pullEndpoint = 'tcp://' + pullAddr + ':' + pullPort;

console.log('pull endpoint: ' + pullEndpoint);

pullSock.on('message', function(data) {
    var dat = mpack.unpacker(data);
    var cmd = dat.get();    
    var rid = dat.get();
    var  rt = _id2runtime[rid];
    if (!rt) {
        console.error('command socket, runtime not found id: ' + rid);
        return;
    }    
    if (cmd == msg.JSON) {
        var obj = JSON.parse(dat.get());
        handleJson(rt, obj);
        return;
    }
    console.error('command socket, invalid message: ' + cmd);
});

function handleJson(rt, obj) {
    var msg = obj[MSG];
    if (msg) {
        var handler = rt['msg_' + msg];
        if (!handler) {
            console.error('runtime handler not found for: ' + msg);
            return;
        }
        handler.apply(rt, [obj]);
        return;
    }
    console.error('invalid json message from transcoder', msg);
}

function Runtime(name) {
    this._name    = name;
    this._stopped = false;
    this._proc    = null;
    this._logId   = name;
    this._refs    = 0;
    this._pingTm  = null;
    this._streams = {};
    this._sidsGen = 1;
}

util.inherits(Runtime, events.EventEmitter);

Runtime.prototype.stop = function() {
    console.log('stop process runtime, ' + this._logId);
    this._stopped = true;
    this.removeAllListeners();
    if (this._proc) {
        var proc = this._proc;
        this._proc = null;
        setTimeout(function() { proc.kill('SIGKILL'); }, 3000);
    }
    
    this.message(msg.HALT);
    if (this._pingTm) {
        clearInterval(this._pingTm);
        this._pingTm = null;
    }
    if (this._pushSock) {
        this._pushSock.close();
        this._pushSock = null;
    }
    if (this._subSock) {
        this._subSock.close();
        this._subSock = null;
    }
}

Runtime.prototype.start = function() {
    var self    = this;
    var logFile = conf.logDir + '/' + this._name + '.log';
    
    this._rid   = _runtimeIds++;
    this._logId = '{cid=' + this._name + ', rid=' + this._rid + '}';

    _id2runtime[this._rid] = this;
    
    this._proc  = exec.execFile(conf.exec, [pullEndpoint, this._rid, logFile],
        {},
        function(error, stdOut, stdErr) {
            delete _id2runtime[self._rid];
            console.log('process runtime stopped, ' + self._logId);
            if (self._stopped) {
                return;
            }
            if (error) {
                console.error('error in process, ' + self._logId, error, stdOut, stdErr);
            }
            setTimeout(function() { self.start(); }, conf.restartInterval);
            clearInterval(self._pingTm);
        });
    this._proc.unref();
    
    console.log('process runtime start, ' + self._logId + ', log file: ' + logFile);
}

Runtime.prototype.msg_started = function(obj) {
    console.log('started ', obj);
    
    this._pushSock = zmq.socket('push'); // this -> transcoder commands
    this._subSock  = zmq.socket('sub');  // transcoder media -> this
    
    this._pushSock.connect('tcp://' + pullAddr + ':' + obj.command.port);
    this._subSock .connect('tcp://' + pullAddr + ':' + obj.media.port);
    
    for (var name in this._streams) {
        var sid = this._streams[name];
        this.message(msg.JSON, {stream:{name:name,sid:sid}});
    }
    
    var self = this;
    this._pingTm = setInterval(function() {
        self.message(msg.PING, self._rid);
    }, 10000);
    
    this._subSock.subscribe('');
    this._subSock.on('message', function(data) {
        var dat = mpack.unpacker(data);
        var cmd = dat.get();
        if (cmd == msg.FRAME) {
            var sid   = dat.get();
            var key   = dat.get();
            var frame = dat.get();
            self.emit('frame', sid, key, frame);
            return;
        }
        console.error('media socket, invalid command', cmd);
    });
}

Runtime.prototype.message = function(type) {
    if (!this._pushSock) {
        return;
    }
    var pack = mpack.packer();
    pack.put(type);
    if (type == msg.JSON) {
        console.log('runtime sends json to the process', arguments[1]);
        pack.put(JSON.stringify(arguments[1]));
    } else {
        for (var i = 1; i < arguments.length; i++ ) {
            pack.put(arguments[i]);
        }
    }
    this._pushSock.send(pack.buffer());
}

Runtime.prototype.stream = function(stream) {
    var sid = this._streams[stream];
    if (sid) {
        return sid;
    }
    sid = this._sidsGen++;
    this._streams[stream] = sid;
    this.message(msg.JSON, {stream:{name:stream,sid:sid}});
    proc.nextTick(function() {
        //TODO:. request
        console.log('TODO: request keyframe for stream: ' + stream);        
    });
    return sid;
}

/**
 * Process instance.
 */
function Process(rt) {
    var self      = this;
    this.streams  = {};
    this.sids     = {};
    this._runtime = rt;
    this._frameListener = function(sid, key, frame) {
        if (self.sids[sid]) {
            self.emit('frame', sid, key, frame);
        }
    };
    rt.on('frame', this._frameListener);
}

util.inherits(Process, events.EventEmitter);

Process.prototype.close = function() {
    this._runtime._refs--;
    this._runtime.removeListener('frame', this._frameListener);
    this.removeAllListeners();
}

Process.prototype.stream = function(stream) {
    var sid = this._runtime.stream(stream);
    this.streams[stream] = sid;
    this.sids[sid] = stream;
}

exports.open = function(name) {
    var rt = _name2runtime[name];
    if (rt == null) {
        rt = new Runtime(name);
        rt.start();
        _name2runtime[name] = rt;
    }
    rt._refs++;
    return new Process(rt);
}

