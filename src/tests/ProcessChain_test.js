var pc = require("ProcessChain");
var jshNative = require('jsh');
jsh = {
    jshNative: new jshNative.jsh()
};

var obj1 = new pc.ProcessChain(jshNative, 0);
obj1.chain({ program: "/bin/ls", arguments: [ "/bin" ]}).chain({ program: "/bin/grep", arguments: [ "bz" ]});
obj1.exec(function(data) { console.log(JSON.stringify(data) + "\n"); });

var obj2 = new pc.ProcessChain(jshNative, 0);
obj2.chain({ program: "/bin/bash", arguments: [ "-c", "echo $FOO" ], cwd: "/home", environment: [ "FOO=bar" ] });
obj2.exec(function(data) { console.log(JSON.stringify(data) + "\n"); });

var obj3 = new pc.ProcessChain(jshNative, 0);
obj3.chain({ program: "/bin/grep", arguments: [ "foob" ] }).write("foobar baz").exec(function(data) { console.log("grep " + JSON.stringify(data) + "\n"); });
