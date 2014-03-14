var pc = require("ProcessChain");

var obj = new pc.ProcessChain();
//obj.chain({ program: "/bin/ls", arguments: [ "/usr/bin" ] });
obj.chain({ program: "/bin/echo", arguments: [ "foo" ] });
obj.exec(function(data) { console.log(data + "\n"); });
