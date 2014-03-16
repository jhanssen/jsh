var rl = require('ReadLine');


var read = new rl.ReadLine(function(data) {
    if (data === undefined) {
        read.cleanup();
        process.exit();
    }
    console.log("got '" + data + "'");
});
