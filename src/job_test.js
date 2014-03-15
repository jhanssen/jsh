var Job = require('Job');

var job = new Job.Job();

var js1 = new Job.JavaScript(function(data) { return "hello!"; });
var js2 = new Job.JavaScript(function(data) { return data + " fofo"; });
job.js(js1).js(js2).proc({ program: "/bin/grep", arguments: [ "hello" ]}).exec(function(data) { console.log("hey " + data); });
