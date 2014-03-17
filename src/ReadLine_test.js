var rl = require('ReadLine');
var pc = require('ProcessChain');

function Tokenizer()
{
}

Tokenizer.prototype._line = undefined;
Tokenizer.prototype._pos = undefined;
Tokenizer.prototype._prev = undefined;
Tokenizer.prototype._state = {
    state: [],
    push: function(s) { this.state.push(s); },
    pop: function() { return this.state.pop(); },
    value: function() {
        if (this.state.length === 0)
            throw("state length is 0");
        return this.state[this.state.length - 1];
    },
    prev: function() {
        if (this.state.length === 0)
            throw("state length is 0");
        if (this.state.length === 1)
            return undefined;
        return this.state[this.state.length - 2];
    },
    count: function(s) {
        if (this.state.length === 0)
            throw("state length is 0");
        var pos = this.state.length - 1;
        var cnt = 0;
        while (pos >= 0) {
            if (this.state[pos] !== s)
                return cnt;
            --pos;
            ++cnt;
        }
        return cnt;
    },
    is: function() {
        for (var i in arguments) {
            if (this.value() === arguments[i])
                return true;
        }
        return false;
    }
};

Tokenizer.prototype.NORMAL = 0;
Tokenizer.prototype.QUOTE = 1;
Tokenizer.prototype.SINGLEQUOTE = 2;
Tokenizer.prototype.BRACE = 3;
Tokenizer.prototype.PAREN = 4;

Tokenizer.prototype.HIDDEN = 0;
Tokenizer.prototype.OPERATOR = 1;
Tokenizer.prototype.COMMAND = 2;
Tokenizer.prototype.JAVASCRIPT = 3;
Tokenizer.prototype.GROUP = 4;

Tokenizer.prototype.tokenize = function(line)
{
    this._line = line;
    this._pos = this._prev = 0;
    this._state.push(Tokenizer.prototype.NORMAL);
};

Tokenizer.prototype._addPrev = function(type, entry)
{
    if (this._pos > this._prev)
        entry.push({ type: type, data: this._line.substr(this._prev, this._pos - this._prev)});
    this._prev = this._pos + 1;
};

Tokenizer.prototype._addOperator = function(entry)
{
    var op = this._line[this._pos];
    var len = 1;
    if (this._pos + 1 < this._line.length && this._line[this._pos + 1] === op) {
        len = 2;
    }
    entry.push({ type: this.OPERATOR, data: this._line.substr(this._pos, len) });
    if (len === 2)
        ++this._pos;
    this._prev = this._pos + 1;
    return len;
};

Tokenizer.prototype._addHidden = function(entry, data)
{
    entry.push({ type: this.HIDDEN, data: data });
};

Tokenizer.prototype.next = function()
{
    var entry = [], len, ch, st;

    var done = false;
    this._prev = this._pos;
    var start = this._prev;
    while (!done && this._pos < this._line.length) {
        ch = this._line[this._pos];
        switch (ch) {
        case '"':
            if (this._state.value() === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                this._addHidden(entry, "'");
                this._state.push(this.QUOTE);
            } else if (this._state.value() === this.QUOTE) {
                this._addPrev(this.COMMAND, entry);
                this._addHidden(entry, "'");
                this._state.pop();
            }
            break;
        case '\'':
            if (this._state.value() === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                this._addHidden(entry, "'");
                this._state.push(this.SINGLEQUOTE);
            } else if (this._state.value() === this.SINGLEQUOTE) {
                this._addPrev(this.COMMAND, entry);
                this._addHidden(entry, "'");
                this._state.pop();
            }
            break;
        case '{':
        case '(':
            st = (ch === '{' ? this.BRACE : this.PAREN);
            if (this._state.is(this.NORMAL, st)) {
                if (this._state.is(this.NORMAL)) {
                    this._addPrev(this.COMMAND, entry);
                    if (entry.length !== 0)
                        return entry;
                }
                this._state.push(st);
            }
            break;
        case '}':
        case ')':
            st = (ch === '}' ? this.BRACE : this.PAREN);
            if (this._state.value() === st) {
                if (this._state.prev() === this.NORMAL) {
                    this._addPrev((st === this.BRACE) ? this.JAVASCRIPT : this.GROUP, entry);
                    if (st === this.BRACE)
                        done = true;
                }
                this._state.pop();
            }
            break;
        case ';':
            if (this._state.value() === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                this._addHidden(entry, ';');
                done = true;
            }
            break;
        case ' ':
            if (this._state.value() === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
            }
            break;
        case '|':
            if (this._state.value() === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                this._addOperator(entry);
                done = true;
            }
            break;
        case '&':
            if (this._state.value() === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                if (this._addOperator(entry) === 2) // &&
                    done = true;
            }
            break;
        case '>':
        case '<':
        case '=':
        case ',':
            if (this._state.value() === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                this._addOperator(entry);
            }
            break;
        }
        ++this._pos;
    }

    if (this._state.value() !== this.NORMAL) {
        throw "Tokenizer didn't end in normal state";
    }

    this._addPrev(this.COMMAND, entry);
    if (entry.length > 0) {
        var e = entry[entry.length - 1];
        if (e.type !== this.OPERATOR && e.data !== ";")
            entry.push({ type: this.HIDDEN, data: ";" });
    }

    return entry.length === 0 ? undefined : entry;
};

function maybeJavaScript(token)
{
    if (token[0].type === Tokenizer.prototype.GROUP) {
        return false;
    } else if (token[0].type === Tokenizer.prototype.JAVASCRIPT) {
        if (token.length !== 1) {
            throw "Unexpected JS token length: " + token.length;
        }
        return true;
    } else if (token[0].type === Tokenizer.prototype.COMMAND) {
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

function runJavaScript(token)
{
    var func = "";
    var state = 0;
    var cnt = 0;

    if (token[0].type !== Tokenizer.prototype.JAVASCRIPT) {
        var hasParen = token.length > 1 && token[1].data === '(';
        if (token.length > 1 && (token[token.length - 1].type === Tokenizer.prototype.OPERATOR || token[token.length - 1].data === ";")) {
            token.pop();
        }

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

    console.log("evaling " + func);
    eval.call(global, func);
}

function operator(token)
{
    if (token.length === 0)
        return undefined;
    var tok = token[token.length - 1];
    if (tok.type === Tokenizer.prototype.OPERATOR)
        return tok.data;
    else if (tok.type === Tokenizer.prototype.HIDDEN && tok.data === ";")
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

    var op, ret;

    var tok = new Tokenizer(), token;
    tok.tokenize(line);
    while ((token = tok.next())) {
        console.log("----");
        op = operator(token);
        if (op === undefined) {
            throw "Unrecognized operator";
        }
        console.log("operator " + op);
        for (var i in token) {
            console.log("  token " + token[i].type + " '" + token[i].data + "'");
        }

        var iscmd = true;
        if (token.length >= 1 && token[0].type === tok.GROUP) {
            console.log("    is a group");
            // run the group
            ret = runLine(token[0].data);
            iscmd = false;
        } else if (maybeJavaScript(token)) {
            console.log("    might be js");
            iscmd = false;
            try {
                ret = runJavaScript(token);
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

        if (token[token.length - 1].type === Tokenizer.prototype.OPERATOR || token[token.length - 1].data === ";") {
            token.pop();
        }
        console.log("  is a command");
        var cmd = undefined;
        var args = [];
        for (var i in token) {
            if (cmd === undefined) {
                cmd = token[i].data;
            } else if (token[i].type !== tok.HIDDEN) {
                args.push(token[i].data);
            }
        }
        if (cmd !== undefined) {
            console.log("execing cmd " + cmd);
            var proc = new pc.ProcessChain();
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

var read = new rl.ReadLine(function(data) {
    if (data === undefined) {
        read.cleanup();
        process.exit();
    }

    runLine(data);
});
