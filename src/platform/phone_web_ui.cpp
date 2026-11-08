#include "pkgmaster/platform/phone_web_ui.hpp"
#include "pkgmaster/platform/phone_lan_server.hpp"

namespace pkgmaster {
const std::string& phone_web_ui_html() {
    static const std::string page = R"PKGHTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#10141d">
<title>PKGMaster Phone Link</title>
<style>
:root{color-scheme:dark;font-family:system-ui,-apple-system,Segoe UI,sans-serif;background:#10141d;color:#eef2f8}
*{box-sizing:border-box}body{margin:0;background:linear-gradient(145deg,#10141d,#172235);min-height:100vh}
main{width:min(920px,100%);margin:auto;padding:24px 16px 48px}
header{display:flex;align-items:center;gap:12px;margin:8px 0 24px}
.logo{width:42px;height:42px;border-radius:12px;background:#2763d7;display:grid;place-items:center;font-weight:800}
h1{font-size:22px;margin:0} .muted{color:#9ba9bd;font-size:13px}
.panel{background:#1a2433;border:1px solid #2b3b51;border-radius:16px;padding:18px;margin:14px 0}
h2{font-size:16px;margin:0 0 14px}
form{display:flex;gap:8px;flex-wrap:wrap}
input{flex:1;min-width:220px;background:#0e1724;color:#fff;border:1px solid #3a4c64;border-radius:10px;padding:12px;font:inherit}
button{border:0;border-radius:10px;background:#2867df;color:white;padding:10px 14px;font:600 14px inherit;cursor:pointer}
button.secondary{background:#34445a}button.danger{background:#7d3540}button:disabled{opacity:.45;cursor:not-allowed}
#message{min-height:22px;color:#9ec2ff;font-size:13px;margin:10px 0}
.item{display:flex;justify-content:space-between;gap:14px;align-items:center;padding:12px 0;border-top:1px solid #2b3b51}
.item:first-child{border-top:0}.item-title{font-weight:650;overflow-wrap:anywhere}.meta{font-size:12px;color:#9ba9bd;margin-top:4px}
.actions{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}.actions button{padding:8px 10px;font-size:12px}
.empty{color:#9ba9bd;font-size:13px;padding:12px 0}
.badge{display:inline-block;background:#263b5b;border-radius:99px;padding:4px 8px;font-size:11px}
footer{color:#8493a8;font-size:12px;margin-top:20px;line-height:1.5}
@media(max-width:560px){.item{align-items:flex-start;flex-direction:column}.actions{justify-content:flex-start}main{padding-top:12px}}
</style>
<script src="/app.js" defer></script>
</head>
<body><main>
<header><div class="logo">PKG</div><div><h1>PKGMaster</h1><div class="muted">Local console companion</div></div><span class="badge" id="connection">Not paired</span></header>
<div id="message" role="status" aria-live="polite">Pair this phone using the one-time link displayed by your console.</div>
<section class="panel"><h2>Add a package download</h2><form id="download-form"><input id="package-url" type="url" placeholder="https://host.example/game.pkg" autocomplete="url" required><button type="submit">Add download</button></form><div class="muted" style="margin-top:9px">Only .pkg downloads are accepted. Files are saved in the console's configured download folder.</div></section>
<section class="panel"><h2>Downloads</h2><div id="downloads" class="empty">Waiting for connection…</div></section>
<section class="panel"><h2>Storage</h2><div id="storage" class="empty">Waiting for connection…</div></section>
<section class="panel"><h2>Package library</h2><div id="library" class="empty">Waiting for connection…</div></section>
<footer>Connected over your local network. Pairing and API traffic use HTTP in this build; use only on a trusted, isolated LAN. Never share your pairing link or session.</footer>
</main></body></html>)PKGHTML";
    return page;
}
const std::string& phone_web_ui_javascript() {
    static const std::string script = R"PKGJS("use strict";
(() => {
  const $ = id => document.getElementById(id);
  const message = text => { $("message").textContent = text; };
  let token = sessionStorage.getItem("pkgmaster_session") || "";
  const api = async (path, options = {}) => {
    const headers = Object.assign({}, options.headers || {}, token ? {Authorization: "Bearer " + token} : {});
    const response = await fetch(path, Object.assign({}, options, {headers, cache: "no-store"}));
    let data = {};
    try { data = await response.json(); } catch (_) {}
    if (!response.ok) {
      if (response.status === 401) {
        token = "";
        sessionStorage.removeItem("pkgmaster_session");
        $("connection").textContent = "Not paired";
      }
      throw new Error(data.error || ("HTTP " + response.status));
    }
    return data;
  };
  const button = (label, action, style = "secondary") => {
    const node = document.createElement("button");
    node.type = "button"; node.className = style; node.textContent = label;
    node.addEventListener("click", action); return node;
  };
  const refreshDownloads = async () => {
    const data = await api("/api/v1/downloads");
    const root = $("downloads"); root.replaceChildren();
    if (!data.downloads || !data.downloads.length) {
      root.className = "empty"; root.textContent = "No downloads yet."; return;
    }
    root.className = "";
    for (const item of data.downloads) {
      const row = document.createElement("div"); row.className = "item";
      const info = document.createElement("div");
      const title = document.createElement("div"); title.className = "item-title";
      title.textContent = item.file_name || ("Download #" + item.id);
      const meta = document.createElement("div"); meta.className = "meta";
      const percent = item.expected_bytes ? Math.min(100, item.received_bytes / item.expected_bytes * 100).toFixed(1) + "%" : "—";
      meta.textContent = item.state + " · " + item.received_bytes + " / " + (item.expected_bytes || "unknown") + " bytes · " + percent;
      info.append(title, meta);
      if (item.error) { const err = document.createElement("div"); err.className = "meta"; err.textContent = item.error; info.append(err); }
      const actions = document.createElement("div"); actions.className = "actions";
      const run = async (path, method = "POST") => { try { await api(path, {method}); await refreshDownloads(); message("Download updated."); } catch (e) { message(e.message); } };
      if (item.state === "queued" || item.state === "resolving" || item.state === "downloading")
        actions.append(button("Pause", () => run("/api/v1/downloads/" + item.id + "/pause")));
      if (item.state === "paused")
        actions.append(button("Resume", () => run("/api/v1/downloads/" + item.id + "/resume")));
      if (item.state === "failed")
        actions.append(button("Retry", () => run("/api/v1/downloads/" + item.id + "/retry")));
      if (!["completed", "cancelled"].includes(item.state))
        actions.append(button("Cancel", () => run("/api/v1/downloads/" + item.id, "DELETE"), "danger"));
      row.append(info, actions); root.append(row);
    }
  };
  const refreshLibrary = async () => {
    const data = await api("/api/v1/library");
    const root = $("library"); root.replaceChildren();
    if (!data.packages || !data.packages.length) {
      root.className = "empty"; root.textContent = "No indexed packages found."; return;
    }
    root.className = "";
    for (const item of data.packages) {
      const row = document.createElement("div"); row.className = "item";
      const title = document.createElement("div");
      const name = document.createElement("div"); name.className = "item-title"; name.textContent = item.title || "Untitled package";
      const meta = document.createElement("div"); meta.className = "meta";
      meta.textContent = (item.title_id || "Unknown title ID") + " · v" + (item.app_version || "—") + " · " + (item.category || "—");
      title.append(name, meta); row.append(title); root.append(row);
    }
  };
  const formatBytes = value => {
    if (!value) return "0 B";
    const units = ["B", "KB", "MB", "GB", "TB"];
    let amount = value, index = 0;
    while (amount >= 1024 && index < units.length - 1) { amount /= 1024; index++; }
    return amount.toFixed(index ? 1 : 0) + " " + units[index];
  };
  const refreshStorage = async () => {
    const data = await api("/api/v1/storage");
    const root = $("storage"); root.replaceChildren();
    if (!data.volumes || !data.volumes.length) {
      root.className = "empty"; root.textContent = "No configured storage volumes are currently available."; return;
    }
    root.className = "";
    for (const volume of data.volumes) {
      const row = document.createElement("div"); row.className = "item";
      const info = document.createElement("div");
      const title = document.createElement("div"); title.className = "item-title";
      title.textContent = volume.label || volume.id;
      const meta = document.createElement("div"); meta.className = "meta";
      meta.textContent = volume.kind + " · " + formatBytes(volume.available_bytes) + " free of " + formatBytes(volume.total_bytes) + " · " + (volume.writable ? "writable" : "read-only");
      info.append(title, meta); row.append(info); root.append(row);
    }
  };
  const refresh = async () => {
    if (!token) return;
    try {
      await api("/api/v1/status");
      $("connection").textContent = "Connected";
      await Promise.all([refreshDownloads(), refreshLibrary(), refreshStorage()]);
    } catch (e) { message(e.message); }
  };
  const pair = async () => {
    const secret = location.hash.slice(1);
    if (!secret) return;
    history.replaceState(null, "", location.pathname);
    if (!/^[0-9a-f]{64}$/i.test(secret)) { message("The pairing link is invalid."); return; }
    try {
      message("Pairing with your console…");
      const response = await fetch("/api/v1/pair", {method:"POST", headers:{"Content-Type":"text/plain"}, body:secret, cache:"no-store"});
      const data = await response.json();
      if (!response.ok || !data.session_token) throw new Error(data.error || "Pairing failed");
      token = data.session_token;
      sessionStorage.setItem("pkgmaster_session", token);
      message("Phone paired successfully.");
      await refresh();
    } catch (e) { message(e.message); }
  };
  $("download-form").addEventListener("submit", async event => {
    event.preventDefault();
    const url = $("package-url").value.trim();
    try {
      const result = await api("/api/v1/downloads", {method:"POST", headers:{"Content-Type":"text/plain"}, body:url});
      $("package-url").value = "";
      message("Download #" + result.id + " queued.");
      await refreshDownloads();
    } catch (e) { message(e.message); }
  });
  if (token) refresh();
  else pair();
  window.setInterval(() => { if (token) refreshDownloads().catch(e => message(e.message)); }, 4000);
})();
)PKGJS";
    return script;
}
bool make_phone_pairing_url(const std::string& address, std::uint16_t port,
                            const std::string& secret, std::string& output,
                            std::string& error) {
    output.clear(); error.clear();
    if (!PhoneLanServer::is_private_lan_ipv4(address) || port == 0 || secret.size() != 64) {
        error = "Pairing URL requires a private LAN IPv4 address, nonzero port and 256-bit token";
        return false;
    }
    for (char c : secret) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            error = "Pairing token must be hexadecimal";
            return false;
        }
    }
    output = "http://" + address + ":" + std::to_string(port) + "/pair#" + secret;
    return true;
}
}
