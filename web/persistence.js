(function () {
  const saveDirectory = "/crownless-save";
  const campaignPath = saveDirectory + "/crownless_campaign.ccsave";
  const sessionPath = campaignPath + ".session";
  const databaseName = "crownless-carriage";
  const storeName = "campaign-files";
  let databasePromise;

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
    const database = await openDatabase();
    const files = await Promise.all([
      readStoredFile(database, campaignPath),
      readStoredFile(database, sessionPath)
    ]);
    if (files[0]) FS.writeFile(campaignPath, files[0]);
    if (files[1]) FS.writeFile(sessionPath, files[1]);
    Module.crownlessCampaignRestored = Boolean(files[0]);
    console.info(files[0]
      ? "Crownless Carriage campaign restored."
      : "Crownless Carriage save storage is ready.");
  }

  Module.persistCrownlessSave = async function (campaignFile, sessionFile) {
    const database = await openDatabase();
    const campaign = FS.readFile(campaignFile).slice();
    const hasSession = FS.analyzePath(sessionFile).exists;
    const session = hasSession ? FS.readFile(sessionFile).slice() : null;
    await new Promise(function (resolve, reject) {
      const transaction = database.transaction(storeName, "readwrite");
      transaction.oncomplete = function () { resolve(); };
      transaction.onerror = function () { reject(transaction.error); };
      transaction.onabort = function () { reject(transaction.error); };
      const store = transaction.objectStore(storeName);
      store.put(campaign, campaignPath);
      if (session) store.put(session, sessionPath);
    });
  };

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
