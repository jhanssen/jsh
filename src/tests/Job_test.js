var Job = require('Job');
var jshNative = require('jsh');
jsh = {
    jshNative: new jshNative.jsh()
};

var job1 = new Job.Job();

var js1 = new Job.JavaScript(function(data) { return data + " fofo hello"; });
var js2 = new Job.JavaScript(function(data) { return data + " final"; });
job1.proc({ program: "/bin/echo", arguments: [ "hello from echo"] }).js(js1).proc({ program: "/bin/grep", arguments: [ "hello" ]}).js(js2).exec(0, function(data) { console.log("hey 1 " + data); });
//job1.proc({ program: "/bin/echo", arguments: [ "hello from echo"] }).js(js1).exec(0, function(data) { console.log("hey 1 " + data); });

var js3 = new Job.JavaScript(function(data) { return "hello from js"; });
var js4 = new Job.JavaScript(function(data) { return data + " fofo"; });
var js5 = new Job.JavaScript(function(data) { return data + " final"; });
var job2 = new Job.Job();
job2.js(js3).js(js4).proc({ program: "/bin/grep", arguments: [ "hello" ]}).js(js5).exec(0, function(data) { console.log("hey 2 " + data); });
