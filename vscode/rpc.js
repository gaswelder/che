function createRPC(stdin, stdout, log) {
  let nextId = 0;
  let closed = false;
  const pending = new Map();

  function sendRaw(msg) {
    log("sending " + JSON.stringify(msg));
    if (closed || !stdin.writable) {
      return;
    }
    const body = JSON.stringify(msg);
    stdin.write(
      "Content-Length: " + Buffer.byteLength(body) + "\r\n\r\n" + body,
    );
  }

  async function startReader() {
    let buf = Buffer.alloc(0);
    for await (const chunk of stdout) {
      buf = Buffer.concat([buf, chunk]);
      while (true) {
        const i = buf.indexOf("\r\n\r\n");
        if (i === -1) {
          break;
        }
        const header = buf.slice(0, i).toString("utf8");
        const start = i + 4;
        const m = /Content-Length:\s*(\d+)/i.exec(header);
        if (!m) {
          log("dropping message with bad header: " + JSON.stringify(header));
          buf = buf.slice(start);
          continue;
        }
        const len = parseInt(m[1], 10);
        if (buf.length < start + len) {
          break;
        }
        const body = buf.slice(start, start + len).toString("utf8");
        buf = buf.slice(start + len);
        let msg;
        try {
          msg = JSON.parse(body);
        } catch (e) {
          log("failed to parse server message: " + e);
          continue;
        }
        if (msg.id !== undefined && msg.method) {
          sendRaw({
            jsonrpc: "2.0",
            id: msg.id,
            error: { code: -32601, message: "method not found" },
          });
          continue;
        }
        if (msg.id !== undefined) {
          const p = pending.get(msg.id);
          if (p) {
            pending.delete(msg.id);
            if (msg.error) {
              p.reject(
                new Error(msg.error.message || JSON.stringify(msg.error)),
              );
            } else {
              p.resolve(msg.result);
            }
          }
          continue;
        }
        log("notification `" + msg.method + "` from server");
      }
    }
  }
  startReader();

  return {
    send(method, params) {
      sendRaw({ jsonrpc: "2.0", method, params });
    },
    call(method, params) {
      return new Promise((resolve, reject) => {
        const id = ++nextId;
        if (closed) {
          reject(new Error("rpc closed"));
          return;
        }
        pending.set(id, { resolve, reject });
        sendRaw({ jsonrpc: "2.0", id, method, params });
      });
    },
    close() {
      closed = true;
      for (const p of pending.values()) {
        p.reject(new Error("rpc closed"));
      }
      pending.clear();
    },
  };
}

module.exports = { createRPC };