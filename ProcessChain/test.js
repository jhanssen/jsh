var pc = require("ProcessChain");

var obj1 = new pc.ProcessChain();
obj1.chain({ program: "/bin/ls", arguments: [ "/bin" ]}).chain({ program: "/bin/grep", arguments: [ "bz" ]});
obj1.end(function(data) { console.log(data + "\n"); });

var obj2 = new pc.ProcessChain();
obj2.chain({ program: "/bin/bash", arguments: [ "-c", "echo $FOO" ], cwd: "/home", environment: [ "FOO=bar" ] });
obj2.end(function(data) { console.log(data + "\n"); });

var obj3 = new pc.ProcessChain();
obj3.chain({ program: "/bin/grep", arguments: [ "foob" ] }).write("foobar baz").end(function(data) { console.log("grep " + data + "\n"); });
