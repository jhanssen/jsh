function match(opt, arg) {
    var res = new RegExp(opt + "=(.*)").exec(arg);
    if (res)
        return res[1];
    return undefined;
}

if (typeof process === 'undefined')
    process.exit(1);

var Service = require('Service');

var modulePath;
var socketFile;
for (var i=1; i<process.argv.length; ++i) {
    var arg = process.argv[i];
    var res = match("--module-path", arg);
    if (res) {
        modulePath = res;
        continue;
    }
    res = match("--socket-file", arg);
    if (res) {
        socketFile = res;
        continue;
    }
}
if (!modulePath || !socketFile || !Service.launchService(modulePath, socketFile)) {
    process.exit(2);
}
