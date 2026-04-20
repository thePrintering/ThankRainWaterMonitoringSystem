const FM_PAGE_LIMIT = 40;
let fmCurrentDir = "/";
let fmOffset = 0;
let fmHasMore = false;
let fmStatusTimer = null;

function escapeHtml(str) {
    return String(str)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
}

function formatBytes(size) {
    const n = Number(size);
    if (!Number.isFinite(n) || n < 0) {
        return "--";
    }
    if (n < 1024) {
        return `${n} B`;
    }
    if (n < 1024 * 1024) {
        return `${(n / 1024).toFixed(1)} KB`;
    }
    return `${(n / (1024 * 1024)).toFixed(1)} MB`;
}

function fmSetStatus(text, type = "info", autoHideMs = 0) {
    const status = document.getElementById("fmStatus");
    if (!status) {
        return;
    }

    if (fmStatusTimer) {
        clearTimeout(fmStatusTimer);
        fmStatusTimer = null;
    }

    if (!text) {
        status.textContent = "";
        status.className = "fm-status";
        status.style.display = "none";
        return;
    }

    const safeType = ["info", "progress", "success", "error"].includes(type) ? type : "info";
    status.textContent = text;
    status.className = `fm-status fm-status--${safeType}`;
    status.style.display = "block";

    if (autoHideMs > 0 && safeType !== "error") {
        fmStatusTimer = setTimeout(() => {
            fmSetStatus("");
        }, autoHideMs);
    }
}

async function fmReadErrorText(response, fallbackText) {
    try {
        const txt = await response.text();
        return txt || fallbackText;
    } catch (_) {
        return fallbackText;
    }
}

function fmNormalizeDirPath(path) {
    const raw = String(path || "/").trim();
    if (!raw || raw === "/") {
        return "/";
    }
    let normalized = raw.startsWith("/") ? raw : `/${raw}`;
    normalized = normalized.replace(/\/+/g, "/");
    if (normalized.length > 1 && normalized.endsWith("/")) {
        normalized = normalized.slice(0, -1);
    }
    return normalized;
}

function fmParentDir(path) {
    const normalized = fmNormalizeDirPath(path);
    if (normalized === "/") {
        return "/";
    }
    const idx = normalized.lastIndexOf("/");
    return idx > 0 ? normalized.slice(0, idx) : "/";
}

function fmSetButtonAvailability(button, available, hideWhenUnavailable = true) {
    if (!button) {
        return;
    }
    button.disabled = !available;
    button.hidden = hideWhenUnavailable && !available;
}

async function fmFetchList(dir, offset = 0) {
    const params = new URLSearchParams({
        dir: fmNormalizeDirPath(dir),
        offset: String(Math.max(0, offset)),
        limit: String(FM_PAGE_LIMIT)
    });

    const res = await fetch(`/fm/list?${params.toString()}`, { cache: "no-store" });
    if (!res.ok) {
        throw new Error(`fm/list ${res.status}`);
    }
    return res.json();
}

function fmRenderList(payload) {
    const tbody = document.querySelector("#fmTable tbody");
    if (!tbody) {
        return;
    }

    const entries = Array.isArray(payload.entries) ? payload.entries : [];
    tbody.innerHTML = "";

    entries.forEach((entry) => {
        const tr = document.createElement("tr");
        const name = escapeHtml(entry.name || "");
        const type = entry.type === "dir" ? "Dossier" : "Fichier";
        const size = entry.type === "dir" ? "-" : formatBytes(entry.size);
        const encodedPath = encodeURIComponent(entry.path || "");

        const viewableExtensions = [".csv", ".json", ".txt", ".log"];
        const isViewable = entry.type === "file" && viewableExtensions.some(ext => entry.name.toLowerCase().endsWith(ext));

        tr.innerHTML = `
            <td>${name}</td>
            <td>${type}</td>
            <td>${size}</td>
            <td>
                <div class="fm-row-actions">
                    ${entry.type === "dir"
                        ? `<button type="button" data-action="open" data-path="${encodedPath}">Ouvrir</button>`
                        : `${isViewable ? `<button type="button" data-action="view" data-path="${encodedPath}">Voir</button>` : ""}<button type="button" data-action="download" data-path="${encodedPath}">Telecharger</button>`}
                    <button type="button" data-action="delete" data-path="${encodedPath}" data-type="${entry.type}">Supprimer</button>
                </div>
            </td>
        `;

        tbody.appendChild(tr);
    });

    if (entries.length === 0) {
        tbody.innerHTML = `<tr><td colspan="4">Dossier vide</td></tr>`;
    }
}

function fmUpdateUiState(payload) {
    fmCurrentDir = fmNormalizeDirPath(payload.dir || "/");
    fmOffset = Number(payload.offset) || 0;
    fmHasMore = !!payload.hasMore;

    const pathEl = document.getElementById("fmPath");
    if (pathEl) {
        pathEl.textContent = fmCurrentDir;
    }

    const pageInfo = document.getElementById("fmPageInfo");
    if (pageInfo) {
        const visibleFrom = Number(payload.visibleFrom) || 0;
        const visibleTo = Number(payload.visibleTo) || 0;
        const totalChildren = Number(payload.totalChildren);
        const totalLabel = Number.isFinite(totalChildren) && totalChildren >= 0
            ? `${totalChildren}`
            : `>${visibleTo}`;
        const endLabel = fmHasMore ? "" : " | Fin du dossier";
        pageInfo.textContent = `Elements ${visibleFrom}-${visibleTo} / ${totalLabel}${endLabel}`;
    }

    const prevBtn = document.getElementById("fmPrevBtn");
    const nextBtn = document.getElementById("fmNextBtn");
    const upBtn = document.getElementById("fmUpBtn");

    fmSetButtonAvailability(prevBtn, fmOffset > 0);
    fmSetButtonAvailability(nextBtn, fmHasMore);
    fmSetButtonAvailability(upBtn, fmCurrentDir !== "/");
}

async function fmLoadCurrentDir(resetOffset = false) {
    if (resetOffset) {
        fmOffset = 0;
    }

    fmSetStatus(`Chargement du dossier ${fmCurrentDir}...`, "progress");
    try {
        const payload = await fmFetchList(fmCurrentDir, fmOffset);
        fmRenderList(payload);
        fmUpdateUiState(payload);
        fmSetStatus(`Dossier charge: ${fmCurrentDir}`, "success", 1800);
    } catch (err) {
        console.error("FM load failed", err);
        fmSetStatus(`Erreur de chargement (${err?.message || "inconnue"})`, "error");
    }
}

async function fmOpenDir(path) {
    fmCurrentDir = fmNormalizeDirPath(path);
    fmOffset = 0;
    await fmLoadCurrentDir(false);
}

async function fmGoParent() {
    await fmOpenDir(fmParentDir(fmCurrentDir));
}

async function fmPrevPage() {
    fmOffset = Math.max(0, fmOffset - FM_PAGE_LIMIT);
    await fmLoadCurrentDir(false);
}

async function fmNextPage() {
    if (!fmHasMore) {
        return;
    }
    fmOffset += FM_PAGE_LIMIT;
    await fmLoadCurrentDir(false);
}

function fmDownloadFile(path) {
    window.location.href = `/fm/download?path=${encodeURIComponent(path)}`;
}

async function fmViewFile(path, targetWindow) {
    try {
        fmSetStatus(`Chargement du fichier...`, "progress");
        const res = await fetch(`/fm/download?path=${encodeURIComponent(path)}`, { cache: "no-store" });
        if (!res.ok) {
            fmSetStatus("Impossible de charger le fichier", "error");
            if (targetWindow && !targetWindow.closed) {
                targetWindow.close();
            }
            return;
        }
        const content = await res.text();
        
        // Create a modal-like view
        const viewerHtml = `
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Viewer - ${escapeHtml(path)}</title>
    <style>
        * { box-sizing: border-box; }
        body {
            margin: 0;
            padding: 16px;
            font-family: "Trebuchet MS", "Segoe UI", monospace;
            background: #0a1a20;
            color: #def4ff;
        }
        .viewer-container {
            max-width: 1200px;
            margin: 0 auto;
            border: 1px solid #2c4d59;
            border-radius: 8px;
            background: #081218;
            overflow: hidden;
            display: flex;
            flex-direction: column;
            height: 100vh;
        }
        .viewer-header {
            padding: 12px 16px;
            border-bottom: 1px solid #2c4d59;
            display: flex;
            justify-content: space-between;
            align-items: center;
            background: #10222b;
        }
        .viewer-header h1 {
            margin: 0;
            font-size: 1.1rem;
            word-break: break-all;
        }
        .viewer-header button {
            padding: 8px 12px;
            border: none;
            border-radius: 6px;
            background: #3f1d24;
            color: #ffc9cf;
            cursor: pointer;
            font-weight: 700;
        }
        .viewer-header button:hover {
            filter: brightness(1.06);
        }
        .viewer-content {
            flex: 1;
            overflow: auto;
            padding: 16px;
            white-space: pre-wrap;
            word-wrap: break-word;
            font-size: 0.9rem;
            line-height: 1.5;
            background: #0a1a20;
        }
        @media (max-width: 768px) {
            .viewer-content {
                font-size: 0.85rem;
            }
        }
    </style>
</head>
<body>
    <div class="viewer-container">
        <div class="viewer-header">
            <h1>${escapeHtml(path)}</h1>
            <button onclick="window.close()">Fermer</button>
        </div>
        <div class="viewer-content">${escapeHtml(content)}</div>
    </div>
</body>
</html>
        `;
        
        if (!targetWindow || targetWindow.closed) {
            targetWindow = window.open("about:blank", "_blank");
        }

        if (!targetWindow) {
            fmSetStatus("Popup bloquee. Autorisez les popups pour utiliser Voir.", "error");
            return;
        }

        targetWindow.document.open();
        targetWindow.document.write(viewerHtml);
        targetWindow.document.close();
        fmSetStatus("");
    } catch (err) {
        if (targetWindow && !targetWindow.closed) {
            targetWindow.close();
        }
        fmSetStatus(`Erreur lecture fichier (${err?.message || "inconnue"})`, "error");
    }
}

async function fmDeletePath(path, type) {
    const normalizedPath = String(path || "").trim();
    if (!normalizedPath) {
        fmSetStatus("Chemin de suppression vide", "error");
        return;
    }

    const label = type === "dir" ? "ce dossier" : "ce fichier";
    if (!confirm(`Supprimer ${label} ?\n${normalizedPath}`)) {
        return;
    }

    try {
        fmSetStatus(`Suppression en cours: ${normalizedPath}`, "progress");
        const res = await fetch(`/fm/delete?path=${encodeURIComponent(normalizedPath)}`, { cache: "no-store" });
        if (!res.ok) {
            const msg = await fmReadErrorText(res, "reponse serveur invalide");
            fmSetStatus(`Suppression echouee: ${msg}`, "error");
            return;
        }
        fmSetStatus(`Suppression reussie: ${normalizedPath}`, "success", 1800);
        await fmLoadCurrentDir(false);
    } catch (err) {
        fmSetStatus(`Erreur de suppression (${err?.message || "inconnue"})`, "error");
    }
}

async function fmCreateDir() {
    const input = document.getElementById("fmNewDirName");
    const rawName = input ? input.value.trim() : "";
    if (!rawName) {
        fmSetStatus("Nom de dossier manquant", "error");
        return;
    }
    if (rawName.includes("/") || rawName.includes("..")) {
        fmSetStatus("Nom de dossier invalide", "error");
        return;
    }

    const newPath = fmCurrentDir === "/" ? `/${rawName}` : `${fmCurrentDir}/${rawName}`;
    try {
        fmSetStatus(`Creation du dossier ${newPath}...`, "progress");
        const body = new URLSearchParams({ path: newPath });
        const res = await fetch("/fm/mkdir", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body
        });
        if (!res.ok) {
            const msg = await fmReadErrorText(res, "reponse serveur invalide");
            fmSetStatus(`Creation echouee: ${msg}`, "error");
            return;
        }
        if (input) {
            input.value = "";
        }
        fmSetStatus(`Dossier cree: ${newPath}`, "success", 1800);
        await fmLoadCurrentDir(false);
    } catch (err) {
        fmSetStatus(`Erreur creation dossier (${err?.message || "inconnue"})`, "error");
    }
}

async function fmUploadFile(event) {
    event.preventDefault();

    const fileInput = document.getElementById("fmUploadFile");
    if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
        fmSetStatus("Selectionne un fichier", "error");
        return;
    }

    const uploadedName = fileInput.files[0].name;
    const uploadedSize = Number(fileInput.files[0].size) || 0;
    let overwrite = false;

    try {
        const existing = await fmFetchList(fmCurrentDir, 0);
        const entries = Array.isArray(existing.entries) ? existing.entries : [];
        const existsInCurrentPage = entries.some((entry) => entry.type === "file" && entry.name === uploadedName);
        if (existsInCurrentPage) {
            overwrite = confirm(`Le fichier ${uploadedName} existe deja dans ce dossier. Voulez-vous le remplacer ?`);
            if (!overwrite) {
                fmSetStatus("Upload annule (fichier existant conserve)", "info", 1800);
                return;
            }
        }
    } catch (_) {
        // Ignore pre-check errors: backend still enforces overwrite protection.
    }

    const formData = new FormData();
    formData.append("file", fileInput.files[0]);

    fmSetStatus(`Upload en cours: ${uploadedName} (0%)`, "progress");

    const uploadWithProgress = (url, body) => new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        let lastPercentShown = -1;

        xhr.open("POST", url, true);

        xhr.upload.onprogress = (e) => {
            if (!e.lengthComputable) {
                return;
            }

            const percent = Math.max(0, Math.min(100, Math.round((e.loaded / e.total) * 100)));
            if (percent === lastPercentShown) {
                return;
            }

            lastPercentShown = percent;
            const sentLabel = formatBytes(e.loaded);
            const totalLabel = formatBytes(e.total);
            fmSetStatus(`Upload en cours: ${uploadedName} (${percent}%) - ${sentLabel}/${totalLabel}`, "progress");
        };

        xhr.onload = () => {
            resolve({ ok: xhr.status >= 200 && xhr.status < 300, status: xhr.status, text: xhr.responseText || "" });
        };

        xhr.onerror = () => reject(new Error("echec reseau"));
        xhr.ontimeout = () => reject(new Error("delai depasse"));

        xhr.send(body);
    });

    try {
        const url = `/fm/upload?dir=${encodeURIComponent(fmCurrentDir)}&overwrite=${overwrite ? "1" : "0"}`;
        const res = await uploadWithProgress(url, formData);
        if (!res.ok) {
            const msg = res.text || `reponse serveur invalide (${res.status})`;
            if (res.status === 409) {
                fmSetStatus(`Upload annule: ${uploadedName} existe deja (choisir remplacer pour ecraser)`, "error");
                return;
            }
            fmSetStatus(`Upload echoue: ${msg}`, "error");
            return;
        }

        fileInput.value = "";
        const sizeLabel = uploadedSize > 0 ? ` (${formatBytes(uploadedSize)})` : "";
        fmSetStatus(`Upload termine: ${uploadedName}${sizeLabel}`, "success", 1800);
        await fmLoadCurrentDir(false);
    } catch (err) {
        fmSetStatus(`Erreur upload (${err?.message || "inconnue"})`, "error");
    }
}

function fmHandleTableClick(event) {
    const button = event.target.closest("button[data-action]");
    if (!button) {
        return;
    }

    const action = button.dataset.action;
    const encodedPath = button.dataset.path || "";
    const path = encodedPath ? decodeURIComponent(encodedPath) : "";
    const type = button.dataset.type || "file";

    if (!path) {
        return;
    }

    if (action === "open") {
        fmOpenDir(path);
        return;
    }
    if (action === "view") {
        const viewerWindow = window.open("about:blank", "_blank");
        if (!viewerWindow) {
            fmSetStatus("Popup bloquee. Autorisez les popups pour utiliser Voir.", "error");
            return;
        }
        fmViewFile(path, viewerWindow);
        return;
    }
    if (action === "download") {
        fmDownloadFile(path);
        return;
    }
    if (action === "delete") {
        fmDeletePath(path, type);
    }
}

window.addEventListener("DOMContentLoaded", () => {
    const fmRefreshBtn = document.getElementById("fmRefreshBtn");
    const fmUpBtn = document.getElementById("fmUpBtn");
    const fmPrevBtn = document.getElementById("fmPrevBtn");
    const fmNextBtn = document.getElementById("fmNextBtn");
    const fmMkdirBtn = document.getElementById("fmMkdirBtn");
    const fmUploadForm = document.getElementById("fmUploadForm");
    const fmTable = document.getElementById("fmTable");
    const fmBackBtn = document.getElementById("fmBackBtn");

    if (fmRefreshBtn) {
        fmRefreshBtn.addEventListener("click", () => fmLoadCurrentDir(true));
    }
    if (fmUpBtn) {
        fmUpBtn.addEventListener("click", fmGoParent);
    }
    if (fmPrevBtn) {
        fmPrevBtn.addEventListener("click", fmPrevPage);
    }
    if (fmNextBtn) {
        fmNextBtn.addEventListener("click", fmNextPage);
    }
    if (fmMkdirBtn) {
        fmMkdirBtn.addEventListener("click", fmCreateDir);
    }
    if (fmUploadForm) {
        fmUploadForm.addEventListener("submit", fmUploadFile);
    }
    if (fmTable) {
        fmTable.addEventListener("click", fmHandleTableClick);
    }
    if (fmBackBtn) {
        fmBackBtn.addEventListener("click", () => {
            window.location.href = "/";
        });
    }

    fmLoadCurrentDir(true);
});
