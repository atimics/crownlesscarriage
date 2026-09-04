import assert from "node:assert/strict";

const campaignPath = "/crownless-save/crownless_campaign.ccsave";
const sessionPath = campaignPath + ".session";
const storedFiles = new Map();
const virtualFiles = new Map();
let failNextWrite = false;

function copyBytes(value) {
  return new Uint8Array(value);
}

const database = {
  transaction(_storeName, mode) {
    const operations = [];
    const transaction = {
      error: null,
      objectStore() {
        return {
          get(path) {
            const request = {};
            queueMicrotask(function () {
              const value = storedFiles.get(path);
              request.result = value ? copyBytes(value) : undefined;
              if (request.onsuccess) request.onsuccess();
            });
            return request;
          },
          put(value, path) {
            operations.push(function () {
              storedFiles.set(path, copyBytes(value));
            });
          },
          delete(path) {
            operations.push(function () { storedFiles.delete(path); });
          }
        };
      }
    };
    queueMicrotask(function () {
      if (mode === "readwrite" && failNextWrite) {
        failNextWrite = false;
        transaction.error = new Error("injected transaction failure");
        if (transaction.onabort) transaction.onabort();
        return;
      }
      for (const operation of operations) operation();
      if (transaction.oncomplete) transaction.oncomplete();
    });
    return transaction;
  }
};

globalThis.indexedDB = {
  open() {
    const request = {};
    queueMicrotask(function () {
      request.result = database;
      if (request.onsuccess) request.onsuccess();
    });
    return request;
  }
};

globalThis.Module = {};
globalThis.FS = {
  mkdir() {},
  readFile(path) {
    const value = virtualFiles.get(path);
    if (!value) throw new Error("missing virtual file: " + path);
    return copyBytes(value);
  },
  writeFile(path, value) {
    virtualFiles.set(path, copyBytes(value));
  },
  analyzePath(path) {
    return {exists: virtualFiles.has(path)};
  }
};
globalThis.addRunDependency = function () {};
let restoreFinished;
const restored = new Promise(function (resolve) { restoreFinished = resolve; });
globalThis.removeRunDependency = function () { restoreFinished(); };

await import(new URL("../web/persistence.js", import.meta.url));

const oldCampaign = new Uint8Array([1, 2, 3]);
const oldSession = new Uint8Array([4, 5, 6]);
const newCampaign = new Uint8Array([7, 8, 9]);
virtualFiles.set(campaignPath, oldCampaign);
virtualFiles.set(sessionPath, oldSession);
await Module.persistCrownlessSave(campaignPath, sessionPath);
assert.deepEqual(storedFiles.get(campaignPath), oldCampaign);
assert.deepEqual(storedFiles.get(sessionPath), oldSession);

virtualFiles.set(campaignPath, newCampaign);
failNextWrite = true;
await assert.rejects(Module.persistCrownlessNewCampaign(campaignPath));
assert.deepEqual(storedFiles.get(campaignPath), oldCampaign);
assert.deepEqual(storedFiles.get(sessionPath), oldSession);

await Module.persistCrownlessNewCampaign(campaignPath);
assert.deepEqual(storedFiles.get(campaignPath), newCampaign);
assert.equal(storedFiles.has(sessionPath), false);

virtualFiles.clear();
Module.preRun[0]();
await restored;
assert.deepEqual(virtualFiles.get(campaignPath), newCampaign);
assert.equal(virtualFiles.has(sessionPath), false);
assert.equal(Module.crownlessCampaignRestored, true);

console.log("Web persistence tests passed");
