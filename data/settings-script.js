function byId(id) {
  return document.getElementById(id);
}

function populatePresetChoices(selectElement, choices) {
  if (!selectElement || selectElement.options.length > 0) {
    return;
  }

  for (const choice of choices || []) {
    const option = document.createElement("option");
    option.value = String(choice.value);
    option.textContent = choice.label;
    selectElement.appendChild(option);
  }
}

function setPresetValue(selectElement, value, choices, fallbackUnit) {
  if (!selectElement) {
    return;
  }

  const stringValue = String(value);
  const existingOption = Array.from(selectElement.options).find((option) => option.value === stringValue);

  if (!existingOption) {
    const customOption = document.createElement("option");
    customOption.value = stringValue;
    const matchingChoice = (choices || []).find((item) => String(item.value) === stringValue);
    customOption.textContent = `${matchingChoice ? matchingChoice.label : `${value} ${fallbackUnit}`} (perso)`;
    selectElement.appendChild(customOption);
  }

  selectElement.value = stringValue;
}

function populateTimeZoneChoices(selectElement, choices) {
  if (!selectElement || selectElement.options.length > 0) {
    return;
  }

  const customOption = document.createElement("option");
  customOption.value = "";
  customOption.textContent = "-- Personnalisé --";
  selectElement.appendChild(customOption);

  for (const choice of choices || []) {
    const option = document.createElement("option");
    option.value = choice.value;
    option.textContent = choice.label;
    selectElement.appendChild(option);
  }
}

function setTimeZoneValue(selectElement, inputElement, tzValue, choices) {
  if (!selectElement || !inputElement) {
    return;
  }

  const matchingChoice = (choices || []).find((c) => c.value === tzValue);
  if (matchingChoice) {
    selectElement.value = tzValue;
    inputElement.value = "";
  } else {
    selectElement.value = "";
    inputElement.value = tzValue || "";
  }
}

function applyRangeProps(inputElement, schemaNode) {
  if (!inputElement || !schemaNode) {
    return;
  }
  if (schemaNode.min !== undefined) inputElement.min = schemaNode.min;
  if (schemaNode.max !== undefined) inputElement.max = schemaNode.max;
  if (schemaNode.step !== undefined) inputElement.step = schemaNode.step;
}

function selectedTimeZoneString() {
  const presetEl = byId("timezoneSelect");
  const customEl = byId("timezoneString");
  const preset = presetEl ? presetEl.value : "";
  const custom = customEl ? customEl.value.trim() : "";
  return (preset || custom) || "CET-1CEST,M3.5.0,M10.5.0";
}

function setInputValue(id, value) {
  const el = byId(id);
  if (el) {
    el.value = value;
  }
}

function setCheckboxValue(id, value) {
  const el = byId(id);
  if (el) {
    el.checked = !!value;
  }
}

async function openSettings() {
  const res = await fetch("/get-settings", { cache: "no-store" });
  if (!res.ok) {
    throw new Error(`HTTP ${res.status}`);
  }

  const s = await res.json();
  const tzNode = s?.timeSource?.tzString;
  const timezoneValue = typeof tzNode === "object" ? (tzNode.value || "") : (tzNode || "");

  const measurePeriodChoices = s.mesure.measurePeriod.choices || [];
  const pageIntervalChoices = s.display.pageInterval.choices || [];
  const brightChoices = s.display.bright.choices || [];
  const standbyBrightChoices = s.display.standbyBright.choices || [];
  const sensorSampleMethodChoices = s.mesure.sampleMethod.choices || [];
  const predictionMethodChoices = s.mesure.predictionMethod.choices || [];
  const sensorSampleIntervalChoices = s.mesure.sampleInterval.choices || [];
  const timezoneChoices = (typeof tzNode === "object" && Array.isArray(tzNode.choices))
    ? tzNode.choices
    : [];

  populatePresetChoices(byId("MEASURE_PERIOD"), measurePeriodChoices);
  populatePresetChoices(byId("PAGE_INTERVAL"), pageIntervalChoices);
  populatePresetChoices(byId("bright"), brightChoices);
  populatePresetChoices(byId("standbyBright"), standbyBrightChoices);
  populatePresetChoices(byId("sensorSampleMethod"), sensorSampleMethodChoices);
  populatePresetChoices(byId("predictionMethod"), predictionMethodChoices);
  populatePresetChoices(byId("sensorSampleInterval"), sensorSampleIntervalChoices);
  populateTimeZoneChoices(byId("timezoneSelect"), timezoneChoices);

  setInputValue("tankCapacite_L", s.tank.capacite_L.value);
  setInputValue("tankHeight_mm", s.tank.height_mm.value);
  setInputValue("sensorOffset_mm", s.sensor.offset_mm.value);
  setInputValue("sensorSampleCount", s.mesure.sampleCount.value);
  setCheckboxValue("sensorEnableSpikeDetection", s.sensor.enableSpikeDetection);
  setInputValue("sensorSpikeThreshold", s.sensor.spikeThreshold_mm.value);
  setInputValue("sensorSpikeRetryMax", s.sensor.spikeRetryMax.value);
  setInputValue("sensorPredictionRetryMax", s.sensor.predictionRetryMax.value);
  setCheckboxValue("sensorEnableIqrFilter", s.sensor.enableIqrFilter);
  setInputValue("DEBOUNCE_TIME", s.button.debounceTime.value);
  setInputValue("LONG_PRESS_TIME", s.button.longPressTime.value);
  setInputValue("standbyInterval", s.display.standbyInterval.value);
  setCheckboxValue("standbyActived", s.display.standbyActived);

  setPresetValue(byId("sensorSampleInterval"), s.mesure.sampleInterval.value, sensorSampleIntervalChoices, "ms");
  setPresetValue(byId("MEASURE_PERIOD"), s.mesure.measurePeriod.value, measurePeriodChoices, "ms");
  setPresetValue(byId("PAGE_INTERVAL"), s.display.pageInterval.value, pageIntervalChoices, "sec");
  setPresetValue(byId("bright"), s.display.bright.value, brightChoices, "lvl");
  setPresetValue(byId("standbyBright"), s.display.standbyBright.value, standbyBrightChoices, "lvl");
  setPresetValue(byId("sensorSampleMethod"), s.mesure.sampleMethod.value, sensorSampleMethodChoices, "mode");
  setPresetValue(byId("predictionMethod"), s.mesure.predictionMethod.value, predictionMethodChoices, "mode");
  setTimeZoneValue(byId("timezoneSelect"), byId("timezoneString"), timezoneValue, timezoneChoices);

  applyRangeProps(byId("tankCapacite_L"), s.tank.capacite_L);
  applyRangeProps(byId("tankHeight_mm"), s.tank.height_mm);
  applyRangeProps(byId("sensorOffset_mm"), s.sensor.offset_mm);
  applyRangeProps(byId("sensorSampleCount"), s.mesure.sampleCount);
  applyRangeProps(byId("sensorSpikeThreshold"), s.sensor.spikeThreshold_mm);
  applyRangeProps(byId("sensorSpikeRetryMax"), s.sensor.spikeRetryMax);
  applyRangeProps(byId("sensorPredictionRetryMax"), s.sensor.predictionRetryMax);
  applyRangeProps(byId("DEBOUNCE_TIME"), s.button.debounceTime);
  applyRangeProps(byId("LONG_PRESS_TIME"), s.button.longPressTime);
  applyRangeProps(byId("standbyInterval"), s.display.standbyInterval);

  const settingsForm = byId("settingsForm");
  if (settingsForm) {
    settingsForm.style.display = "block";
  }
}

function buildSettingsPayload() {
  const sensorOffset = Number(byId("sensorOffset_mm").value);

  return {
    tank: {
      capacite_L: { value: Number(byId("tankCapacite_L").value) },
      height_mm: { value: Number(byId("tankHeight_mm").value) }
    },
    sensor: {
      offset_mm: { value: sensorOffset },
      enableSpikeDetection: byId("sensorEnableSpikeDetection").checked,
      spikeThreshold_mm: { value: Number(byId("sensorSpikeThreshold").value) },
      spikeRetryMax: { value: Number(byId("sensorSpikeRetryMax").value) },
      predictionRetryMax: { value: Number(byId("sensorPredictionRetryMax").value) },
      enableIqrFilter: byId("sensorEnableIqrFilter").checked
    },
    button: {
      debounceTime: { value: Number(byId("DEBOUNCE_TIME").value) },
      longPressTime: { value: Number(byId("LONG_PRESS_TIME").value) }
    },
    display: {
      bright: { value: Number(byId("bright").value) },
      standbyBright: { value: Number(byId("standbyBright").value) },
      standbyInterval: { value: Number(byId("standbyInterval").value) },
      standbyActived: byId("standbyActived").checked,
      pageInterval: { value: Number(byId("PAGE_INTERVAL").value) }
    },
    mesure: {
      measurePeriod: { value: Number(byId("MEASURE_PERIOD").value) },
      predictionMethod: { value: Number(byId("predictionMethod").value) },
      sampleMethod: { value: Number(byId("sensorSampleMethod").value) },
      sampleInterval: { value: Number(byId("sensorSampleInterval").value) },
      sampleCount: { value: Number(byId("sensorSampleCount").value) }
    },
    timeSource: {
      tzString: selectedTimeZoneString()
    }
  };
}

async function saveSettings() {
  try {
    const res = await fetch("/save-settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(buildSettingsPayload())
    });

    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }

    document.body.innerHTML = "<h2>Parametres sauvegardes</h2>";
    setTimeout(() => {
      window.location.href = "/";
    }, 750);
  } catch (err) {
    console.error(err);
    alert(`Erreur sauvegarde: ${err?.message || "inconnue"}`);
  }
}

async function loadDefaultSettings() {
  try {
    if (!confirm("Reinitialiser tous les parametres et redemarrer l'ESP32 ?")) {
      return;
    }

    const res = await fetch("/load-default-settings", { method: "POST" });
    if (!res.ok) {
      alert("Erreur lors du chargement des parametres par defaut");
      return;
    }

    document.body.innerHTML = "<h2>Parametres par defaut charges. Redemarrage en cours...</h2>";
    setTimeout(() => {
      window.location.href = "/settings";
    }, 3000);
  } catch (err) {
    console.error(err);
    alert("Erreur reseau");
  }
}

window.addEventListener("DOMContentLoaded", () => {
  const backHomeBtn = byId("backHomeBtn");
  const openFileManagerBtn = byId("openFileManagerBtn");
  const saveSettingsBtn = byId("saveSettingsBtn");
  const cancelSettingsBtn = byId("cancelSettingsBtn");
  const loadDefaultSettingsBtn = byId("loadDefaultSettingsBtn");
  const loadLogsBtn = byId("loadLogsBtn");
  const deleteLogsBtn = byId("deleteLogsBtn");
  const timezoneSelect = byId("timezoneSelect");

  if (backHomeBtn) {
    backHomeBtn.addEventListener("click", () => {
      window.location.href = "/";
    });
  }

  if (openFileManagerBtn) {
    openFileManagerBtn.addEventListener("click", () => {
      window.location.href = "/file-manager";
    });
  }

  if (saveSettingsBtn) {
    saveSettingsBtn.addEventListener("click", saveSettings);
  }

  if (cancelSettingsBtn) {
    cancelSettingsBtn.addEventListener("click", () => {
      document.body.innerHTML = "<h2>Annulation.</h2>";
      setTimeout(() => {
        window.location.href = "/";
      }, 750);
    });
  }

  if (loadDefaultSettingsBtn) {
    loadDefaultSettingsBtn.addEventListener("click", loadDefaultSettings);
  }

  if (loadLogsBtn) {
    loadLogsBtn.addEventListener("click", () => {
      window.location.href = "/logs";
    });
  }

  if (deleteLogsBtn) {
    deleteLogsBtn.addEventListener("click", async () => {
      if (!confirm("Effacer le fichier log.txt ?")) {
        return;
      }

      try {
        const res = await fetch("/delete-log", { method: "POST" });
        if (!res.ok) {
          throw new Error(res.statusText || `HTTP ${res.status}`);
        }
        alert("Fichier log.txt efface avec succes");
        window.location.href = "/settings";
      } catch (err) {
        alert(`Impossible de contacter le serveur: ${err?.message || err}`);
      }
    });
  }

  if (timezoneSelect) {
    timezoneSelect.addEventListener("change", (event) => {
      if (event.target.value) {
        const timezoneString = byId("timezoneString");
        if (timezoneString) {
          timezoneString.value = "";
        }
      }
    });
  }

  openSettings().catch((err) => {
    console.error("openSettings failed", err);
    alert(`Erreur chargement des parametres: ${err?.message || "inconnue"}`);
  });
});

