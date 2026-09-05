(function () {
  if (Module.ccCoop && Module.ccCoop.enabled) {
    Module.crownlessCampaignAccess = 0;
    Module.crownlessCampaignAccessMessage = "";
    Module.persistCrownlessSave = async function () {};
    Module.persistCrownlessNewCampaign = async function () {};
    const preferencesKey = "cc-coop-preferences";
    Module.persistCrownlessPreferences = async function (path) {
      localStorage.setItem(preferencesKey, FS.readFile(path, {encoding: "utf8"}));
    };
    Module.preRun = Module.preRun || [];
    Module.preRun.push(function () {
      try {
        const preferences = localStorage.getItem(preferencesKey);
        if (preferences !== null) {
          FS.mkdirTree("/tmp");
          FS.writeFile("/tmp/crownless-coop.ccsave.preferences", preferences);
        }
      } catch (error) {
        console.error("Could not load shared-world preferences", error);
      }
    });
    return;
  }
  const saveDirectory = "/crownless-save";
  const campaignPath = saveDirectory + "/crownless_campaign.ccsave";
  const sessionPath = campaignPath + ".session";
  const preferencesPath = campaignPath + ".preferences";
  const databaseName = "crownless-carriage";
  const storeName = "campaign-files";
  const revisionPath = campaignPath + ".revision";
  const lockName = "crownless-carriage-campaign";
  const leaseKey = "crownless-carriage-campaign-lease";
  const leaseDuration = 8000;
  const leaseRenewal = 2000;
  const lockOwner = typeof crypto !== "undefined" && crypto.randomUUID
    ? crypto.randomUUID()
    : Date.now().toString(36) + "-" + Math.random().toString(36).slice(2);
  let databasePromise;
  let releaseWebLock = null;
  let leaseTimer = null;
  let lockKind = null;

  function setCampaignAccess(code, message) {
    Module.crownlessCampaignAccess = code;
    Module.crownlessCampaignAccessMessage = message;
  }

  function currentLease() {
    if (typeof localStorage === "undefined") return null;
    try {
      const value = JSON.parse(localStorage.getItem(leaseKey));
      return value && typeof value.owner === "string" &&
        Number.isFinite(value.expiresAt) ? value : null;
    } catch (_error) {
      return null;
    }
  }

  function ownsLease() {
    const lease = currentLease();
    return lease && lease.owner === lockOwner && lease.expiresAt > Date.now();
  }

  function renewLease() {
    if (lockKind !== "lease" || !ownsLease()) {
      if (leaseTimer !== null) clearInterval(leaseTimer);
      leaseTimer = null;
      lockKind = null;
      setCampaignAccess(
        2, "Another tab took control of this campaign. Reload before saving.");
      return;
    }
    localStorage.setItem(leaseKey, JSON.stringify({
      owner: lockOwner,
      expiresAt: Date.now() + leaseDuration
    }));
  }

  async function acquireLease() {
    if (typeof localStorage === "undefined") return false;
    const existing = currentLease();
    if (existing && existing.owner !== lockOwner &&
        existing.expiresAt > Date.now()) {
      return false;
    }
    localStorage.setItem(leaseKey, JSON.stringify({
      owner: lockOwner,
      expiresAt: Date.now() + leaseDuration
    }));
    await new Promise(function (resolve) { setTimeout(resolve, 75); });
    if (!ownsLease()) return false;
    lockKind = "lease";
    leaseTimer = setInterval(renewLease, leaseRenewal);
    return true;
  }

  async function acquireCampaignLock() {
    if (lockKind === "web" || ownsLease()) {
      setCampaignAccess(0, "");
      return true;
    }
    if (typeof navigator !== "undefined" && navigator.locks &&
        typeof navigator.locks.request === "function") {
      const acquired = await new Promise(function (resolve) {
        let answered = false;
        const request = navigator.locks.request(
          lockName, {mode: "exclusive", ifAvailable: true},
          async function (lock) {
            answered = true;
            if (!lock) {
              resolve(false);
              return;
            }
            lockKind = "web";
            setCampaignAccess(0, "");
            resolve(true);
            await new Promise(function (release) {
              releaseWebLock = release;
            });
            releaseWebLock = null;
            if (lockKind === "web") {
              lockKind = null;
              setCampaignAccess(
                2, "This tab released the campaign. Reload before saving.");
            }
          });
        request.catch(async function () {
          if (!answered) resolve(await acquireLease());
        });
      });
      if (!acquired) {
        setCampaignAccess(
          1, "This campaign is open in another tab. This tab is read-only.");
      }
      return acquired;
    }
    const acquired = await acquireLease();
    setCampaignAccess(
      acquired ? 0 : 1,
      acquired ? "" :
        "This campaign is open in another tab. This tab is read-only.");
    return acquired;
  }

  function releaseCampaignLock() {
    if (releaseWebLock !== null) {
      const release = releaseWebLock;
      lockKind = null;
      releaseWebLock = null;
      release();
    }
    if (leaseTimer !== null) clearInterval(leaseTimer);
    leaseTimer = null;
    if (lockKind === "lease" && ownsLease()) localStorage.removeItem(leaseKey);
    lockKind = null;
    setCampaignAccess(2, "This tab released the campaign. Reload before saving.");
  }

  function requireCampaignWriteAccess() {
    if (Module.crownlessCampaignAccess !== 0 ||
        (lockKind === "lease" && !ownsLease()) || lockKind === null) {
      if (lockKind === "lease" && !ownsLease()) {
        lockKind = null;
        setCampaignAccess(
          2, "Another tab took control of this campaign. Reload before saving.");
      }
      throw new Error(Module.crownlessCampaignAccessMessage ||
        "This campaign is read-only in this tab.");
    }
  }

  function storedRevision(value) {
    return Number.isSafeInteger(value) && value >= 0 ? value : 0;
  }

  function openDatabase() {
    if (databasePromise) return databasePromise;
    databasePromise = new Promise(function (resolve, reject) {
      const request = indexedDB.open(databaseName, 1);
      request.onupgradeneeded = function () {
        request.result.createObjectStore(storeName);
      };
      request.onsuccess = function () { resolve(request.result); };
      request.onerror = function () { reject(request.error); };
    });
    return databasePromise;
  }

  function readStoredFile(database, path) {
    return new Promise(function (resolve, reject) {
      const request = database
        .transaction(storeName, "readonly")
        .objectStore(storeName)
        .get(path);
      request.onsuccess = function () { resolve(request.result); };
      request.onerror = function () { reject(request.error); };
    });
  }

  async function restoreCampaign() {
    await acquireCampaignLock();
    const database = await openDatabase();
    const files = await Promise.all([
      readStoredFile(database, campaignPath),
      readStoredFile(database, sessionPath),
      readStoredFile(database, revisionPath),
      readStoredFile(database, preferencesPath)
    ]);
    if (files[0]) FS.writeFile(campaignPath, files[0]);
    if (files[1]) FS.writeFile(sessionPath, files[1]);
    if (files[3]) FS.writeFile(preferencesPath, files[3]);
    Module.crownlessCampaignRestored = Boolean(files[0]);
    Module.crownlessSaveRevision = storedRevision(files[2]);
    console.info(Module.crownlessCampaignAccess === 0
      ? (files[0]
        ? "Crownless Carriage campaign restored."
        : "Crownless Carriage save storage is ready.")
      : Module.crownlessCampaignAccessMessage);
  }

  async function persistCampaign(campaign, session, clearSession) {
    requireCampaignWriteAccess();
    const database = await openDatabase();
    const expectedRevision = storedRevision(Module.crownlessSaveRevision);
    let nextRevision = expectedRevision;
    let failure = null;
    await new Promise(function (resolve, reject) {
      const transaction = database.transaction(storeName, "readwrite");
      transaction.oncomplete = function () { resolve(); };
      transaction.onerror = function () {
        reject(failure || transaction.error ||
          new Error("Browser save transaction failed."));
      };
      transaction.onabort = function () {
        reject(failure || transaction.error ||
          new Error("Browser save transaction was cancelled."));
      };
      const store = transaction.objectStore(storeName);
      const revisionRequest = store.get(revisionPath);
      revisionRequest.onerror = function () {
        failure = revisionRequest.error ||
          new Error("Browser save revision could not be read.");
        transaction.abort();
      };
      revisionRequest.onsuccess = function () {
        const revision = storedRevision(revisionRequest.result);
        if (revision !== expectedRevision) {
          setCampaignAccess(
            2, "Another tab changed this campaign. Reload before saving.");
          failure = new Error(Module.crownlessCampaignAccessMessage);
          transaction.abort();
          return;
        }
        try {
          requireCampaignWriteAccess();
        } catch (error) {
          failure = error;
          transaction.abort();
          return;
        }
        nextRevision = revision + 1;
        if (campaign === null) store.delete(campaignPath);
        else store.put(campaign, campaignPath);
        if (clearSession) store.delete(sessionPath);
        else if (session) store.put(session, sessionPath);
        store.put(nextRevision, revisionPath);
      };
    });
    Module.crownlessSaveRevision = nextRevision;
  }

  Module.persistCrownlessSave = async function (campaignFile, sessionFile) {
    const campaign = FS.readFile(campaignFile).slice();
    const hasSession = FS.analyzePath(sessionFile).exists;
    const session = hasSession ? FS.readFile(sessionFile).slice() : null;
    await persistCampaign(campaign, session, false);
  };

  Module.persistCrownlessNewCampaign = async function (campaignFile) {
    const campaign = FS.readFile(campaignFile).slice();
    await persistCampaign(campaign, null, true);
  };

  Module.deleteCrownlessCampaign = async function () {
    await persistCampaign(null, null, true);
    Module.crownlessCampaignRestored = false;
  };

  Module.persistCrownlessPreferences = async function (preferencesFile) {
    const preferences = FS.readFile(preferencesFile).slice();
    const database = await openDatabase();
    await new Promise(function (resolve, reject) {
      const transaction = database.transaction(storeName, "readwrite");
      transaction.oncomplete = function () { resolve(); };
      transaction.onerror = function () {
        reject(transaction.error ||
          new Error("Browser preferences transaction failed."));
      };
      transaction.onabort = function () {
        reject(transaction.error ||
          new Error("Browser preferences transaction was cancelled."));
      };
      transaction.objectStore(storeName).put(
        preferences, preferencesPath);
    });
  };

  Module.acquireCrownlessCampaignLock = acquireCampaignLock;
  Module.releaseCrownlessCampaignLock = releaseCampaignLock;
  setCampaignAccess(1, "Campaign ownership is still loading.");

  if (typeof addEventListener === "function") {
    addEventListener("pagehide", releaseCampaignLock);
    addEventListener("beforeunload", releaseCampaignLock);
    addEventListener("storage", function (event) {
      if (event.key === leaseKey && lockKind === "lease" && !ownsLease()) {
        renewLease();
      }
    });
  }

  Module.preRun = Module.preRun || [];
  Module.preRun.push(function () {
    FS.mkdir(saveDirectory);
    const dependency = "crownless-save-load";
    addRunDependency(dependency);
    restoreCampaign()
      .catch(function (error) {
        console.error("Could not load Crownless Carriage saves", error);
      })
      .finally(function () { removeRunDependency(dependency); });
  });
})();
