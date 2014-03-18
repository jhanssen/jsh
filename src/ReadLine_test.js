var rl = require('ReadLine');
var pc = require('ProcessChain');
var Job = require('Job');
var Tokenizer = require('Tokenizer');
var jsh = require('jsh');
global.jsh = {
    jshNative: new jsh.native.jsh()
};

function maybeJavaScript(token)
{
    if (token[0].type === Tokenizer.GROUP) {
        return false;
    } else if (token[0].type === Tokenizer.JAVASCRIPT) {
        if (token.length !== 1) {
            throw "Unexpected JS token length: " + token.length;
        }
        return true;
    } else if (token[0].type === Tokenizer.COMMAND) {
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

function runJavaScript(token, job)
{
    var func = "";
    var state = 0;
    var cnt = 0;

    if (token[0].type !== Tokenizer.JAVASCRIPT) {
        var hasParen = token.length > 1 && token[1].data === '(';

        for (var i in token) {
            if (!func) {
                func = token[i].data + (hasParen ? "" : "(");
            } else {
                if (hasParen) {
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
        func += (hasParen ? "" : ")");
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
    return false;
}

function runLine(line)
{
    // try to run the entire thing as JS
    var isjs = true;
    try {
        eval.call(global, line);
    } catch (e) {
        isjs = false;
    }
    if (isjs)
        return;

    var op, ret, job;

    var tok = new Tokenizer.Tokenizer(), token;
    tok.tokenize(line);
    while ((token = tok.next())) {
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
        for (var i in token) {
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
            }
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
        for (var i in token) {
            if (cmd === undefined) {
                cmd = token[i].data;
            } else if (token[i].type !== Tokenizer.HIDDEN) {
                args.push(token[i].data);
            }
        }
        if (cmd !== undefined) {
            console.log("execing cmd " + cmd);
            if (job) {
                job.proc({ program: cmd, arguments: args });
            } else {
                var proc = new pc.ProcessChain(global.jsh.jshNative);
                proc.chain({ program: cmd, arguments: args });
                ret = proc.end(console.log);
                console.log("got " + ret);
                if (matchOperator(op, !ret))
                    continue;
                else
                    return;
            }
        }
    }
    if (job) {
        console.log("running job");
        job.exec(console.log);
    }
}

var read = new rl.ReadLine(function(data) {
    if (data === undefined) {
        read.cleanup();
        global.jsh.jshNative.cleanup();
        process.exit();
    }

    runLine(data);
});
