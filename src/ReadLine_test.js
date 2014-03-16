var rl = require('ReadLine');

function Tokenizer()
{
}

Tokenizer.prototype._line = undefined;
Tokenizer.prototype._pos = undefined;
Tokenizer.prototype._prev = undefined;
Tokenizer.prototype._state = undefined;
Tokenizer.prototype._statenum = 0;

Tokenizer.prototype.NORMAL = 0;
Tokenizer.prototype.QUOTE = 1;
Tokenizer.prototype.SINGLEQUOTE = 2;
Tokenizer.prototype.BRACE = 3;

Tokenizer.prototype.HIDDEN = 0;
Tokenizer.prototype.OPERATOR = 1;
Tokenizer.prototype.COMMAND = 2;
Tokenizer.prototype.JAVASCRIPT = 3;

Tokenizer.prototype.tokenize = function(line)
{
    this._line = line;
    this._pos = this._prev = 0;
    this._state = this.NORMAL;
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
};

Tokenizer.prototype._addQuote = function(entry)
{
    entry.push({ type: this.HIDDEN, data: "'" });
};

Tokenizer.prototype.next = function()
{
    var entry = [];

    var done = false;
    this._prev = this._pos;
    var start = this._prev;
    while (!done && this._pos < this._line.length) {
        switch (this._line[this._pos]) {
        case '"':
            if (this._state === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                this._addQuote(entry);
                this._state = this.QUOTE;
            } else if (this._state === this.QUOTE) {
                this._addPrev(this.COMMAND, entry);
                this._addQuote(entry);
                this._state = this.NORMAL;
            }
            break;
        case '\'':
            if (this._state === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                this._addQuote(entry);
                this._state = this.SINGLEQUOTE;
            } else if (this._state === this.SINGLEQUOTE) {
                this._addPrev(this.COMMAND, entry);
                this._addQuote(entry);
                this._state = this.NORMAL;
            }
            break;
        case '{':
            if (this._state === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                if (start < this._pos)
                    return entry;
                this._state = this.BRACE;
            }
            ++this._statenum;
            break;
        case '}':
            if (this._state === this.BRACE) {
                --this._statenum;
                if (this._statenum < 0) {
                    throw "statenum < 0";
                }
                if (!this._statenum) {
                    this._addPrev(this.JAVASCRIPT, entry);
                    this._state = this.NORMAL;
                    done = true;
                }
            }
            break;
        case ';':
            if (this._state === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                done = true;
            }
            break;
        case ' ':
            if (this._state === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
            }
            break;
        case '&':
        case '|':
        case '>':
        case '<':
        case '(':
        case ')':
        case '=':
        case ',':
            if (this._state === this.NORMAL) {
                this._addPrev(this.COMMAND, entry);
                this._addOperator(entry);
            }
            break;
        }
        ++this._pos;
    }

    if (this._state !== this.NORMAL) {
        throw "Tokenizer didn't end in normal state";
    }

    this._addPrev(this.COMMAND, entry);

    return entry.length === 0 ? undefined : entry;
};

function maybeJavaScript(token)
{
    if (token[0].type === Tokenizer.prototype.JAVASCRIPT) {
        if (token.length !== 1) {
            throw "Unexpected JS token length: " + token.length;
        }
        return true;
    } else if (token[0].type === Tokenizer.prototype.COMMAND) {
        // see if either
        // 1: the second token is "=" and we have a total of three tokens
        // 2: the first token is an existing function

        if (token.length === 3 && token[1].data === "=")
            return true;

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
    return true;
}

function runJavaScript(token)
{
    var func = "";
    var state = 0;
    var cnt = 0;

    if (token[0].type !== Tokenizer.prototype.JAVASCRIPT && (token.length != 3 || token[1].data !== "=")) {
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

    console.log("evaling " + func);
    eval.call(global, func);
}

var read = new rl.ReadLine(function(data) {
    if (data === undefined) {
        read.cleanup();
        process.exit();
    }

    var tok = new Tokenizer(), token, js;
    tok.tokenize(data);
    while ((token = tok.next())) {
        console.log("----");
        for (var i in token) {
            console.log("  token " + token[i].type + " '" + token[i].data + "'");
        }

        if (maybeJavaScript(token)) {
            console.log("    might be js");
            runJavaScript(token);
        }
    }
});
