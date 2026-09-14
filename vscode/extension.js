const vscode = require("vscode");
const { spawn } = require("child_process");

let output;
let child = null;
let buffer = Buffer.alloc(0);
let nextId = 0;
const pending = new Map();

function send(msg) {
  console.log("sending", msg);
  if (!child || !child.stdin.writable) {
    return;
  }
  const body = JSON.stringify(msg);
  child.stdin.write(
    "Content-Length: " + Buffer.byteLength(body) + "\r\n\r\n" + body,
  );
}

function request(method, params) {
  return new Promise((resolve, reject) => {
    const id = ++nextId;
    pending.set(id, { resolve, reject });
    send({ jsonrpc: "2.0", id, method, params });
  });
}

function resetState() {
  buffer = Buffer.alloc(0);
  pending.clear();
}

function startServer() {
  resetState();
  const cmd = vscode.workspace.getConfiguration("che").get("lsp.path", "che");
  child = spawn(cmd, ["lsp"], { stdio: ["pipe", "pipe", "pipe"] });
  output.appendLine("spawned `" + cmd + " lsp` (pid " + child.pid + ")");
  child.stdout.on("data", (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
    pump();
  });
  child.stderr.on("data", (chunk) => output.append(chunk.toString()));
  child.on("error", (err) =>
    output.appendLine("failed to launch `" + cmd + "`: " + err.message),
  );
  child.on("exit", (code, signal) => {
    output.appendLine("server exited (" + (signal || code) + ")");
    child = null;
  });
}

function stopServer() {
  if (child) {
    child.kill();
    child = null;
  }
}

function pump() {
  while (true) {
    const i = buffer.indexOf("\r\n\r\n");
    if (i === -1) {
      return;
    }
    const header = buffer.slice(0, i).toString("utf8");
    const m = /Content-Length:\s*(\d+)/i.exec(header);
    const start = i + 4;
    if (!m) {
      output.appendLine(
        "dropping message with bad header: " + JSON.stringify(header),
      );
      buffer = buffer.slice(start);
      continue;
    }
    const len = parseInt(m[1], 10);
    if (buffer.length < start + len) {
      return;
    }
    const body = buffer.slice(start, start + len).toString("utf8");
    buffer = buffer.slice(start + len);
    try {
      onMessage(JSON.parse(body));
    } catch (e) {
      output.appendLine("failed to parse server message: " + e);
    }
  }
}

function onMessage(msg) {
  if (msg.id !== undefined && msg.method) {
    send({
      jsonrpc: "2.0",
      id: msg.id,
      error: { code: -32601, message: "method not found" },
    });
    return;
  }
  if (msg.id !== undefined) {
    const p = pending.get(msg.id);
    if (p) {
      pending.delete(msg.id);
      if (msg.error) {
        p.reject(new Error(msg.error.message || JSON.stringify(msg.error)));
      } else {
        p.resolve(msg.result);
      }
    }
    return;
  }
  output.appendLine("notification `" + msg.method + "` from server");
}

async function initialize() {
  const rootUri =
    vscode.workspace.workspaceFolders &&
    vscode.workspace.workspaceFolders.length
      ? vscode.workspace.workspaceFolders[0].uri.toString()
      : null;
  try {
    const res = await request("initialize", {
      processId: process.pid,
      rootUri,
      capabilities: { textDocument: { synchronization: { didSave: true } } },
    });
    output.appendLine("server info: " + JSON.stringify(res));
    send({ jsonrpc: "2.0", method: "initialized", params: {} });
    for (const doc of vscode.workspace.textDocuments) {
      didOpen(doc);
    }
  } catch (e) {
    output.appendLine("initialize failed: " + e.message);
  }
}

function cheDoc(doc) {
  return doc.languageId === "c" || doc.languageId === "che";
}

function didOpen(doc) {
  if (!cheDoc(doc)) {
    return;
  }
  send({
    jsonrpc: "2.0",
    method: "textDocument/didOpen",
    params: {
      textDocument: {
        uri: doc.uri.toString(),
        languageId: doc.languageId,
        version: doc.version,
        text: doc.getText(),
      },
    },
  });
}

function didChange(e) {
  if (!cheDoc(e.document)) {
    return;
  }
  send({
    jsonrpc: "2.0",
    method: "textDocument/didChange",
    params: {
      textDocument: {
        uri: e.document.uri.toString(),
        version: e.document.version,
      },
      contentChanges: [{ text: e.document.getText() }],
    },
  });
}

function didClose(doc) {
  if (!cheDoc(doc)) {
    return;
  }
  send({
    jsonrpc: "2.0",
    method: "textDocument/didClose",
    params: {
      textDocument: { uri: doc.uri.toString() },
    },
  });
}

function activate(context) {
  output = vscode.window.createOutputChannel("Che LSP");
  context.subscriptions.push(
    output,
    vscode.commands.registerCommand("che-lsp.logs", () => output.show()),
    vscode.commands.registerCommand("che-lsp.restart", () => {
      stopServer();
      startServer();
      initialize();
    }),
    vscode.workspace.onDidOpenTextDocument(didOpen),
    vscode.workspace.onDidChangeTextDocument(didChange),
    vscode.workspace.onDidCloseTextDocument(didClose),
  );

  startServer();
  initialize();
}

function deactivate() {
  stopServer();
}

module.exports = { activate, deactivate };
