const { spawn } = require("child_process");
const { createRPC } = require("./rpc");

function createServer({ logs, lspPath, workspace }) {
  let lspProcess = null;
  let lsprpc = null;

  function cheDoc(doc) {
    return doc.languageId === "c" || doc.languageId === "che";
  }

  return {
    async start(rootUri) {
      const c = spawn(lspPath, ["lsp"], { stdio: ["pipe", "pipe", "pipe"] });
      lspProcess = c;
      logs.appendLine("spawned `" + lspPath + " lsp` (pid " + c.pid + ")");
      lsprpc = createRPC(c.stdin, c.stdout, (s) => logs.appendLine(s));
      c.stderr.on("data", (chunk) => logs.append(chunk.toString()));
      c.on("error", (err) =>
        logs.appendLine("failed to launch `" + lspPath + "`: " + err.message),
      );
      c.on("exit", (code, signal) => {
        logs.appendLine("server exited (" + (signal || code) + ")");
        if (lspProcess === c) {
          lspProcess = null;
          lsprpc.close();
          lsprpc = null;
        }
      });

      try {
        const res = await lsprpc.call("initialize", {
          processId: process.pid,
          rootUri,
          capabilities: {
            textDocument: { synchronization: { didSave: true } },
          },
        });
        logs.appendLine("server info: " + JSON.stringify(res));
        lsprpc.send("initialized", {});
        for (const doc of workspace.textDocuments) {
          this.didOpen(doc);
        }
      } catch (e) {
        logs.appendLine("initialize failed: " + e.message);
      }
    },
    stop() {
      if (lspProcess) {
        lspProcess.kill();
        lspProcess = null;
        lsprpc.close();
        lsprpc = null;
      }
    },
    didOpen(doc) {
      if (!cheDoc(doc) || !lsprpc) {
        return;
      }
      lsprpc.send("textDocument/didOpen", {
        textDocument: {
          uri: doc.uri.toString(),
          languageId: doc.languageId,
          version: doc.version,
          text: doc.getText(),
        },
      });
    },
    didChange(e) {
      if (!cheDoc(e.document) || !lsprpc) {
        return;
      }
      lsprpc.send("textDocument/didChange", {
        textDocument: {
          uri: e.document.uri.toString(),
          version: e.document.version,
        },
        contentChanges: [{ text: e.document.getText() }],
      });
    },
    didClose(doc) {
      if (!cheDoc(doc) || !lsprpc) {
        return;
      }
      lsprpc.send("textDocument/didClose", {
        textDocument: { uri: doc.uri.toString() },
      });
    },
  };
}

module.exports = { createServer };
