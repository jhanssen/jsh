var net = require('net');
var cmd = undefined;
var funcs = {};
var pendingClosed = [];
var pidPad = "00000000000000000000";

function writePid(pid) {
    var str = "" + pid;
    cmd.write(pidPad.substring(0, pidPad.length - str.length) + str);
}

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
            if (cmd) {
                writePid(this._pid);
            } else {
                pendingClosed.push(this._pid);
            }
            socket.end();
            funcs[socket] = undefined;
            this._closed = true;
        },
        stdout: function(data) {
            console.log("writing " + data);
            socket.write("" + data);
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
    if (socket == cmd)
        return;

    if (funcs[socket]) {
        funcs[socket].io._call("stdin", data.toString());
        return;
    }

    console.log(data.length);
    if (socket.jshData === undefined)
        socket.jshData = data;
    else
        socket.jshData = Buffer.concat([socket.jshData, data]);
    for (;;) {
        if (!socket.jshPid) {
            socket.jshPid = socket.jshData.readInt32LE(0, true);
            if (!socket.jshPid)
                return;
            console.log("pid " + socket.jshPid);
        }
        if (socket.jshPid == -1) {
            // command socket
            cmd = socket;
            for (var pid in pendingClosed) {
                console.log("writing pending pid " + pendingClosed[pid]);
                writePid(pendingClosed[pid]);
            }
            pendingClosed = [];
            return;
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
                socket.jshSize = 0;
                break;
            }
            if (read) {
                var tmp = new Buffer(newLength);
                socket.jshData.copy(tmp, 0, socket.jshSize + 8, newLength);

                funcs[socket].io._call("stdin", tmp.toString());
            }
            socket.jshData = undefined;
            socket.jshSize = undefined;
        } else {
            break;
        }
    }
}

function unixServer(socket)
{
    socket.jshData = new Buffer(0);
    socket.on("data", function(data) { unixData(socket, data); });
}

var server = net.createServer(unixServer);
server.listen('/home/jhanssen/.jsh-socket');
