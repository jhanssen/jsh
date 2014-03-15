var Job = require('Job');

var job1 = new Job.Job();

var js1 = new Job.JavaScript(function(data) { return "hello from js"; });
var js2 = new Job.JavaScript(function(data) { return data + " fofo"; });
var js3 = new Job.JavaScript(function(data) { return data + " final"; });
job1.proc({ program: "/bin/echo", arguments: [ "hello from echo"] }).js(js2).proc({ program: "/bin/grep", arguments: [ "hello" ]}).js(js3).exec(function(data) { console.log("hey " + data); });

var job2 = new Job.Job();
job2.js(js1).js(js2).proc({ program: "/bin/grep", arguments: [ "hello" ]}).js(js3).exec(function(data) { console.log("hey " + data); });
