'use strict';
 
var exec   = require('child_process'),
    conf   = require('./_config.js').transcoder,
    util   = require('util'),
    events = require('events'),
    fs     = require('fs'),
    zmq    = require('zmq'),
    os     = require('os');

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


var _runtimeIds = 0;
var _runtimes   = {};

setInterval(function() {
    for (var name in _runtimes) {
        var rt = _runtimes[name];
        if (rt._refs == 0) {
            rt.stop();
            delete _runtimes[name];
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
}

Runtime.prototype.start = function() {
    var self    = this;
    
    this._rid   = _runtimeIds++;
    this._logId = '{cid=' + this._name + ', rid=' + this._rid + '}';

    var logFile = conf.logDir + '/' + this._name + '.log';
    
    this._proc  = exec.execFile(conf.exec, [pullEndpoint, this._rid, logFile],
        {},
        function(error) {
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
    
}

exports.open = function(name) {
    var rt = _runtimes[name];
    if (rt == null) {
        rt = new Runtime(name);
        rt.start();
        _runtimes[name] = rt;
    }
    rt._refs++;
    return new Process(rt);
}

