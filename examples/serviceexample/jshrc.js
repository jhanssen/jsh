// jsh.logEnabled = true;

var Service = require('Service');

Service.registerService('foobar', function(service) {
    if (service) {
        foobar = service;
        foobar.addEventListener(function(event) {
            console.log("Got event from", service.name, event);
        });
    }
    // console.log("GOT SERVICE", service);
    var foobars = 10;
    function next()
    {
        if (--foobars > 0) {
            service.foo(foobars);
            setTimeout(next, 1000);
        }
    }
    next();
});
