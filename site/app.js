(function () {
  "use strict";

  const settingType = {
    Integer: 0,
    PositiveBigInteger: 1,
    Float: 2,
    Boolean: 3,
    String: 4,
    Palette: 5,
    Color: 6,
    Slider: 7
  };

  const state = {
    staticStats: null,
    dynamicStats: null,
    settings: null,
    settingsSpecs: [],
    unifiedSettings: null,
    unifiedSchema: null,
    effects: null,
    effectDraft: {},
    deviceDraft: {},
    deviceErrors: new Set(),
    effectDialog: {
      open: false,
      effectIndex: null,
      effectName: "",
      specs: [],
      values: {},
      draft: {},
      errors: new Set()
    },
    autoRefresh: true,
    statsRefreshSeconds: Number(localStorage.getItem("nd.statsRefreshSeconds") || 3),
    timers: {
      stats: null,
      effects: null,
      countdown: null
    },
    preview: {
      socket: null,
      connected: false,
      frame: null
    }
  };

  const els = {};

  document.addEventListener("DOMContentLoaded", init);

  async function init() {
    bindElements();
    bindEvents();
    initializeStaticShell();
    await loadAll();
    restartPolling();
  }

  function bindElements() {
    const ids = [
      "connectionStatus", "connectionStatusText", "hostValue", "webPortValue",
      "prevEffectButton", "nextEffectButton", "refreshEffectsButton",
      "effectIntervalInput", "saveIntervalButton", "statsRefreshInput", "autoRefreshToggle",
      "rebootButton", "resetDeviceConfigButton", "resetEffectsConfigButton",
      "summaryCurrentEffect", "summaryEffectStatus", "summaryInterval", "summaryIntervalRemaining",
      "summaryTopology", "summaryDriver", "summaryLedFps", "summaryAudioFps", "summaryCpu",
      "summaryCpuCores", "summaryHeap", "summaryPsram",
      "effectsMeta", "effectsTableBody", "reloadSettingsButton", "applySettingsButton",
      "applySettingsRebootButton", "deviceSettingsForm", "statsTimestamp", "statsGrid",
      "previewConnectButton", "previewDisconnectButton", "previewStatus", "previewCanvas",
      "effectSettingsDialog", "effectDialogTitle", "effectSettingsForm", "closeEffectDialogButton",
      "cancelEffectDialogButton", "applyEffectDialogButton", "toastStack"
    ];
    ids.forEach((id) => {
      els[id] = document.getElementById(id);
    });
  }

  function bindEvents() {
    els.prevEffectButton.addEventListener("click", () => postForm("/previousEffect").then(loadEffectsOnly));
    els.nextEffectButton.addEventListener("click", () => postForm("/nextEffect").then(loadEffectsOnly));
    els.refreshEffectsButton.addEventListener("click", () => loadEffectsOnly());
    els.saveIntervalButton.addEventListener("click", applyEffectInterval);
    els.reloadSettingsButton.addEventListener("click", () => loadSettingsOnly());
    els.applySettingsButton.addEventListener("click", () => applyDeviceSettings(false));
    els.applySettingsRebootButton.addEventListener("click", () => applyDeviceSettings(true));
    els.rebootButton.addEventListener("click", () => resetDevice({ board: 1 }));
    els.resetDeviceConfigButton.addEventListener("click", () => resetDevice({ board: 1, deviceConfig: 1 }));
    els.resetEffectsConfigButton.addEventListener("click", () => resetDevice({ board: 1, effectsConfig: 1 }));
    els.statsRefreshInput.addEventListener("change", () => {
      state.statsRefreshSeconds = clampInt(els.statsRefreshInput.value, 1, 60, 3);
      localStorage.setItem("nd.statsRefreshSeconds", String(state.statsRefreshSeconds));
      restartPolling();
    });
    els.autoRefreshToggle.addEventListener("change", () => {
      state.autoRefresh = !!els.autoRefreshToggle.checked;
      restartPolling();
    });
    els.previewConnectButton.addEventListener("click", connectPreviewSocket);
    els.previewDisconnectButton.addEventListener("click", disconnectPreviewSocket);
    els.closeEffectDialogButton.addEventListener("click", closeEffectDialog);
    els.cancelEffectDialogButton.addEventListener("click", closeEffectDialog);
    els.applyEffectDialogButton.addEventListener("click", applyEffectSettings);
  }

  function initializeStaticShell() {
    els.hostValue.textContent = window.location.hostname || "--";
    els.webPortValue.textContent = window.location.port || (window.location.protocol === "https:" ? "443" : "80");
    els.statsRefreshInput.value = state.statsRefreshSeconds;
    els.autoRefreshToggle.checked = state.autoRefresh;
    setConnectionState("Starting", "pending");
  }

  async function loadAll() {
    setConnectionState("Loading", "pending");
    try {
      const [staticStats, dynamicStats, settings, settingsSpecs, unifiedSettings, unifiedSchema, effects] = await Promise.all([
        fetchJson("/statistics/static"),
        fetchJson("/statistics/dynamic"),
        fetchJson("/settings"),
        fetchJson("/settings/specs"),
        fetchJson("/api/v1/settings"),
        fetchJson("/api/v1/settings/schema"),
        fetchJson("/effects")
      ]);

      state.staticStats = staticStats;
      state.dynamicStats = dynamicStats;
      state.settings = settings;
      state.settingsSpecs = settingsSpecs;
      state.unifiedSettings = unifiedSettings;
      state.unifiedSchema = unifiedSchema;
      state.effects = effects;
      state.deviceDraft = {};
      state.deviceErrors.clear();

      renderSettingsForm();
      renderEffects();
      renderSummaries();
      renderStats();
      drawPreviewFrame();
      setConnectionState("Connected", "online");
    } catch (error) {
      handleError("Initial load failed", error);
      setConnectionState("Offline", "offline");
    }
  }

  async function loadEffectsOnly() {
    try {
      state.effects = await fetchJson("/effects");
      renderEffects();
      renderSummaries();
    } catch (error) {
      handleError("Failed to refresh effects", error);
    }
  }

  async function loadSettingsOnly() {
    try {
      const [settings, settingsSpecs, unifiedSettings, unifiedSchema, staticStats] = await Promise.all([
        fetchJson("/settings"),
        fetchJson("/settings/specs"),
        fetchJson("/api/v1/settings"),
        fetchJson("/api/v1/settings/schema"),
        fetchJson("/statistics/static")
      ]);
      state.settings = settings;
      state.settingsSpecs = settingsSpecs;
      state.unifiedSettings = unifiedSettings;
      state.unifiedSchema = unifiedSchema;
      state.staticStats = staticStats;
      state.deviceDraft = {};
      state.deviceErrors.clear();
      renderSettingsForm();
      renderSummaries();
      renderStats();
    } catch (error) {
      handleError("Failed to reload settings", error);
    }
  }

  async function loadDynamicStatsOnly() {
    if (!state.autoRefresh) {
      return;
    }
    try {
      state.dynamicStats = await fetchJson("/statistics/dynamic");
      renderSummaries();
      renderStats();
    } catch (error) {
      handleError("Failed to refresh statistics", error);
    }
  }

  function restartPolling() {
    clearTimer("stats");
    clearTimer("effects");
    clearTimer("countdown");

    if (state.autoRefresh) {
      state.timers.stats = window.setInterval(loadDynamicStatsOnly, state.statsRefreshSeconds * 1000);
      state.timers.effects = window.setInterval(loadEffectsOnly, 3000);
    }

    state.timers.countdown = window.setInterval(renderSummaries, 250);
  }

  function clearTimer(key) {
    if (state.timers[key]) {
      window.clearInterval(state.timers[key]);
      state.timers[key] = null;
    }
  }

  function setConnectionState(text, stateClass) {
    els.connectionStatusText.textContent = text;
    els.connectionStatus.classList.remove("online", "offline");
    if (stateClass === "online" || stateClass === "offline") {
      els.connectionStatus.classList.add(stateClass);
    }
  }

  function renderSummaries() {
    const effects = state.effects;
    const staticStats = state.staticStats;
    const dynamicStats = state.dynamicStats;
    const unifiedSettings = state.unifiedSettings;

    if (!effects || !staticStats || !dynamicStats || !unifiedSettings) {
      return;
    }

    const effectList = Array.isArray(effects.Effects) ? effects.Effects : [];
    const currentIndex = Number(effects.currentEffect || 0);
    const currentEffect = effectList[currentIndex];
    const intervalMs = Number(effects.effectInterval || 0);
    const pinned = !!effects.eternalInterval || intervalMs === 0;
    const remainingMs = Number(effects.millisecondsRemaining || 0);
    const remainingSec = pinned ? "Pinned" : `${Math.max(0, Math.ceil(remainingMs / 1000))} sec`;

    els.summaryCurrentEffect.textContent = currentEffect ? currentEffect.name : "--";
    els.summaryEffectStatus.textContent = currentEffect ? (currentEffect.enabled ? "Enabled" : "Disabled") : "No effect";
    els.summaryInterval.textContent = pinned ? "Pinned" : `${Math.round(intervalMs / 1000)} sec`;
    els.summaryIntervalRemaining.textContent = remainingSec;
    els.summaryTopology.textContent = `${staticStats.ACTIVE_MATRIX_WIDTH}x${staticStats.ACTIVE_MATRIX_HEIGHT} / ${staticStats.ACTIVE_NUM_LEDS} leds`;
    els.summaryDriver.textContent = `${staticStats.ACTIVE_OUTPUT_DRIVER} / ${staticStats.ACTIVE_NUM_CHANNELS} ch`;
    els.summaryLedFps.textContent = formatNumber(dynamicStats.LED_FPS);
    els.summaryAudioFps.textContent = `Audio ${formatNumber(dynamicStats.AUDIO_FPS)} / Serial ${formatNumber(dynamicStats.SERIAL_FPS)}`;
    els.summaryCpu.textContent = `${formatPercent(dynamicStats.CPU_USED)}%`;
    els.summaryCpuCores.textContent = `C0 ${formatPercent(dynamicStats.CPU_USED_CORE0)}% / C1 ${formatPercent(dynamicStats.CPU_USED_CORE1)}%`;
    els.summaryHeap.textContent = formatBytes(dynamicStats.HEAP_FREE);
    els.summaryPsram.textContent = `PSRAM ${formatBytes(dynamicStats.PSRAM_FREE)}`;

    els.effectIntervalInput.value = String(Math.round(intervalMs / 1000));
    els.effectsMeta.textContent = `${effectList.length} effects / active ${currentIndex}`;
  }

  function renderEffects() {
    const effects = state.effects;
    if (!effects || !Array.isArray(effects.Effects)) {
      els.effectsTableBody.innerHTML = '<tr><td colspan="5" class="empty-cell">No effects loaded.</td></tr>';
      return;
    }

    const rows = effects.Effects.map((effect, index) => {
      const tr = document.createElement("tr");
      const isCurrent = index === Number(effects.currentEffect || 0);

      const onCell = document.createElement("td");
      const toggle = document.createElement("input");
      toggle.type = "checkbox";
      toggle.checked = !!effect.enabled;
      toggle.addEventListener("change", () => {
        postForm(effect.enabled ? "/disableEffect" : "/enableEffect", { effectIndex: index }).then(loadEffectsOnly).catch((error) => {
          toggle.checked = !!effect.enabled;
          handleError("Failed to toggle effect", error);
        });
      });
      onCell.appendChild(toggle);
      tr.appendChild(onCell);

      const nameCell = document.createElement("td");
      nameCell.innerHTML = `<span class="effect-name">${escapeHtml(effect.name || `Effect ${index}`)}</span>`;
      tr.appendChild(nameCell);

      const statusCell = document.createElement("td");
      statusCell.className = isCurrent ? "effect-active" : "effect-disabled";
      statusCell.textContent = isCurrent ? "Active" : (effect.enabled ? "Queued" : "Disabled");
      tr.appendChild(statusCell);

      const coreCell = document.createElement("td");
      coreCell.textContent = effect.core ? "Core" : "User";
      tr.appendChild(coreCell);

      const actionsCell = document.createElement("td");
      actionsCell.className = "row-actions";
      actionsCell.appendChild(miniButton("Trigger", () => postForm("/currentEffect", { currentEffectIndex: index }).then(loadEffectsOnly), !effect.enabled));
      actionsCell.appendChild(miniButton("Settings", () => openEffectDialog(index)));
      actionsCell.appendChild(miniButton("Up", () => moveEffect(index, Math.max(0, index - 1)), index === 0));
      actionsCell.appendChild(miniButton("Down", () => moveEffect(index, index + 1), index === effects.Effects.length - 1));
      actionsCell.appendChild(miniButton("Delete", () => deleteEffect(index), !!effect.core));
      tr.appendChild(actionsCell);

      return tr;
    });

    els.effectsTableBody.replaceChildren(...rows);
  }

  function renderSettingsForm() {
    if (!state.settings || !Array.isArray(state.settingsSpecs)) {
      return;
    }

    const fragment = document.createDocumentFragment();
    state.settingsSpecs.forEach((spec) => {
      if (spec.writeOnly && !spec.hasValidation) {
        return;
      }
      const currentValue = Object.prototype.hasOwnProperty.call(state.settings, spec.name)
        ? state.settings[spec.name]
        : defaultValueForSpec(spec);
      fragment.appendChild(buildSettingField(spec, currentValue, state.deviceDraft, state.deviceErrors, false));
    });

    els.deviceSettingsForm.replaceChildren(fragment);
  }

  function renderEffectDialogForm() {
    const dialog = state.effectDialog;
    if (!dialog.open) {
      return;
    }
    const fragment = document.createDocumentFragment();
    dialog.specs.forEach((spec) => {
      const currentValue = Object.prototype.hasOwnProperty.call(dialog.values, spec.name)
        ? dialog.values[spec.name]
        : defaultValueForSpec(spec);
      fragment.appendChild(buildSettingField(spec, currentValue, dialog.draft, dialog.errors, true));
    });
    els.effectDialogTitle.textContent = `${dialog.effectName} Settings`;
    els.effectSettingsForm.replaceChildren(fragment);
  }

  function buildSettingField(spec, currentValue, draftStore, errorSet, isEffectDialog) {
    const wrapper = document.createElement("label");
    wrapper.className = "field" + (spec.type === settingType.String && String(currentValue || "").length > 80 ? " full-span" : "");

    const title = document.createElement("span");
    title.textContent = spec.friendlyName || spec.name;
    wrapper.appendChild(title);

    const key = spec.name;
    const currentDraft = Object.prototype.hasOwnProperty.call(draftStore, key) ? draftStore[key] : currentValue;
    const readOnly = !!spec.readOnly;

    const setDraftValue = (value) => {
      if (valuesEqual(value, currentValue)) {
        delete draftStore[key];
      } else {
        draftStore[key] = value;
      }
    };

    const setFieldError = (hasError, message) => {
      const help = wrapper.querySelector(".field-help");
      if (hasError) {
        errorSet.add(key);
        help.textContent = message;
        help.classList.add("field-error");
      } else {
        errorSet.delete(key);
        help.innerHTML = spec.description || "";
        help.classList.remove("field-error");
      }
    };

    let control;
    switch (spec.type) {
      case settingType.Boolean: {
        wrapper.classList.add("full-span");
        control = document.createElement("input");
        control.type = "checkbox";
        control.checked = !!currentDraft;
        control.disabled = readOnly;
        control.addEventListener("change", () => {
          setDraftValue(!!control.checked);
          setFieldError(false, "");
        });
        const toggle = document.createElement("div");
        toggle.className = "toggle";
        toggle.appendChild(control);
        const toggleText = document.createElement("span");
        toggleText.textContent = spec.friendlyName || spec.name;
        toggle.appendChild(toggleText);
        wrapper.replaceChildren(toggle);
        break;
      }

      case settingType.Color: {
        const row = document.createElement("div");
        row.className = "color-row";
        control = document.createElement("input");
        control.type = "color";
        control.value = colorIntToHex(currentDraft);
        control.disabled = readOnly;
        const numeric = document.createElement("input");
        numeric.type = "number";
        numeric.className = "color-value";
        numeric.value = String(currentDraft);
        numeric.disabled = readOnly;
        control.addEventListener("input", () => {
          const intValue = hexToColorInt(control.value);
          numeric.value = String(intValue);
          setDraftValue(intValue);
          setFieldError(false, "");
        });
        numeric.addEventListener("change", () => {
          const intValue = clampInt(numeric.value, 0, 16777215, currentValue);
          numeric.value = String(intValue);
          control.value = colorIntToHex(intValue);
          setDraftValue(intValue);
          setFieldError(false, "");
        });
        row.appendChild(control);
        row.appendChild(numeric);
        wrapper.appendChild(row);
        break;
      }

      case settingType.Palette: {
        const row = document.createElement("div");
        row.className = "palette-row";
        const palette = Array.isArray(currentDraft) ? currentDraft : [];
        palette.forEach((entry, index) => {
          const box = document.createElement("div");
          box.className = "palette-entry";
          const color = document.createElement("input");
          color.type = "color";
          color.value = colorIntToHex(entry);
          color.disabled = readOnly;
          const value = document.createElement("input");
          value.type = "number";
          value.className = "color-value";
          value.value = String(entry);
          value.disabled = readOnly;
          const updatePalette = (intValue) => {
            const next = palette.slice();
            next[index] = intValue;
            setDraftValue(next);
            setFieldError(false, "");
          };
          color.addEventListener("input", () => {
            const intValue = hexToColorInt(color.value);
            value.value = String(intValue);
            updatePalette(intValue);
          });
          value.addEventListener("change", () => {
            const intValue = clampInt(value.value, 0, 16777215, entry);
            value.value = String(intValue);
            color.value = colorIntToHex(intValue);
            updatePalette(intValue);
          });
          box.appendChild(color);
          box.appendChild(value);
          row.appendChild(box);
        });
        wrapper.appendChild(row);
        break;
      }

      case settingType.Slider: {
        control = document.createElement("input");
        control.type = "range";
        control.min = spec.minimumValue ?? 0;
        control.max = spec.maximumValue ?? 255;
        control.value = currentDraft;
        control.disabled = readOnly;
        const output = document.createElement("input");
        output.type = "number";
        output.value = String(currentDraft);
        output.disabled = readOnly;
        output.addEventListener("change", () => {
          const intValue = clampInt(output.value, Number(control.min), Number(control.max), currentValue);
          output.value = String(intValue);
          control.value = String(intValue);
          setDraftValue(intValue);
          setFieldError(false, "");
        });
        control.addEventListener("input", () => {
          output.value = control.value;
          setDraftValue(clampInt(control.value, Number(control.min), Number(control.max), currentValue));
          setFieldError(false, "");
        });
        const row = document.createElement("div");
        row.className = "color-row";
        row.appendChild(control);
        row.appendChild(output);
        wrapper.appendChild(row);
        break;
      }

      default: {
        control = document.createElement(spec.type === settingType.String ? "textarea" : "input");
        if (control.tagName === "INPUT") {
          control.type = spec.type === settingType.Float || spec.type === settingType.Integer || spec.type === settingType.PositiveBigInteger
            ? "number"
            : "text";
          if (spec.minimumValue !== undefined) control.min = spec.minimumValue;
          if (spec.maximumValue !== undefined) control.max = spec.maximumValue;
          if (spec.type === settingType.Integer || spec.type === settingType.PositiveBigInteger) control.step = "1";
        }
        control.value = currentDraft ?? "";
        control.readOnly = readOnly;
        control.addEventListener("change", () => {
          const validation = validateFieldValue(spec, control.value);
          if (!validation.valid) {
            setFieldError(true, validation.message);
            return;
          }
          const coerced = coerceFieldValue(spec, control.value);
          setDraftValue(coerced);
          setFieldError(false, "");
        });
        wrapper.appendChild(control);
        break;
      }
    }

    const help = document.createElement("div");
    help.className = "field-help";
    help.innerHTML = spec.description || "";
    wrapper.appendChild(help);

    if (isEffectDialog) {
      wrapper.dataset.dialogField = "1";
    }

    return wrapper;
  }

  function validateFieldValue(spec, rawValue) {
    if (spec.type === settingType.String) {
      if (!spec.emptyAllowed && rawValue === "") {
        return { valid: false, message: "Empty value is not allowed." };
      }
      return { valid: true, message: "" };
    }

    if (spec.type === settingType.Integer || spec.type === settingType.PositiveBigInteger || spec.type === settingType.Float || spec.type === settingType.Slider) {
      if (rawValue === "") {
        return { valid: false, message: "Value is required." };
      }
      const numeric = Number(rawValue);
      if (Number.isNaN(numeric)) {
        return { valid: false, message: "Value must be numeric." };
      }
      if (spec.minimumValue !== undefined && numeric < spec.minimumValue) {
        return { valid: false, message: `Value must be at least ${spec.minimumValue}.` };
      }
      if (spec.maximumValue !== undefined && numeric > spec.maximumValue) {
        return { valid: false, message: `Value must be at most ${spec.maximumValue}.` };
      }
    }

    return { valid: true, message: "" };
  }

  function coerceFieldValue(spec, rawValue) {
    switch (spec.type) {
      case settingType.Integer:
      case settingType.PositiveBigInteger:
      case settingType.Slider:
        return Math.trunc(Number(rawValue));
      case settingType.Float:
        return Number(rawValue);
      default:
        return rawValue;
    }
  }

  async function applyEffectInterval() {
    const seconds = clampInt(els.effectIntervalInput.value, 0, 2147483, 0);
    try {
      await postForm("/settings", { effectInterval: seconds * 1000 });
      toast(`Effect interval set to ${seconds} sec.`, "success");
      await Promise.all([loadEffectsOnly(), loadSettingsOnly()]);
    } catch (error) {
      handleError("Failed to set effect interval", error);
    }
  }

  async function resetDevice(payload) {
    try {
      await postForm("/reset", payload);
      toast("Reset request sent.", "success");
    } catch (error) {
      handleError("Failed to send reset request", error);
    }
  }

  async function moveEffect(effectIndex, newIndex) {
    try {
      await postForm("/moveEffect", { effectIndex, newIndex });
      await loadEffectsOnly();
    } catch (error) {
      handleError("Failed to move effect", error);
    }
  }

  async function deleteEffect(effectIndex) {
    if (!window.confirm("Delete this effect?")) {
      return;
    }
    try {
      await postForm("/deleteEffect", { effectIndex });
      await loadEffectsOnly();
    } catch (error) {
      handleError("Failed to delete effect", error);
    }
  }

  async function applyDeviceSettings(rebootAfter) {
    if (state.deviceErrors.size > 0) {
      toast("Fix invalid settings before applying.", "error");
      return;
    }

    const payload = { ...state.deviceDraft };
    if (Object.keys(payload).length === 0) {
      toast("No device setting changes to apply.", "success");
      return;
    }

    try {
      await postForm("/settings", payload);
      toast("Device settings applied.", "success");
      await loadSettingsOnly();
      if (rebootAfter) {
        await resetDevice({ board: 1 });
      }
    } catch (error) {
      handleError("Failed to apply device settings", error);
    }
  }

  async function openEffectDialog(effectIndex) {
    try {
      const [specs, values] = await Promise.all([
        fetchJson(`/settings/effect/specs?effectIndex=${encodeURIComponent(effectIndex)}`),
        fetchJson(`/settings/effect?effectIndex=${encodeURIComponent(effectIndex)}`)
      ]);
      const effect = state.effects && Array.isArray(state.effects.Effects) ? state.effects.Effects[effectIndex] : null;
      state.effectDialog = {
        open: true,
        effectIndex,
        effectName: effect ? effect.name : `Effect ${effectIndex}`,
        specs,
        values,
        draft: {},
        errors: new Set()
      };
      renderEffectDialogForm();
      if (!els.effectSettingsDialog.open) {
        els.effectSettingsDialog.showModal();
      }
    } catch (error) {
      handleError("Failed to load effect settings", error);
    }
  }

  function closeEffectDialog() {
    state.effectDialog.open = false;
    state.effectDialog.draft = {};
    state.effectDialog.errors = new Set();
    if (els.effectSettingsDialog.open) {
      els.effectSettingsDialog.close();
    }
  }

  async function applyEffectSettings() {
    const dialog = state.effectDialog;
    if (!dialog.open) {
      return;
    }
    if (dialog.errors.size > 0) {
      toast("Fix invalid effect settings before applying.", "error");
      return;
    }
    const payload = { effectIndex: dialog.effectIndex, ...dialog.draft };
    if (Object.keys(dialog.draft).length === 0) {
      closeEffectDialog();
      return;
    }
    try {
      await postForm("/settings/effect", payload);
      toast("Effect settings applied.", "success");
      closeEffectDialog();
      await loadEffectsOnly();
    } catch (error) {
      handleError("Failed to apply effect settings", error);
    }
  }

  function renderStats() {
    const staticStats = state.staticStats;
    const dynamicStats = state.dynamicStats;
    const unifiedSettings = state.unifiedSettings;
    if (!staticStats || !dynamicStats || !unifiedSettings) {
      return;
    }

    const cards = [];
    cards.push(statCard("Output", [
      ["Compiled driver", staticStats.COMPILED_OUTPUT_DRIVER],
      ["Active driver", staticStats.ACTIVE_OUTPUT_DRIVER],
      ["Configured", `${staticStats.CONFIGURED_MATRIX_WIDTH}x${staticStats.CONFIGURED_MATRIX_HEIGHT}`],
      ["Active", `${staticStats.ACTIVE_MATRIX_WIDTH}x${staticStats.ACTIVE_MATRIX_HEIGHT}`],
      ["LEDs", `${staticStats.ACTIVE_NUM_LEDS} / ${staticStats.COMPILED_NUM_LEDS}`],
      ["Channels", `${staticStats.ACTIVE_NUM_CHANNELS} / ${staticStats.COMPILED_NUM_CHANNELS}`]
    ]));

    cards.push(statCard("Audio", [
      ["Mode", staticStats.AUDIO_INPUT_MODE],
      ["Configured pin", staticStats.CONFIGURED_AUDIO_INPUT_PIN],
      ["Compiled pin", staticStats.COMPILED_AUDIO_INPUT_PIN],
      ["Audio FPS", formatNumber(dynamicStats.AUDIO_FPS)],
      ["Frames socket", truthy(staticStats.FRAMES_SOCKET)],
      ["Effects socket", truthy(staticStats.EFFECTS_SOCKET)]
    ]));

    cards.push(statCard("CPU", [
      ["Total", `${formatPercent(dynamicStats.CPU_USED)}%`],
      ["Core 0", `${formatPercent(dynamicStats.CPU_USED_CORE0)}%`],
      ["Core 1", `${formatPercent(dynamicStats.CPU_USED_CORE1)}%`],
      ["LED FPS", formatNumber(dynamicStats.LED_FPS)],
      ["Serial FPS", formatNumber(dynamicStats.SERIAL_FPS)]
    ], dynamicStats.CPU_USED));

    cards.push(statCard("Memory", [
      ["Heap free", formatBytes(dynamicStats.HEAP_FREE)],
      ["Heap size", formatBytes(staticStats.HEAP_SIZE)],
      ["DMA free", formatBytes(dynamicStats.DMA_FREE)],
      ["DMA size", formatBytes(staticStats.DMA_SIZE)],
      ["PSRAM free", formatBytes(dynamicStats.PSRAM_FREE)],
      ["PSRAM size", formatBytes(staticStats.PSRAM_SIZE)]
    ], memoryUsagePercent(dynamicStats.HEAP_FREE, staticStats.HEAP_SIZE)));

    cards.push(statCard("Package", [
      ["Chip", staticStats.CHIP_MODEL],
      ["Cores", staticStats.CHIP_CORES],
      ["Clock", `${staticStats.CHIP_SPEED} MHz`],
      ["Program", formatBytes(staticStats.PROG_SIZE)],
      ["Flash", formatBytes(staticStats.FLASH_SIZE)],
      ["Code free", formatBytes(staticStats.CODE_FREE)]
    ]));

    cards.push(statCard("Schema", [
      ["Topology live", truthy(unifiedSettings.topology.liveApply)],
      ["Output live", truthy(unifiedSettings.outputs.liveApply)],
      ["Audio live", truthy(unifiedSettings.device.audio.liveApply)],
      ["Audio reboot", truthy(unifiedSettings.device.audio.requiresReboot)],
      ["Remote enabled", truthy(unifiedSettings.device.remote.enabled)],
      ["Remote pin", unifiedSettings.device.remote.pin]
    ]));

    els.statsGrid.replaceChildren(...cards);
    els.statsTimestamp.textContent = `Updated ${new Date().toLocaleTimeString()}`;
  }

  function statCard(title, rows, meterPercent) {
    const card = document.createElement("div");
    card.className = "stat-card";
    const heading = document.createElement("h3");
    heading.textContent = title;
    card.appendChild(heading);
    if (meterPercent !== undefined) {
      const meter = document.createElement("div");
      meter.className = "meter";
      const fill = document.createElement("span");
      fill.style.width = `${Math.max(0, Math.min(100, meterPercent))}%`;
      meter.appendChild(fill);
      card.appendChild(meter);
    }
    const list = document.createElement("div");
    list.className = "stat-list";
    rows.forEach(([label, value]) => {
      const row = document.createElement("div");
      row.className = "stat-row";
      row.innerHTML = `<span>${escapeHtml(String(label))}</span><strong>${escapeHtml(String(value))}</strong>`;
      list.appendChild(row);
    });
    card.appendChild(list);
    return card;
  }

  function connectPreviewSocket() {
    if (state.preview.socket) {
      return;
    }

    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const socket = new WebSocket(`${protocol}//${window.location.host}/frames`);
    socket.binaryType = "arraybuffer";
    socket.onopen = function () {
      state.preview.connected = true;
      els.previewStatus.textContent = "Preview connected";
      toast("Frame preview connected.", "success");
    };
    socket.onclose = function () {
      state.preview.connected = false;
      state.preview.socket = null;
      els.previewStatus.textContent = "Preview offline";
    };
    socket.onerror = function () {
      handleError("Preview socket error");
    };
    socket.onmessage = function (event) {
      try {
        state.preview.frame = new Uint8Array(event.data);
        drawPreviewFrame();
      } catch (error) {
        handleError("Failed to render preview frame", error);
      }
    };
    state.preview.socket = socket;
  }

  function disconnectPreviewSocket() {
    if (state.preview.socket) {
      state.preview.socket.close();
      state.preview.socket = null;
    }
    state.preview.connected = false;
    els.previewStatus.textContent = "Preview offline";
  }

  function drawPreviewFrame() {
    const frame = state.preview.frame;
    const staticStats = state.staticStats;
    const canvas = els.previewCanvas;
    if (!canvas || !frame || !staticStats) {
      return;
    }

    const width = Number(staticStats.ACTIVE_MATRIX_WIDTH || staticStats.CONFIGURED_MATRIX_WIDTH || 1);
    const height = Number(staticStats.ACTIVE_MATRIX_HEIGHT || staticStats.CONFIGURED_MATRIX_HEIGHT || 1);
    canvas.width = width;
    canvas.height = height;

    const ctx = canvas.getContext("2d");
    const imageData = ctx.createImageData(width, height);
    let pixel = 0;
    for (let i = 0; i < frame.length && pixel < imageData.data.length; i += 3) {
      imageData.data[pixel++] = frame[i] || 0;
      imageData.data[pixel++] = frame[i + 1] || 0;
      imageData.data[pixel++] = frame[i + 2] || 0;
      imageData.data[pixel++] = 255;
    }
    ctx.putImageData(imageData, 0, 0);
  }

  function miniButton(label, handler, disabled) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "mini-button";
    button.textContent = label;
    button.disabled = !!disabled;
    button.addEventListener("click", handler);
    return button;
  }

  async function fetchJson(path, options) {
    const response = await fetch(path, options);
    if (!response.ok) {
      throw await buildHttpError(response);
    }
    return response.json();
  }

  async function postForm(path, payload) {
    const body = new URLSearchParams();
    Object.entries(payload || {}).forEach(([key, value]) => {
      if (value === undefined || value === null) {
        return;
      }
      if (Array.isArray(value)) {
        body.append(key, JSON.stringify(value));
        return;
      }
      if (typeof value === "object") {
        body.append(key, JSON.stringify(value));
        return;
      }
      body.append(key, String(value));
    });
    const response = await fetch(path, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8" },
      body
    });
    if (!response.ok) {
      throw await buildHttpError(response);
    }
    const contentType = response.headers.get("content-type") || "";
    if (contentType.includes("application/json") || contentType.includes("text/json")) {
      return response.json();
    }
    return null;
  }

  async function buildHttpError(response) {
    let message = `HTTP ${response.status}`;
    try {
      const contentType = response.headers.get("content-type") || "";
      if (contentType.includes("json")) {
        const payload = await response.json();
        if (payload && payload.message) {
          message = payload.message;
        }
      } else {
        const text = await response.text();
        if (text) {
          message = text;
        }
      }
    } catch (_error) {
      // Ignore parse failure
    }
    return new Error(message);
  }

  function toast(message, level) {
    const node = document.createElement("div");
    node.className = `toast ${level || ""}`.trim();
    node.textContent = message;
    els.toastStack.appendChild(node);
    window.setTimeout(() => node.remove(), 4200);
  }

  function handleError(context, error) {
    const message = error && error.message ? error.message : String(context || "Unexpected error");
    console.error(context, error);
    toast(context ? `${context}: ${message}` : message, "error");
  }

  function clampInt(value, min, max, fallback) {
    const numeric = Number(value);
    if (Number.isNaN(numeric)) {
      return fallback;
    }
    return Math.max(min, Math.min(max, Math.trunc(numeric)));
  }

  function formatNumber(value) {
    return Number(value || 0).toFixed(0);
  }

  function formatPercent(value) {
    return Number(value || 0).toFixed(1);
  }

  function formatBytes(bytes) {
    const value = Number(bytes || 0);
    if (value >= 1024 * 1024) {
      return `${(value / (1024 * 1024)).toFixed(1)} MB`;
    }
    if (value >= 1024) {
      return `${(value / 1024).toFixed(1)} KB`;
    }
    return `${value} B`;
  }

  function memoryUsagePercent(free, size) {
    const total = Number(size || 0);
    if (!total) {
      return 0;
    }
    const used = total - Number(free || 0);
    return (used / total) * 100;
  }

  function truthy(value) {
    return value ? "Yes" : "No";
  }

  function valuesEqual(left, right) {
    return JSON.stringify(left) === JSON.stringify(right);
  }

  function defaultValueForSpec(spec) {
    if (spec.type === settingType.Boolean) {
      return false;
    }
    if (spec.type === settingType.Palette) {
      return [];
    }
    if (spec.type === settingType.Color) {
      return 0;
    }
    return "";
  }

  function colorIntToHex(value) {
    const numeric = clampInt(value, 0, 16777215, 0);
    return `#${numeric.toString(16).padStart(6, "0")}`;
  }

  function hexToColorInt(value) {
    return parseInt(String(value || "#000000").replace("#", ""), 16) || 0;
  }

  function escapeHtml(value) {
    return String(value)
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#39;");
  }
})();
