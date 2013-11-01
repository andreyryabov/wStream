'use strict';
 
var exec   = require('child_process'),
    conf   = require('./_config.js').transcoder,
    util   = require('util'),
    events = require('events'),
    fs     = require('fs'),
    zmq    = require('zmq'),
    os     = require('os'),
    mpack  = require('./mpack.js');


var MSG = '_msg_';
var UID = 'uuid';

var MSG_HALT = 1;
var MSG_JSON = 2;

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

var _runtimeIds   = 10000;
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
    if (dat.get() == MSG_JSON) {
        var obj = JSON.parse(dat.get());
        handleJson(obj);
    }
});

function handleJson(obj) {
    var msg = obj[MSG], uid = obj[UID];
    if (msg && UID) {
        var rt = _id2runtime[uid];
        if (!rt) {
            console.error('runtime not found id: ' + uid);
            return;
        }
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
    
    this._pushSock.send('HALT'); // TODO: send HALT..
    
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
        function(error) {
            delete _id2runtime[self._rid];
            console.log('process runtime stopped, ' + self._logId);
            if (self._stopped) {
                return;
            }
            if (error) {
                console.error('error in process, ' + self._logId, error);
            }
            setTimeout(function() { self.start(); }, conf.restartInterval);
        });
    this._proc.unref();
    
    console.log('process runtime start, ' + self._logId + ', log file: ' + logFile);
}

Runtime.prototype.msg_started = function(obj) {
    console.log('started ', obj);

    this._pushSock = zmq.socket('push'); // this -> transcoder commands
    this._subSock  = zmq.socket('sub');  // transcoder media   -> this
    
    this._pushSock.connect('tcp://' + pullAddr + ':' + obj.command.port);
    this._subSock .connect('tcp://' + pullAddr + ':' + obj.media.port);
    
var sock = this._pushSock;
setInterval(function() {
//    console.log('send hello...' + pullAddr + ':' + obj.command.port);
//    sock.send('hello!!!!'); // TODO:..
}, 2500);

}

function Process(rt) {
    var self = this;
    this._runtime = rt;
    this._frameListener = function(frame) { self.emit('frame', frame); };
    rt.on('frame', this._frameListener);
}

util.inherits(Process, events.EventEmitter);

Process.prototype.close = function() {
    this._runtime._refs--;
    this._runtime.removeListener('frame', this._frameListener);
    this.removeAllListeners();
}

Process.prototype.stream = function(stream) {
    //TODO:....
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

