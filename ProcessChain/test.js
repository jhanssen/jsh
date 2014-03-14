var pc = require("ProcessChain");

var obj1 = new pc.ProcessChain();
obj1.chain({ program: "/bin/ls", arguments: [ "/bin" ]}).chain({ program: "/bin/grep", arguments: [ "bz" ]});
var obj2 = new pc.ProcessChain();
obj2.chain({ program: "/bin/bash", arguments: [ "-c", "echo $FOO" ], cwd: "/home", environment: [ "FOO=bar" ] });

obj1.exec(function(data) { console.log(data + "\n"); });
obj2.exec(function(data) { console.log(data + "\n"); });
