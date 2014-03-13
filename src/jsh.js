var net = require('net');
var fs = require('fs');

if (!String.prototype.startsWith) {
    Object.defineProperty(String.prototype, 'startsWith', {
        enumerable: false,
        configurable: false,
        writable: false,
        value: function (searchString, position) {
            position = position || 0;
            return this.indexOf(searchString, position) === position;
        }
    });
}

if (!String.prototype.endsWith) {
    Object.defineProperty(String.prototype, 'endsWith', {
        enumerable: false,
        configurable: false,
        writable: false,
        value: function (searchString, position) {
            position = position || this.length;
            position = position - searchString.length;
            var lastIndex = this.lastIndexOf(searchString);
            return lastIndex !== -1 && lastIndex === position;
        }
    });
}

if (!String.prototype.contains) {
    Object.defineProperty(String.prototype, 'contains', {
        enumerable: false,
        configurable: false,
        writable: false,
        value: function (needle) { return this.indexOf(needle) !== -1; }
    });
}

var jsh = (function() {
    var funcs = {};
    var logFile;
    try {
        logFile = fs.openSync("nodejs.log", 'w');
    } catch (err) {}
    var logToConsoleOut = process.env.JSH_LOG_TO_CONSOLE_OUT == "1";

    function readObject(pid, obj, socket)
    {
        console.log(obj.toString());

        var io = {
            on: function(msg, cb) {
                this._ons[msg] = cb;
            },
            close: function() {
                console.log("close " + this._pid);
                if (this._closed)
                    return;
                socket.end();
                funcs[socket] = undefined;
                this._closed = true;
            },
            stdout: function(data) {
                console.log("writing " + data);
                socket.write("" + data + "\n");
            },
            async: undefined,

            _pid: pid,
            _closed: false,
            _ons: {},
            _socket: socket,
            _call: function(msg, data) {
                if (typeof this._ons[msg] === "function")
                    this._ons[msg](data);
            }
        };

        try {
            var func = eval("(function(io) {" + obj.toString() + "})");
        } catch (e) {
            console.log(e);
        }
        if (typeof func === "function") {
            funcs[socket] = { io: io, func: func };
            try {
                func(io);
            } catch(e) {
                funcs[socket] = undefined;
                return false;
            }
            return true;
        }
        return false;
    }

    function unixData(socket, data)
    {
        if (funcs[socket]) {
            jsh.log("got data 1 " + data.length);
            funcs[socket].io._call("stdin", data.toString());
            return;
        }
        jsh.log("hello?");

        console.log(data.length);
        if (socket.jshData === undefined)
            socket.jshData = data;
        else
            socket.jshData = Buffer.concat([socket.jshData, data]);

        if (!socket.jshPid) {
            socket.jshPid = socket.jshData.readInt32LE(0, true);
            if (!socket.jshPid)
                return;
            console.log("pid " + socket.jshPid);
        }
        if (socket.jshPid && !socket.jshSize) {
            socket.jshSize = socket.jshData.readUInt32LE(4, true);
            if (!socket.jshSize)
                return;
            console.log("size " + socket.jshSize);
        }
        if (socket.jshData.length - 8 >= socket.jshSize) {
            var read = readObject(socket.jshPid, socket.jshData.slice(8, socket.jshSize + 8), socket);
            var newLength = socket.jshData.length - (socket.jshSize + 8);
            if (!newLength) {
                socket.jshData = undefined;
                socket.jshSize = undefined;
                return;
            }
            if (read) {
                var tmp = new Buffer(newLength);
                socket.jshData.copy(tmp, 0, socket.jshSize + 8, newLength + socket.jshSize + 8);

                jsh.log("got data 2 " + data.length);
                funcs[socket].io._call("stdin", tmp.toString());
            }
            socket.jshData = undefined;
            socket.jshSize = undefined;
        }
    }

    function unixServer(socket)
    {
        socket.jshData = new Buffer(0);
        socket.on("data", function(data) { unixData(socket, data); });
    }

    var home = (process.env.HOME || process.env.HOMEPATH || process.env.USERPROFILE);
    if (home[home.length - 1] != '/')
        home += '/';
    var socketFile = home + ".jsh-socket";
    for (var i=2; i<process.argv.length; ++i) {
        var arg = process.argv[i];
        if (arg.startsWith("--socket-file=") && arg.length > 14)
            socketFile = arg.substr(14);
    }
    var server = net.createServer(unixServer);
    try {
        fs.unlinkSync(socketFile);
    } catch(err) {}
    server.listen(socketFile);
    var rcFiles = [ "/etc/jsh.js", home + ".jshrc.js" ];
    return {
        get rcFiles() { return rcFiles; },
        get socketFile() { return socketFile; },
        get logFile() { return logFile; },
        get logToConsoleOut() { return logToConsoleOut; },
        log: function log() {
            if (jsh.logFile || jsh.logToConsoleOut) {
                var t = "";
                for (var i=0; i<arguments.length; ++i) {
                    if (t.length)
                        t += ' ';
                    if (typeof arguments[i] !== 'string') {
                        try {
                            t += JSON.stringify(arguments[i]);
                        } catch (err) {
                            t += arguments[i];
                        }
                    } else {
                        t += arguments[i];
                    }
                }
                if (t.length) {
                    if (t[t.length - 1] != '\n')
                        t += '\n';
                    fs.write(logFile, new Buffer(t), 0, t.length);
                }
            }
        }
        // complete: function complete() {

        // }
    };
})();

for (var i=0; i<jsh.rcFiles.length; ++i) {
    try {
        var code = fs.readFileSync(jsh.rcFiles[i], 'utf8');
        if (code)
            eval(code);
    } catch(err) {}
}
