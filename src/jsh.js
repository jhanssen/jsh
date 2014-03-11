var net = require('net');

function readObject(obj)
{
}

function unixData(socket, data)
{
    if (socket.jshData === undefined)
        socket.jshData = data;
    else
        socket.jshData = Buffer.concat([socket.jshData, data]);
    for (;;) {
        if (!socket.jshSize) {
            socket.jshSize = socket.jshData.readUInt32LE(0, true);
            if (!socket.jshSize)
                return;
        }
        if (socket.jshData.length - 4 >= socket.jshSize) {
            readObject(socket.jshData.slice(4, socket.jshSize - 4));
            var newLength = socket.jshData.length - (socket.jshSize + 4);
            if (!newLength) {
                socket.jshData = undefined;
                socket.jshSize = 0;
                break;
            }
            var tmp = new Buffer(newLength);
            socket.jshData.copy(tmp, 0, socket.jshSize + 4, newLength);
            socket.jshData = tmp;
            socket.jshSize = 0;
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
server.listen('~/.jsh-socket');
