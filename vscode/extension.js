const vscode = require('vscode');

// const { LanguageClient } = require('vscode-languageclient');

const HELLO = 'fx.hello'

const log = vscode.window.createOutputChannel("fx.log");
const lsc = require('vscode-languageclient');

function hello() { 
    // vscode.window.showInformationMessage(HELLO); 
    log.appendLine("hello:");
    log.appendLine(['lcs',lsc]);
}

async function activate(context) {
    console.log(activate, context);
    vscode.window.showInformationMessage(activate);
    let disposable = vscode.commands.registerCommand(HELLO, hello);
    context.subscriptions.push(disposable);
    log.appendLine("activate: ok");
    // 
    // https://www.toptal.com/javascript/language-server-protocol-tutorial
    // 
    const executable = {
        command: 'blacklist-server',
        args: ['--stdio'],
      };
      const serverOptions = {
        run: executable,
        debug: executable,
      };
      const clientOptions = {
        documentSelector: [{
          scheme: 'file',
          language: 'plaintext',
        }],
      };
        // 
      const client = new lsc.LanguageClient(
        'fx',
        'fx.lsp',
        serverOptions,
        clientOptions
      );
      log.appendLine(['client',client]);
  
}

function deactivate() {
    console.log(deactivate);
    vscode.window.showInformationMessage(deactivate);
}

module.exports = {
    activate,
    deactivate
}
