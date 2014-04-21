module.exports = {
    bar: function(sock, arg) {
        service.sendEvent(sock, {func:'bar', arg: arg});
        console.log("bar", service.connections.length);
    },
    foo: function(sock, arg) {
        service.sendEvent(sock, {func:'foo', arg: arg});
        console.log("foo", service.connections.length);
    },
    balls: function(sock, arg) {
        service.sendEvent(sock, {func:'balls', arg: arg});
        console.log("balls", service.connections.length);
    }
};

// if (typeof process !== 'undefined') {
//     for (var argc=1; argc<process.argv.length; ++argc) {
//         console.log("GOT ARGS HERE", process.argv[argc]);
//     }
// }

// (function(arg) {
//     console.log("GOT SHIT SHIT SHIT", arg);
// })(this);
