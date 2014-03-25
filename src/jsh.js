var rl = require('ReadLine');
var pc = require('ProcessChain');
var Job = require('Job');
var Tokenizer = require('Tokenizer');
var jsh = require('jsh');
var path = require('path');
var fs = require('fs');
global.jsh = {
    jshNative: new jsh.native.jsh(),
    Job: Job
};
var read;

function isFunction(token)
{
    if (token[0].type === Tokenizer.COMMAND) {
        // Check if the first token is an existing function

        var list = token[0].data.split('.');
        var obj = global;
        for (var i in list) {
            if (obj === undefined)
                return false;
            console.log("testing " + list[i]);
            obj = obj[list[i]];
        }
        return (typeof obj === "function");
    }
    return false;
}

function maybeJavaScript(token)
{
    if (token[0].type === Tokenizer.GROUP) {
        return false;
    } else if (token[0].type === Tokenizer.JAVASCRIPT) {
        if (token.length !== 1) {
            throw "Unexpected JS token length: " + token.length;
        }
        return true;
    } else if (isFunction(token)) {
        return true;
    }
    return false;
}

function runJavaScript(token, job)
{
    var func = "";
    var state = 0;
    var cnt = 0;

    if (token.length < 2) {
        throw "Token length < 2 - " + token.length;
    }

    if (token[0].type !== Tokenizer.JAVASCRIPT) {
        for (var i = 0; i < token.length - 1; ++i) {
            if (!func) {
                func = token[i].data + "(";
            } else {
                if (token[i].type === Tokenizer.GROUP) {
                    func += token[i].data;
                } else {
                    if (token[i].data === "'") {
                        if (state === 0) {
                            if (cnt)
                                func += ", ";
                            func += "'";
                            state = 1;
                        } else {
                            func += "'";
                            state = 0;
                        }
                    } else {
                        if (state === 0 && cnt)
                            func += ", ";
                        func += token[i].data;
                    }
                    ++cnt;
                }
            }
        }
        func += ")";
    } else {
        for (var i in token) {
            func += token[i].data + " ";
        }
    }

    if (job) {
        var jobfunc = undefined;
        try {
            jobfunc = eval("(function(data) {" + func + "})");
        } catch (e) {
        }
        if (typeof jobfunc === "function") {
            job.js(new Job.JavaScript(jobfunc));
        }
    } else {
        console.log("evaling " + func);
        eval.call(global, func);
    }
}

function operator(token)
{
    if (token.length === 0)
        return undefined;
    var tok = token[token.length - 1];
    if (tok.type === Tokenizer.OPERATOR)
        return tok.data;
    else if (tok.type === Tokenizer.HIDDEN && tok.data === ";")
        return tok.data;
    return undefined;
}

function matchOperator(op, ret)
{
    if (op === ';')
        return true;
    else if (op === '&&' && ret)
        return true;
    else if (op === '||' && !ret)
        return true;
    else if (op === '|')
        return true;
    return false;
}

function runLine(line, readLine)
{
    var tokens = [];
    var tok = new Tokenizer.Tokenizer(), token;
    tok.tokenize(line);
    var isjs = true;
    while ((token = tok.next())) {
        // for (var idx = 0; idx < token.length; ++idx) {
        //     console.log(token[idx].type + " -> " + token[idx].data);
        // }
        tokens.push(token);
    }
    if (tokens.length === 1 && isFunction(tokens[0])) {
        try {
            runJavaScript(tokens[0]);
        } catch (e) {
            console.log(e);
            isjs = false;
        }
    } else {
        try {
            console.log("trying the entire thing: '" + line + "'");
            eval.call(global, line);
        } catch (e) {
            console.log(e);
            isjs = false;
        }
    }
    if (isjs) {
        read.resume();
        return;
    }

    var op, ret, job;

    for (var idx = 0; idx < tokens.length; ++idx) {
        token = tokens[idx];
        console.log("----");
        op = operator(token);
        if (op === undefined) {
            throw "Unrecognized operator";
        }
        // remove the operator
        token.pop();

        console.log("operator " + op);
        if (op === '|') {
            if (!job) {
                job = new Job.Job();
            }
        } else if (op !== ';' && job) {
            throw "Invalid operator for pipe job";
        }
        for (i in token) {
            console.log("  token " + token[i].type + " '" + token[i].data + "'");
        }

        var iscmd = true;
        if (token.length >= 1 && token[0].type === Tokenizer.GROUP) {
            console.log("    is a group");
            // run the group
            ret = runLine(token[0].data);
            iscmd = false;
        } else if (maybeJavaScript(token)) {
            console.log("    might be js");
            iscmd = false;
            try {
                ret = runJavaScript(token, job);
            } catch (e) {
                ret = false;
                console.error(e);
            }
            read.resume();
        }
        if (!iscmd) {
            if (matchOperator(op, ret))
                continue;
            else
                return;
        }

        console.log("  is a command");
        var cmd = undefined;
        var args = [];
        for (i in token) {
            if (cmd === undefined) {
                cmd = token[i].data;
            } else if (token[i].type !== Tokenizer.HIDDEN) {
                args.push(token[i].data);
            }
        }
        if (cmd !== undefined) {
            console.log("execing cmd " + cmd);
            if (job) {
                job.proc({ program: cmd, arguments: args, environment: global.jsh.environment(), cwd: process.cwd() });
            } else {
                var procjob = new Job.Job();
                procjob.proc({ program: cmd, arguments: args, environment: global.jsh.environment(), cwd: process.cwd() });
                procjob.exec(Job.FOREGROUND, function(data) { console.log(data); }, function() { read.resume(); });
                if (matchOperator(op, !ret))
                    continue;
                else
                    return;
            }
        }
    }
    if (job) {
        console.log("running job");
        job.exec(Job.FOREGROUND, console.log, function() { read.resume(); });
    }
}

function setupEnv() {
    for (var i in process.env) {
        if (i !== undefined)
            global[i] = process.env[i];
    }
}

function setupBuiltins() {
    var builtins = require('Builtins');
    for (var i in builtins) {
        global[i] = builtins[i];
    }
}

global.jsh.environment = function() {
    var env = [];
    for (var i in global) {
        if (typeof global[i] === "string"
            || typeof global[i] === "number") {
            env.push(i + "=" + global[i]);
        }
    }
    return env;
};

setupEnv();
setupBuiltins();

read = new rl.ReadLine(function(data) {
    if (data === undefined) {
        read.cleanup();
        global.jsh.jshNative.cleanup();
        process.exit();
    }

    try {
        runLine(data, read);
    } catch (e) {
        console.log(e);
        read.resume();
    }
});
