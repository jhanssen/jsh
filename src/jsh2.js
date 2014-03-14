var readline = require('readline');
var fs = require('fs');
var rl = readline.createInterface({
    terminal: true,
    input: process.stdin,
    output: process.stdout,
});
// console.dir(rl);

rl.loadHistory = function loadHistory(file) {
    var histFile = (process.env.HOME || process.env.HOMEPATH || process.env.USERPROFILE) + "/" + file;
    try {
        this.history = fs.readFileSync(histFile, 'utf8').split("\n");
    } catch (err) {}
};

rl.saveHistory = function saveHistory(file) {
    var histFile = (process.env.HOME || process.env.HOMEPATH || process.env.USERPROFILE) + "/" + file;
    try {
        fs.writeFileSync(histFile, this.history.join("\n"));
    } catch (err) { }
}

rl.clearScreen = function clearScreen() {
    process.stdout.write('\u001B[2J\u001B[0;0f');
    this.prompt(true);
};

rl.updateInteractiveHistorySearch = function updateInteractiveHistorySearch(term, idx) {
    this.interactiveHistoryIndex = idx;
    var prompt = "(reverse-i-search)`" + term + "': ";
    if (term && this.history.length) {
        var historyIndex = this.history.length - 1;
        while (idx-- > 0) {
            while (historyIndex >= 0 && this.history[historyIndex].indexOf(term) == -1)
                --historyIndex;
            if (historyIndex >= 0) {
                prompt += this.history[historyIndex];
            }
        }
    }
    this.setPrompt(prompt);
    this.prompt(true);
};
rl.historySearchBackward = function historySearchBackward() {
    var preserve = true;
    if (this.interactiveHistoryIndex === undefined) {
        this.updateInteractiveHistorySearch("", 1);
    }
};

rl.historySearchForward = function historySearchForward() {


};

rl.loadHistory(".jsh_history");
rl.setPrompt('OHAI> ');
rl.prompt();


rl.on('line', function(line) {
    rl.saveHistory(".jsh_history");
    switch(line.trim()) {
    case 'hello':
        console.dir(rl);
        // rl.write('Delete me!');
        // // Simulate ctrl+u to delete the line written previously
        // rl.write(null, {ctrl: true, name: 'u'});
        break;
    default:
        console.log('Say what? I might have heard `' + line.trim() + '`');
        break;
    }
    rl.prompt();
}).on('close', function() {
    console.log('Have a great day!');
    process.exit(0);
});

// var readlineSync = require('readline-sync');
// var answer = readlineSync.question('What is your favorite food? :');
// console.log('Oh, so your favorite food is ' + answer);


process.stdin.on('keypress', function (s, key) { // ctrl-l
    if (key.ctrl) {
        switch (key.name) {
        case 'l': rl.clearScreen(); break;
        case 'r': rl.historySearchBackward(); break;
        case 's': rl.historySearchForward(); break;
        }
    }
});
