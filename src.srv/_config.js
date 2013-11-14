'use strict';

exports.transcoder = {
    exec:            '/Users/RKK2/Library/Developer/Xcode/DerivedData/wStream-ggfijxbmdovottgyborpkszyibej/Build/Products/Debug/wStream',
    logDir:          'logs',
    interface:       'lo0',
    restartInterval:  5000,
    refcheckInterval: 5000,
    publishers:      ['tcp://127.0.0.1:58127']
}

exports.consts = {
    msg: {
        HALT:  1,
        JSON:  2,
        PING:  3,
        FRAME: 4
    }
}
