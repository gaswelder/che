const { workspace, window, commands } = require("vscode");
const { createServer } = require("./server");

let SERVER = null;

function rootUri() {
  return workspace.workspaceFolders && workspace.workspaceFolders.length
    ? workspace.workspaceFolders[0].uri.toString()
    : null;
}

module.exports = {
  activate(context) {
    const output = window.createOutputChannel("Che");
    SERVER = createServer({
      logs: output,
      lspPath: workspace.getConfiguration("che").get("lsp.path", "che"),
      workspace,
    });
    context.subscriptions.push(
      output,
      commands.registerCommand("che-lsp.logs", () => output.show()),
      commands.registerCommand("che-lsp.restart", () => {
        SERVER.stop();
        SERVER.start(rootUri());
      }),
      workspace.onDidOpenTextDocument(SERVER.didOpen),
      workspace.onDidChangeTextDocument(SERVER.didChange),
      workspace.onDidCloseTextDocument(SERVER.didClose),
    );

    SERVER.start(rootUri());
  },
  deactivate() {
    SERVER.stop();
    SERVER = null;
  },
};
