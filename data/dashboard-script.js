// --- WebSocket ---
var gateway = `ws://${window.location.hostname}/ws`; //`ws://citerne.local/ws`;
var websocket;
let refreshTimer = null;
let historicalChart = null;
let availableCsvDates = new Set();
let latestCsvDate = null;
let csvPathByDate = new Map();
let measurePeriodMs = 60000;
let lastMeasureStatusFetchMs = 0;
let isFirstMessage = true;
let bootstrapLoaded = false;
let bootstrapInFlight = null;
let bootstrapFallbackTimer = null;
const DATA_RANGE = {
    DAY: "day",
    MONTH: "month",
    YEAR: "year"
};

const STATUS_CLASS_NAMES = ["status-ok", "status-warn", "status-error", "status-unknown"];

function delayMs(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

async function fetchWithRetry(url, options = {}, retries = 2, retryDelayMs = 300) {
    let lastError = null;

    console.debug("fetchWithRetry:start", {
        url,
        method: options.method || "GET",
        retries,
        origin: window.location.origin
    });

    for (let attempt = 0; attempt <= retries; attempt++) {
        try {
            const response = await fetch(url, options);
            console.debug("fetchWithRetry:response", {
                url,
                attempt,
                ok: response.ok,
                status: response.status
            });
            return response;
        } catch (err) {
            lastError = err;
            console.warn("fetchWithRetry:error", {
                url,
                attempt,
                message: err?.message || String(err)
            });
            if (attempt < retries) {
                await delayMs(retryDelayMs);
            }
        }
    }

    throw lastError || new Error("Load failed");
}

function isDayAvailable(date) {
    return availableCsvDates.has(formatDateForCsv(date));
}

const calendar = flatpickr("#calendar", {
    dateFormat: "Y-m-d",
    defaultDate: "today",
    enable: [isDayAvailable],
    onChange: function(selectedDates, dateStr) {
        updateTimeRange();
    },
    //plugins: [ new monthSelectPlugin({ shorthand: true }) ] // plugin pour mois/année
});

window.addEventListener('load', () => { 
    setStatusChip("wifiStatus", "wifiStatusText", "warn", "Connexion...");
    setStatusChip("sensorStatus", "sensorStatusText", "unknown", "Inconnu");
    setStatusChip("sdStatus", "sdStatusText", "unknown", "Inconnu");
    initWebSocket(); 
    startAutoRefresh(30000, false); // 30 secondes

    // Keep first paint focused on live WS data; if WS is slow, bootstrap via HTTP.
    bootstrapFallbackTimer = setTimeout(() => {
        ensureBootstrapDataLoaded("startup-timeout");
    }, 1500);
});

window.addEventListener('DOMContentLoaded', () => {
    initChart();
    document.getElementById("rebootBtn").addEventListener("click", rebootESP);
    document.getElementById("loadTodayBtn").addEventListener("click", loadToday);
    document.getElementById("openSettingsBtn").addEventListener("click", () => {
        window.location.href = "/settings";
    });
    const openFileManagerBtn = document.getElementById("openFileManagerBtn");
    if (openFileManagerBtn) {
        openFileManagerBtn.addEventListener("click", () => {
            window.location.href = "/file-manager";
        });
    }
    document.getElementById("timeRangeSelect").addEventListener("change", (e) => {
        updateTimeRange();
        console.log("Nouvelle valeur :", e.target.value);
    });
    document.getElementById("forceMeasurementNowBtn").addEventListener("click", () => {
        forceMeasurementNow();
    });
});

async function refreshMeasurePeriod(force = false) {
    const nowMs = Date.now();
    if (!force && (nowMs - lastMeasureStatusFetchMs) < 120000) {
        return;
    }

    try {
        console.debug("refreshMeasurePeriod:request", { force, url: "/measure-status" });
        const res = await fetchWithRetry('/measure-status', { cache: 'no-store' }, 1, 250);
        if (!res.ok) {
            console.warn("refreshMeasurePeriod:non-ok", { status: res.status });
            return;
        }
        const status = await res.json();
        const candidate = Number(status.measurePeriodMs);
        if (Number.isFinite(candidate) && candidate > 0) {
            measurePeriodMs = candidate;
        }
        console.log('Mesure de la période mise à jour:', measurePeriodMs, 'ms');
        lastMeasureStatusFetchMs = nowMs;
    } catch (err) {
        console.warn('Impossible de récupérer /measure-status', err);
    }
}

function parseMeasurementDateTime(rawTime) {
    if (!rawTime || rawTime === '--') {
        return null;
    }

    const normalized = String(rawTime).trim();

    // ESP32 sends local time as "dd/mm/yyyy HH:MM:SS".
    const frMatch = normalized.match(/^(\d{2})\/(\d{2})\/(\d{4})\s+(\d{2}):(\d{2}):(\d{2})$/);
    if (frMatch) {
        const [, day, month, year, hour, minute, second] = frMatch;
        const parsed = new Date(
            Number(year),
            Number(month) - 1,
            Number(day),
            Number(hour),
            Number(minute),
            Number(second)
        );
        if (!Number.isNaN(parsed.getTime())) {
            return parsed;
        }
    }

    const isoCandidate = normalized.includes(' ') ? normalized.replace(' ', 'T') : normalized;
    const parsed = new Date(isoCandidate);
    if (!Number.isNaN(parsed.getTime())) {
        return parsed;
    }

    return null;
}

function renderNextMeasure(lastMeasureTime) {
    const time = parseMeasurementDateTime(lastMeasureTime);
    const nextMeasureEl = document.getElementById('nextMeasureTime');
    if (!time || Number.isNaN(time.getTime()) || !nextMeasureEl) {
        if (nextMeasureEl) {
            nextMeasureEl.textContent = '--:--:--';
        }
        return;
    }

    const nextDate = new Date(time.getTime() + measurePeriodMs);
    nextMeasureEl.textContent = nextDate.toLocaleTimeString('fr-FR', {
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
}

async function initNextMeasureCalculation(lastMeasureTime) {
    await refreshMeasurePeriod(true);
    renderNextMeasure(lastMeasureTime);
}

function updateNextMeasureFromWebSocket(lastMeasureTime) {
    refreshMeasurePeriod(false);
    renderNextMeasure(lastMeasureTime);
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    console.log('WebSocket gateway:', gateway);
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onerror = onError;
    websocket.onmessage = onMessage;
}


function setStatusChip(chipId, textId, state, text) {
    const chip = document.getElementById(chipId);
    const textEl = document.getElementById(textId);
    if (!chip || !textEl) return;

    chip.classList.remove(...STATUS_CLASS_NAMES);

    if (state === "ok") {
        chip.classList.add("status-ok");
    } else if (state === "warn") {
        chip.classList.add("status-warn");
    } else if (state === "error") {
        chip.classList.add("status-error");
    } else {
        chip.classList.add("status-unknown");
    }

    textEl.textContent = text;
}

async function refreshSystemIndicators() {
    try {
        console.debug("refreshSystemIndicators:request", {
            measureUrl: "/measure-status",
            sdUrl: "/sd-status"
        });
        const [measureRes, sdRes] = await Promise.all([
            fetchWithRetry('/measure-status', { cache: 'no-store' }, 1, 250),
            fetchWithRetry('/sd-status', { cache: 'no-store' }, 1, 250)
        ]);

        if (measureRes.ok) {
            const measure = await measureRes.json();
            const sensorOk = !!measure.sensorConnected;
            setStatusChip(
                "sensorStatus",
                "sensorStatusText",
                sensorOk ? "ok" : "error",
                sensorOk ? "Connecte" : "Erreur"
            );
        } else {
            setStatusChip("sensorStatus", "sensorStatusText", "warn", "Non lu");
        }

        if (sdRes.ok || sdRes.status === 503) {
            const sd = await sdRes.json();
            const sdOk = !!sd.sdOK;
            setStatusChip(
                "sdStatus",
                "sdStatusText",
                sdOk ? "ok" : "error",
                sdOk ? "OK" : "Indispo"
            );
        } else {
            setStatusChip("sdStatus", "sdStatusText", "warn", "Non lu");
        }
    } catch (err) {
        console.warn('Erreur refreshSystemIndicators', err);
        setStatusChip("sensorStatus", "sensorStatusText", "warn", "Hors ligne");
        setStatusChip("sdStatus", "sdStatusText", "warn", "Hors ligne");
    }
}

async function ensureBootstrapDataLoaded(reason = "manual") {
    if (bootstrapLoaded) {
        return;
    }

    if (bootstrapInFlight) {
        await bootstrapInFlight;
        return;
    }

    bootstrapInFlight = (async () => {
        await Promise.all([
            loadFileList(),
            refreshSystemIndicators()
        ]);

        const mode = document.getElementById("timeRangeSelect")?.value;
        if (mode === DATA_RANGE.DAY) {
            ensureValidSelectedDate();
            const selected = calendar.input.value;
            if (selected && /^\d{4}-\d{2}-\d{2}$/.test(selected)) {
                const year = selected.slice(0, 4);
                const month = selected.slice(5, 7);
                const day = selected.slice(8, 10);
                await loadDay(year, month, day);
            }
        }

        bootstrapLoaded = true;
        console.log("Bootstrap data loaded:", reason);
    })();

    try {
        await bootstrapInFlight;
    } finally {
        bootstrapInFlight = null;
    }
}

function getReadings() { 
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("getReadings");
    }
}
function onOpen() { 
    console.log('Connection opened'); 
    setStatusChip("wifiStatus", "wifiStatusText", "ok", "Connecte");
    isFirstMessage = true;
    getReadings(); 
}
function onClose() { 
    console.log('Connection closed'); 
    setStatusChip("wifiStatus", "wifiStatusText", "warn", "Reconnexion");
    setTimeout(initWebSocket, 2000); 
}

function onError(event) {
    console.warn('WebSocket error', event);
    setStatusChip("wifiStatus", "wifiStatusText", "error", "Erreur");
}

function rebootESP() {
    if (!confirm(`Redémarrer l'ESP32 ?`)) {
        return;
    }
    fetch("/reboot", { method: "POST" })
        .then(res => res.text())
        .then(msg => alert(msg))
        .catch(() => alert("Erreur de communication"));
}

function forceMeasurementNow() {
    fetch("/force-measure", { method: "POST" })
        .then(res => res.text())
        .then(msg => alert(msg))
        .catch(() => alert("Erreur de communication"));
}


function updateTable(data) {
    let tbody = document.querySelector('#history tbody');
    tbody.innerHTML = '';
    for (let i = data.history.length - 1; i >= 0; i--) {
        let row = `<tr>
            <td>${i - data.history.length + 1}</td>
            <td>${data.history[i].time}</td>
            <td>${data.history[i].distance.toFixed(1)}</td>
            <td>${data.history[i].percent}</td>
            <td>${data.history[i].volume.toFixed(0)}</td>
        </tr>`;
        tbody.innerHTML += row;
    }
}

// --- Graphique ---

function initChart() {
    const ctx = document.getElementById('historicalChart').getContext('2d');
    historicalChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Niveau (%)',
                data: [],
                fill: true,
                backgroundColor: 'rgba(54,162,235,0.2)',
                borderColor: 'rgba(54,162,235,1)',
                tension: 0.3,
                pointRadius: 4
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            layout: {
                padding: {
                    bottom: 30   // évite que les labels touchent le bord
                }
            },
            scales: {
                y: {
                    min: 0,
                    max: 110,
                    title: {
                        display: true,
                        text: 'Pourcentage (%)'
                    }
                },
                x: {
                    title: {
                        display: true,
                        text: 'Heure'
                    },
                    ticks: {
                        maxRotation: 45,
                        minRotation: 0,
                        autoSkip: true,
                        maxTicksLimit: 12
                    }
                }
            }
        }
    });
}

function updateChart(labels, data) {
    historicalChart.data.labels = labels;
    historicalChart.data.datasets[0].data = data;
    historicalChart.update();
}

// --- Mise à jour graphique et tableau ---
function onMessage(event) {
    const data = JSON.parse(event.data);

    document.getElementById('time').textContent = data.time;
    document.getElementById('percent').textContent = data.percent + ' %';
    document.getElementById('volume').textContent = data.volume.toFixed(0) + ' L';
    
    // Initialiser la prochaine mesure au premier message
    if (isFirstMessage) {
        initNextMeasureCalculation(data.time);
        isFirstMessage = false;

        if (bootstrapFallbackTimer) {
            clearTimeout(bootstrapFallbackTimer);
            bootstrapFallbackTimer = null;
        }
        ensureBootstrapDataLoaded("first-websocket-message");
    } else {
        updateNextMeasureFromWebSocket(data.time);
    }

    updateTable(data);
    const activeRange = document.getElementById("timeRangeSelect").value;
    if (activeRange === DATA_RANGE.DAY && labels[labels.length - 1] !== data.time.split(" ")[1].split(":").slice(0, 2).join(":")) {
        const labels = historicalChart.data.labels;
        const values = historicalChart.data.datasets[0].data;
        labels.push(data.time.split(" ")[1].split(":").slice(0, 2).join(":"));
        values.push(data.percent);
        updateChart(labels, values);
    } else if (activeRange === DATA_RANGE.MONTH || activeRange === DATA_RANGE.YEAR) {
        // Recompute aggregates when a new measure lands so current-day/month means stay up to date.
        updateTimeRange();
    }
}

function normalizeCsvFileName(fileName) {
    let normalized = fileName.startsWith("/") ? fileName.slice(1) : fileName;
    const slashIndex = normalized.lastIndexOf("/");
    if (slashIndex >= 0) {
        normalized = normalized.slice(slashIndex + 1);
    }
    return normalized;
}

function isDailyCsvFileName(fileName) {
    return /^\d{4}-\d{2}-\d{2}\.csv$/.test(normalizeCsvFileName(fileName));
}

function extractDateFromCsv(fileName) {
    const normalized = normalizeCsvFileName(fileName);
    if (!isDailyCsvFileName(normalized)) {
        return null;
    }
    return normalized.slice(0, 10);
}

function formatDateForCsv(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, "0");
    const day = String(date.getDate()).padStart(2, "0");
    return `${year}-${month}-${day}`;
}

function normalizeCsvQueryFile(fileName) {
    if (typeof fileName !== "string") {
        return "";
    }
    return fileName.startsWith("/") ? fileName.slice(1) : fileName;
}

async function fetchCsvByCandidates(candidates) {
    const tried = new Set();

    for (const rawCandidate of candidates) {
        const candidate = normalizeCsvQueryFile(rawCandidate || "");
        if (!candidate || tried.has(candidate)) {
            continue;
        }
        tried.add(candidate);

        const response = await fetch(`/csv?file=${encodeURIComponent(candidate)}`, {
            cache: "no-store"
        });

        if (response.ok) {
            return {
                response,
                path: candidate
            };
        }
    }

    return null;
}

function parseMonthlySummaryCsv(csvText) {
    const lines = csvText.trim().split("\n").slice(1);
    const rows = [];

    for (const rawLine of lines) {
        const line = rawLine.trim();
        if (!line) {
            continue;
        }

        const cols = line.split(",");
        if (cols.length < 2) {
            continue;
        }

        const date = cols[0];
        const percent = parseFloat(cols[1]);
        const count = cols.length >= 5 ? parseInt(cols[4], 10) : 1;

        if (!/^\d{4}-\d{2}-\d{2}$/.test(date) || Number.isNaN(percent) || Number.isNaN(count) || count <= 0) {
            continue;
        }

        rows.push({ date, percent, count });
    }

    rows.sort((a, b) => a.date.localeCompare(b.date));
    return rows;
}

async function fetchMonthlySummaryRows(year, month) {
    const params = new URLSearchParams({
        year: String(year),
        month: String(month)
    });

    console.debug("fetchMonthlySummaryRows:request", {
        year,
        month,
        url: `/monthly-summary?${params.toString()}`
    });

    const response = await fetchWithRetry(`/monthly-summary?${params.toString()}`, {
        cache: "no-store"
    }, 1, 300);

    if (!response.ok) {
        console.warn("fetchMonthlySummaryRows:non-ok", {
            year,
            month,
            status: response.status
        });
        throw new Error(`monthly-summary failed (${response.status})`);
    }

    const csv = await response.text();
    return parseMonthlySummaryCsv(csv);
}

async function fetchYearFromMonthlySummaries(year) {
    const monthTasks = [];
    for (let month = 1; month <= 12; month++) {
        monthTasks.push(fetchMonthlySummaryRows(year, month).catch(() => []));
    }

    const monthRows = await Promise.all(monthTasks);
    const labels = [];
    const values = [];

    monthRows.forEach((rows, index) => {
        if (!rows || rows.length === 0) {
            return;
        }

        let weightedPercentSum = 0;
        let totalCount = 0;

        rows.forEach((row) => {
            weightedPercentSum += row.percent * row.count;
            totalCount += row.count;
        });

        if (totalCount <= 0) {
            return;
        }

        labels.push(`${year}-${String(index + 1).padStart(2, "0")}`);
        values.push(weightedPercentSum / totalCount);
    });

    return { labels, values };
}

function isFutureMonthSelection(year, month) {
    const selectedYear = Number(year);
    const selectedMonth = Number(month);
    if (!Number.isInteger(selectedYear) || !Number.isInteger(selectedMonth)) {
        return false;
    }

    const now = new Date();
    const currentYear = now.getFullYear();
    const currentMonth = now.getMonth() + 1;

    return selectedYear > currentYear || (selectedYear === currentYear && selectedMonth > currentMonth);
}

function isFutureYearSelection(year) {
    const selectedYear = Number(year);
    if (!Number.isInteger(selectedYear)) {
        return false;
    }

    return selectedYear > new Date().getFullYear();
}

function updateAvailableDatesFromFiles(files) {
    csvPathByDate = new Map();

    const dates = [];
    files.forEach((filePath) => {
        const date = extractDateFromCsv(filePath);
        if (!date) {
            return;
        }
        dates.push(date);
        if (!csvPathByDate.has(date)) {
            csvPathByDate.set(date, filePath);
        }
    });

    dates.sort();

    availableCsvDates = new Set(dates);
    latestCsvDate = dates.length > 0 ? dates[dates.length - 1] : null;

    // Re-apply day constraints only when day mode is active.
    if (document.getElementById("timeRangeSelect")?.value === DATA_RANGE.DAY) {
        calendar.set("enable", [isDayAvailable]);
    }
}

function ensureValidSelectedDate() {
    if (document.getElementById("timeRangeSelect").value !== DATA_RANGE.DAY) {
        return;
    }

    if (!latestCsvDate) {
        return;
    }

    const selected = calendar.input.value;
    if (!selected || !availableCsvDates.has(selected)) {
        calendar.setDate(latestCsvDate, true, "Y-m-d");
    }
}

async function loadToday() {
    await loadFileList();
    const fallback = formatDateForCsv(new Date());
    calendar.setDate(latestCsvDate || fallback, true, "Y-m-d");
}

async function loadDay(year, month, day) {
    const y = String(year || "");
    const m = String(month || "").padStart(2, "0");
    const d = String(day || "").padStart(2, "0");

    if (!/^\d{4}$/.test(y) || !/^\d{2}$/.test(m) || !/^\d{2}$/.test(d)) {
        console.warn("Date invalide pour loadDay:", year, month, day);
        return;
    }

    // dateStr attendu : "YYYY-MM-DD"
    const filename = `${y}-${m}-${d}.csv`;
    const dateKey = filename.slice(0, 10);
    const csvPath = csvPathByDate.get(dateKey) || filename;
    let res;
    try {
        const candidates = [
            csvPath,
            `history/${year}/${year}-${m}/${filename}`,
            filename
        ];

        const hit = await fetchCsvByCandidates(candidates);
        if (!hit) {
            console.warn("CSV introuvable :", filename);
            return;
        }

        res = hit.response;
    } catch (e) {
        console.error("Erreur fetch", e);
        return;
    }

    if (!res.ok) {
        console.warn("CSV introuvable :", filename);
        return;
    }

    const csv = await res.text();

    const lines = csv.trim().split("\n").slice(1);

    const labels = [];
    const data = [];

    for (const l of lines) {
        const cols = l.split(",");
        const value = parseFloat(cols[1]); // Niveau (%)

        if (!isNaN(value)) {
            labels.push(cols[0].split(" ")[1].split(":").slice(0, 2).join(":")); //HH:MM
            data.push(value);
        }
    }
    historicalChart.options.scales.x.title.text = 'Heure';
    updateChart(labels, data);
    console.log("Points chargés :", data.length);
}

async function loadMonth(year, month) {
    historicalChart.options.scales.x.title.text = 'Jour';

    const y = Number(year);
    const m = Number(month);
    if (!Number.isInteger(y) || !Number.isInteger(m) || m < 1 || m > 12) {
        console.warn("loadMonth invalide:", year, month);
        updateChart([], []);
        return;
    }

    if (isFutureMonthSelection(y, m)) {
        console.info("loadMonth:future-selection skipped", { year: y, month: m });
        updateChart([], []);
        return;
    }

    try {
        console.info("loadMonth:request", { year: y, month: m });
        const rows = await fetchMonthlySummaryRows(y, m);
        console.info("loadMonth:success", { year: y, month: m, rows: rows.length });

        const labels = rows.map((row) => row.date.slice(8, 10));
        const data = rows.map((row) => row.percent);

        updateChart(labels, data);
    } catch (err) {
        console.warn("loadMonth:error", {
            year: y,
            month: m,
            message: err?.message || String(err)
        });
        updateChart([], []);
    }
}

async function loadYear(year) {
    historicalChart.options.scales.x.title.text = 'Mois';

    const y = Number(year);
    if (!Number.isInteger(y)) {
        console.warn("loadYear invalide:", year);
        updateChart([], []);
        return;
    }

    if (isFutureYearSelection(y)) {
        console.info("loadYear:future-selection skipped", { year: y });
        updateChart([], []);
        return;
    }

    try {
        console.info("loadYear:request", { year: y });
        const aggregate = await fetchYearFromMonthlySummaries(y);
        console.info("loadYear:success", {
            year: y,
            labels: aggregate.labels.length,
            values: aggregate.values.length
        });
        updateChart(aggregate.labels, aggregate.values);
    } catch (err) {
        console.warn("loadYear:error", {
            year: y,
            message: err?.message || String(err)
        });
        updateChart([], []);
    }
}

async function loadFileList() {
    try {
        console.debug("loadFileList:request", { url: "/list-files" });
        const res = await fetchWithRetry("/list-files", { cache: "no-store" }, 2, 300);
        let files = await res.json();
        if (!Array.isArray(files)) {
            files = [];
        }

        updateAvailableDatesFromFiles(files);
        ensureValidSelectedDate();
    } 
    catch (e) {
        console.warn("loadFileList:error", {
            message: e?.message || String(e),
            origin: window.location.origin
        });
    }
}

function startAutoRefresh(intervalMs = 30000, runImmediately = true) {
    if (runImmediately) {
        ensureBootstrapDataLoaded("auto-refresh-immediate");
    }

    refreshTimer = setInterval(() => {
        if (!bootstrapLoaded) {
            ensureBootstrapDataLoaded("auto-refresh-before-bootstrap");
            return;
        }
        loadFileList();
        refreshSystemIndicators();
    }, intervalMs);
}

async function updateTimeRange(){
    var dateStr, year, month, day;
    switch (document.getElementById("timeRangeSelect").value) {
            case DATA_RANGE.DAY:
                calendar.set("enable", [isDayAvailable]);
                calendar.set("dateFormat", "Y-m-d");
                calendar.set("altFormat", "d-m-Y");
                ensureValidSelectedDate();
                dateStr = calendar.input.value;
                if (!/^\d{4}-\d{2}-\d{2}$/.test(dateStr || "")) {
                    const fallbackDate = latestCsvDate || formatDateForCsv(new Date());
                    calendar.setDate(fallbackDate, false, "Y-m-d");
                    dateStr = fallbackDate;
                }
                year  = dateStr.slice(0, 4);
                month = dateStr.slice(5, 7);
                day = dateStr.slice(8,10);
                loadDay(year, month, day);       // 2026-01-30
                break;

            case DATA_RANGE.MONTH:
                calendar.set("enable", [isDayAvailable]);
                calendar.set("dateFormat", "Y-m");
                calendar.set("altFormat", "F Y");
                dateStr = calendar.input.value;
                year  = dateStr.slice(0, 4);
                month = dateStr.slice(5, 7);
                loadMonth(year, month);       // 2026-01
                break;

            case DATA_RANGE.YEAR:
                calendar.set("enable", [isDayAvailable]);
                calendar.set("dateFormat", "Y");
                calendar.set("altFormat", "Y");
                dateStr = calendar.input.value;
                year  = dateStr.slice(0, 4);
                loadYear(year);         // 2026
                break;
        }
}