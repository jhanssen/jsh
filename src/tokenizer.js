var Tokenizer = {
    Tokenize_None: 0x0,
    Tokenize_CollapseWhitespace: 0x1,
    Tokenize_ExpandEnvironmentVariables: 0x2,

    tokenize: function tokenize(line, flags)
    {
        if (flags & Tokenize_ExpandEnvironmentVariables) {
            var max = 10;
            while (max--) {
                var newLine = expandEnvironment(line, err);
                if (newLine !== undefined) {
                    line = newLine;
                } else {
                    break;
                }
            }
        }

        function isspace(str, idx)
        {
            return /\s/.test(str.charAt(idx));
        }


        var tokens = [];
        var last = undefined;
        var idx = 0;
        var escapes = 0;
        while (idx < line.length) {
            // error() << "checking" << *str;
            // ### isspace needs to be utf8-aware
            if (last === undefined && (!(flags & Tokenize_CollapseWhitespace) || !isspace(line, idx)))
                last = str;
            if (*str == '\\') {
                ++escapes;
                ++str;
                continue;
            }
            // if (last && str)
            //     printf("last %c %p, str %c %p\n", *last, last, *str, str);

            switch (*str) {
            case '{': {
                addPrev(tokens, last, str - 1, flags);
                const char *end = findEndBrace(str + 1);
                if (!end) {
                    err = String::format<128>("Can't find end of curly brace that starts at position %d", str - start);
                    return List<Shell::Token>();
                }
                tokens.append({Shell::Token::Javascript, String(str, end - str + 1)});
                str = end;
                break; }
            case '\"':
            case '\'': {
                if (escapes % 2 == 0) {
                    const char *end = findUnescaped(str);
                    if (end) {
                        str = end;
                    } else {
                        err = String::format<128>("Can't find end of quote that starts at position %d", str - start);
                        return List<Shell::Token>();
                    }
                }
                break; }
            case '|':
                if (escapes % 2 == 0) {
                    addPrev(tokens, last, str - 1, flags);
                    if (str[1] == '|') {
                        tokens.append({Shell::Token::Operator, String(str, 2)});
                        ++str;
                    } else {
                        tokens.append({Shell::Token::Pipe, String(str, 1)});
                    }
                }
                break;
            case '&':
                if (escapes % 2 == 0) {
                    addPrev(tokens, last, str - 1, flags);
                    if (str[1] == '&') {
                        tokens.append({Shell::Token::Operator, String(str, 2)});
                        ++str;
                    } else {
                        tokens.append({Shell::Token::Operator, String(str, 1)});
                    }
                }
                break;
            case ';':
            case '<':
            case '>':
            case '(':
            case ')':
            case '!':
                if (escapes % 2 == 0) {
                    addPrev(tokens, last, str - 1, flags);
                    tokens.append({Shell::Token::Operator, String(str, 1)});
                }
                break;
            case ' ':
                if (escapes % 2 == 0) {
                    addPrev(tokens, last, str - 1, flags);
                }
                break;
            default:
                break;
            }
            escapes = 0;
            ++str;
        }
        if (last && last + 1 <= str) {
            if (!tokens.isEmpty() && tokens.last().type == Shell::Token::Command) {
                tokens.last().args.append(stripBraces(String(last, str - last)));
                tokens.last().raw += " " + String(last, str - last + 1);
                if (flags & Tokenize_CollapseWhitespace)
                    eatEscapes(tokens.last().args.last());
            } else {
                tokens.append({Shell::Token::Command, stripBraces(String(last, str - last))});
                tokens.last().raw = String(last, str - last);
                if (flags & Tokenize_CollapseWhitespace)
                    eatEscapes(tokens.last().string);
            }
        }
        return tokens;
    }



},

    // private
    expandEnvironment: function expandEnvironment(string)
{
    var changes = false;
    function replace(str, idx, len, replacement)
    {
        changes = true;
        if (!replacement)
            replacement = "";
        return str.substr(0, idx) + replacement + str.substr(idx + len);
    }
    var Invalid = 0, Valid = 1, ValidNonStart = 2;

    function environmentVarChar(ch)
    {
        // ### charcodes?
        if (ch >= '0' || && ch <= '9')
            return ValidNonStart;
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_')
            return Valid;
        return Invalid;
    }

    var environ = process.env;
    var escapes = 0;
    for (var i=0; i<string.length - 1; ++i) {
        switch (string[i]) {
        case '$':
            if (escapes % 2 == 0) {
                if (string[i + 1] == '{') {
                    for (var j=i + 2; j<string.size(); ++j) {
                        if (string[j] == '}') {
                            var env = string.substr(i + 2, j - (i + 2));
                            string = replace(string, i, j - i + 1, environ[env]);
                        } else if (environmentVarChar(string[j]) == Invalid) {
                            throw "Bad substitution";
                        }
                    }

                } else if (string[i + 1] == '$') {
                    string = replace(string, i + 1, 1);
                } else if (environmentVarChar(string[i + 1]) == Valid) {
                    var j=i + 2;
                    while (j<string.length && environmentVarChar(string[j]) != Invalid)
                        ++j;
                    var sub = environ.value(string.substr(i + 1, j - 1));
                    string = replace(string, i, j - i + 1, sub);
                } else {
                    throw "Bad substitution";
                }
            }
            escapes = 0;
            break;
        case '\\':
            ++escapes;
            break;
        default:
            escapes = 0;
            break;
        }
    }
    return changes ? string : undefined;
}
};

// List<Shell::Token> Input::tokenize(String line, unsigned var flags, String &err) const
// {
//     assert(err.isEmpty());
//     if (flags & Tokenize_ExpandEnvironmentVariables) {
//         var max = 10;
//         while (max--) {
//             if (!expandEnvironment(line, err)) {
//                 break;
//             }
//         }
//         if (!err.isEmpty()) {
//             return List<Shell::Token>();
//         }

//         if (max < 0) {
//             err = "Too many recursive environment variable expansions";
//             return List<Shell::Token>();
//         }
//     }


//     List<Shell::Token> tokens;
//     const char *start = line.constData();
//     const char *str = start;
//     const char *last = str;
//     var escapes = 0;
//     while (*str) {
//         // error() << "checking" << *str;
//         // ### isspace needs to be utf8-aware
//         if (!last && (!(flags & Tokenize_CollapseWhitespace) || !isspace(static_cast<unsigned char>(*str))))
//             last = str;
//         if (*str == '\\') {
//             ++escapes;
//             ++str;
//             continue;
//         }
//         // if (last && str)
//         //     printf("last %c %p, str %c %p\n", *last, last, *str, str);

//         switch (*str) {
//         case '{': {
//             addPrev(tokens, last, str - 1, flags);
//             const char *end = findEndBrace(str + 1);
//             if (!end) {
//                 err = String::format<128>("Can't find end of curly brace that starts at position %d", str - start);
//                 return List<Shell::Token>();
//             }
//             tokens.append({Shell::Token::Javascript, String(str, end - str + 1)});
//             str = end;
//             break; }
//         case '\"':
//         case '\'': {
//             if (escapes % 2 == 0) {
//                 const char *end = findUnescaped(str);
//                 if (end) {
//                     str = end;
//                 } else {
//                     err = String::format<128>("Can't find end of quote that starts at position %d", str - start);
//                     return List<Shell::Token>();
//                 }
//             }
//             break; }
//         case '|':
//             if (escapes % 2 == 0) {
//                 addPrev(tokens, last, str - 1, flags);
//                 if (str[1] == '|') {
//                     tokens.append({Shell::Token::Operator, String(str, 2)});
//                     ++str;
//                 } else {
//                     tokens.append({Shell::Token::Pipe, String(str, 1)});
//                 }
//             }
//             break;
//         case '&':
//             if (escapes % 2 == 0) {
//                 addPrev(tokens, last, str - 1, flags);
//                 if (str[1] == '&') {
//                     tokens.append({Shell::Token::Operator, String(str, 2)});
//                     ++str;
//                 } else {
//                     tokens.append({Shell::Token::Operator, String(str, 1)});
//                 }
//             }
//             break;
//         case ';':
//         case '<':
//         case '>':
//         case '(':
//         case ')':
//         case '!':
//             if (escapes % 2 == 0) {
//                 addPrev(tokens, last, str - 1, flags);
//                 tokens.append({Shell::Token::Operator, String(str, 1)});
//             }
//             break;
//         case ' ':
//             if (escapes % 2 == 0) {
//                 addPrev(tokens, last, str - 1, flags);
//             }
//             break;
//         default:
//             break;
//         }
//         escapes = 0;
//         ++str;
//     }
//     if (last && last + 1 <= str) {
//         if (!tokens.isEmpty() && tokens.last().type == Shell::Token::Command) {
//             tokens.last().args.append(stripBraces(String(last, str - last)));
//             tokens.last().raw += " " + String(last, str - last + 1);
//             if (flags & Tokenize_CollapseWhitespace)
//                 eatEscapes(tokens.last().args.last());
//         } else {
//             tokens.append({Shell::Token::Command, stripBraces(String(last, str - last))});
//             tokens.last().raw = String(last, str - last);
//             if (flags & Tokenize_CollapseWhitespace)
//                 eatEscapes(tokens.last().string);
//         }
//     }
//     return tokens;
// }
