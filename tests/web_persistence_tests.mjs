import assert from "node:assert/strict";
import {readFile} from "node:fs/promises";
import vm from "node:vm";

const campaignPath = "/crownless-save/crownless_campaign.ccsave";
const sessionPath = campaignPath + ".session";
const revisionPath = campaignPath + ".revision";
const persistenceSource = await readFile(
  new URL("../web/persistence.js", import.meta.url), "utf8");
const storedFiles = new Map();
let failNextWrite = false;

function cloneValue(value) {
  return value instanceof Uint8Array ? new Uint8Array(value) : value;
}

function createDatabase() {
  return {
    transaction(_storeName, mode) {
      const operations = [];
      let pending = 0;
      let aborted = false;
      let completionScheduled = false;
      const transaction = {
        error: null,
        abort() {
          if (aborted) return;
          aborted = true;
          queueMicrotask(function () {
            if (transaction.onabort) transaction.onabort();
          });
        },
        objectStore() {
          return {
            get(path) {
              const request = {};
              pending += 1;
              queueMicrotask(function () {
                if (aborted) return;
                request.result = cloneValue(storedFiles.get(path));
                pending -= 1;
                if (request.onsuccess) request.onsuccess();
                scheduleCompletion();
              });
              return request;
            },
            put(value, path) {
              operations.push(function () {
                storedFiles.set(path, cloneValue(value));
              });
            },
            delete(path) {
              operations.push(function () { storedFiles.delete(path); });
            }
          };
        }
      };
      function scheduleCompletion() {
        if (completionScheduled || pending > 0 || aborted) return;
        completionScheduled = true;
        queueMicrotask(function () {
          if (aborted) return;
          if (pending > 0) {
            completionScheduled = false;
            return;
          }
          if (mode === "readwrite" && failNextWrite) {
            failNextWrite = false;
            transaction.error = new Error("injected transaction failure");
            transaction.abort();
            return;
          }
          for (const operation of operations) operation();
          if (transaction.oncomplete) transaction.oncomplete();
        });
      }
      scheduleCompletion();
      return transaction;
    }
  };
}

const database = createDatabase();
const indexedDB = {
  open() {
    const request = {};
    queueMicrotask(function () {
      request.result = database;
      if (request.onsuccess) request.onsuccess();
    });
    return request;
  }
};

class LockManager {
  constructor() {
    this.owner = null;
  }

  request(name, options, callback) {
    assert.equal(name, "crownless-carriage-campaign");
    assert.equal(options.mode, "exclusive");
    assert.equal(options.ifAvailable, true);
    if (this.owner !== null) return Promise.resolve(callback(null));
    const owner = {};
    this.owner = owner;
    return Promise.resolve(callback({name})).finally(() => {
      if (this.owner === owner) this.owner = null;
    });
  }
}

const lockManager = new LockManager();
let pageCounter = 0;

async function createPage() {
  pageCounter += 1;
  const virtualFiles = new Map();
  const listeners = new Map();
  let ready;
  const readyPromise = new Promise(function (resolve) { ready = resolve; });
  const sandbox = {
    Module: {},
    FS: {
      mkdir() {},
      readFile(path) {
        const value = virtualFiles.get(path);
        if (!value) throw new Error("missing virtual file: " + path);
        return cloneValue(value);
      },
      writeFile(path, value) {
        virtualFiles.set(path, cloneValue(value));
      },
      analyzePath(path) {
        return {exists: virtualFiles.has(path)};
      }
    },
    indexedDB,
    navigator: {locks: lockManager},
    crypto: {randomUUID: () => "page-" + pageCounter},
    addRunDependency() {},
    removeRunDependency() { ready(); },
    addEventListener(name, listener) { listeners.set(name, listener); },
    console: {info() {}, warn() {}, error() {}},
    clearInterval,
    setInterval,
    setTimeout,
    Uint8Array
  };
  vm.createContext(sandbox);
  new vm.Script(persistenceSource, {filename: "persistence.js"})
    .runInContext(sandbox);
  sandbox.Module.preRun[0]();
  await readyPromise;
  return {
    Module: sandbox.Module,
    files: virtualFiles,
    async close() {
      const pagehide = listeners.get("pagehide");
      if (pagehide) pagehide();
      await new Promise(function (resolve) { setTimeout(resolve, 0); });
    }
  };
}

const oldCampaign = new Uint8Array([1, 2, 3]);
const oldSession = new Uint8Array([4, 5, 6]);
const ownerCampaign = new Uint8Array([7, 8, 9]);
const staleCampaign = new Uint8Array([10, 11, 12]);
const replacementCampaign = new Uint8Array([13, 14, 15]);
storedFiles.set(campaignPath, oldCampaign);
storedFiles.set(sessionPath, oldSession);
storedFiles.set(revisionPath, 0);

const owner = await createPage();
const second = await createPage();
assert.equal(owner.Module.crownlessCampaignAccess, 0);
assert.equal(second.Module.crownlessCampaignAccess, 1);
assert.deepEqual(owner.files.get(campaignPath), oldCampaign);
assert.deepEqual(second.files.get(campaignPath), oldCampaign);

second.files.set(campaignPath, staleCampaign);
await assert.rejects(
  second.Module.persistCrownlessSave(campaignPath, sessionPath),
  /read-only/);
assert.deepEqual(storedFiles.get(campaignPath), oldCampaign);

owner.files.set(campaignPath, replacementCampaign);
failNextWrite = true;
await assert.rejects(
  owner.Module.persistCrownlessNewCampaign(campaignPath),
  /injected transaction failure/);
assert.deepEqual(storedFiles.get(campaignPath), oldCampaign);
assert.deepEqual(storedFiles.get(sessionPath), oldSession);

owner.files.set(campaignPath, ownerCampaign);
owner.files.set(sessionPath, oldSession);
await owner.Module.persistCrownlessSave(campaignPath, sessionPath);
assert.deepEqual(storedFiles.get(campaignPath), ownerCampaign);
assert.equal(storedFiles.get(revisionPath), 1);

await owner.close();
assert.equal(await second.Module.acquireCrownlessCampaignLock(), true);
assert.equal(second.Module.crownlessCampaignAccess, 0);
await assert.rejects(
  second.Module.persistCrownlessSave(campaignPath, sessionPath),
  /Another tab changed this campaign/);
assert.equal(second.Module.crownlessCampaignAccess, 2);
assert.deepEqual(storedFiles.get(campaignPath), ownerCampaign);
assert.equal(storedFiles.get(revisionPath), 1);

await second.close();
const recovered = await createPage();
assert.equal(recovered.Module.crownlessCampaignAccess, 0);
assert.equal(recovered.Module.crownlessSaveRevision, 1);
assert.deepEqual(recovered.files.get(campaignPath), ownerCampaign);
recovered.files.set(campaignPath, replacementCampaign);
await recovered.Module.persistCrownlessNewCampaign(campaignPath);
assert.deepEqual(storedFiles.get(campaignPath), replacementCampaign);
assert.equal(storedFiles.has(sessionPath), false);
assert.equal(storedFiles.get(revisionPath), 2);

await recovered.close();
const reloaded = await createPage();
assert.equal(reloaded.Module.crownlessCampaignAccess, 0);
assert.equal(reloaded.Module.crownlessSaveRevision, 2);
assert.deepEqual(reloaded.files.get(campaignPath), replacementCampaign);
assert.equal(reloaded.files.has(sessionPath), false);
await reloaded.close();

console.log("Web persistence tests passed in two page contexts");
