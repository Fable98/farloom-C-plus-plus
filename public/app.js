// Farloom C++20 Server Manager Controller
document.addEventListener("DOMContentLoaded", () => {
  // Navigation Tabs
  const tabBtns = document.querySelectorAll(".pixel-tab-btn");
  const tabPanels = document.querySelectorAll(".tab-panel");

  function switchTab(targetId) {
    tabBtns.forEach(btn => {
      btn.classList.toggle("active", btn.dataset.tab === targetId);
    });
    tabPanels.forEach(panel => {
      panel.classList.toggle("active", panel.id === targetId);
    });
  }

  tabBtns.forEach(btn => {
    btn.addEventListener("click", () => switchTab(btn.dataset.tab));
  });

  document.querySelectorAll(".nav-link").forEach(link => {
    link.addEventListener("click", () => switchTab(link.dataset.tab));
  });

  // Telemetry Heartbeat & Auto-Poll Toggle
  const pingStat = document.getElementById("ping-stat");
  const serverStateBadge = document.getElementById("server-state-badge");
  const metricPingVal = document.getElementById("metric-ping-val");
  const metricPingsCount = document.getElementById("metric-pings-count");
  const metricReqsCount = document.getElementById("metric-reqs-count");
  const telemetryToggle = document.getElementById("telemetry-toggle");
  const telemetryToggleLabel = document.getElementById("telemetry-toggle-label");

  let isAutoPoll = true;
  let totalPings = 0;
  let totalRequests = 0;
  let pollInterval = null;

  async function pingServer() {
    const t0 = performance.now();
    try {
      const res = await fetch("/health", { cache: "no-store" });
      const elapsed = Math.round(performance.now() - t0);
      if (res.ok) {
        pingStat.textContent = `${elapsed} ms`;
        if (metricPingVal) metricPingVal.textContent = `${elapsed} ms`;
        serverStateBadge.textContent = "ONLINE";
        serverStateBadge.className = "pixel-badge pixel-badge-green";
        totalPings++;
        if (metricPingsCount) metricPingsCount.textContent = totalPings;
      } else {
        throw new Error("HTTP error " + res.status);
      }
    } catch (_) {
      pingStat.textContent = "OFFLINE";
      if (metricPingVal) metricPingVal.textContent = "OFFLINE";
      serverStateBadge.textContent = "DISCONNECTED";
      serverStateBadge.className = "pixel-badge pixel-badge-red";
    }
  }

  function startAutoPoll() {
    pingServer();
    pollInterval = setInterval(pingServer, 2000);
  }

  function stopAutoPoll() {
    if (pollInterval) clearInterval(pollInterval);
    pollInterval = null;
  }

  startAutoPoll();

  if (telemetryToggle) {
    telemetryToggle.addEventListener("click", () => {
      isAutoPoll = !isAutoPoll;
      telemetryToggle.classList.toggle("off", !isAutoPoll);
      telemetryToggleLabel.textContent = isAutoPoll ? "AUTO-POLL: ON" : "AUTO-POLL: OFF";
      if (isAutoPoll) {
        startAutoPoll();
      } else {
        stopAutoPoll();
      }
    });
  }

  // System ping button
  const systemOkBtn = document.getElementById("system-ok-btn");
  if (systemOkBtn) {
    systemOkBtn.addEventListener("click", () => {
      pingServer();
      systemOkBtn.textContent = "Pinging";
      setTimeout(() => systemOkBtn.textContent = "Ping", 800);
    });
  }

  // Diagnostic Notice Dialog Buttons
  const noticeCheckBtn = document.getElementById("notice-check-btn");
  const noticeConsoleBtn = document.getElementById("notice-console-btn");
  if (noticeCheckBtn) {
    noticeCheckBtn.addEventListener("click", () => {
      pingServer();
      noticeCheckBtn.textContent = "OK!";
      setTimeout(() => noticeCheckBtn.textContent = "Check", 1000);
    });
  }
  if (noticeConsoleBtn) {
    noticeConsoleBtn.addEventListener("click", () => {
      switchTab("tab-api");
    });
  }

  // File Explorer Click to Inspect
  document.querySelectorAll(".file-chip").forEach(chip => {
    chip.addEventListener("click", () => {
      const path = chip.dataset.file;
      switchTab("tab-api");
      document.getElementById("api-method").value = "GET";
      document.getElementById("api-path").value = path;
      document.getElementById("api-headers").value = "";
      document.getElementById("api-body").value = "";
      executeApi();
    });
  });

  // API Console
  const apiMethod = document.getElementById("api-method");
  const apiPath = document.getElementById("api-path");
  const apiHeaders = document.getElementById("api-headers");
  const apiBody = document.getElementById("api-body");
  const apiSendBtn = document.getElementById("api-send-btn");
  const apiClearBtn = document.getElementById("api-clear-btn");
  const apiStatusBadge = document.getElementById("api-status-badge");
  const apiTimeStat = document.getElementById("api-time-stat");
  const apiTerminal = document.getElementById("api-terminal");
  const copyTerminalBtn = document.getElementById("copy-terminal-btn");

  const presets = {
    health: { method: "GET", path: "/health", headers: "", body: "" },
    echoJson: { 
      method: "POST", 
      path: "/api/echo", 
      headers: "Content-Type: application/json\nX-Client: Farloom-Client", 
      body: JSON.stringify({ message: "Hello Farloom C++20", timestamp: Date.now() }, null, 2) 
    },
    echoText: { 
      method: "POST", 
      path: "/api/echo", 
      headers: "Content-Type: text/plain", 
      body: "Farloom Multithreaded C++20 HTTP/1.1 Server" 
    },
    static: { method: "GET", path: "/index.html", headers: "", body: "" },
    traversal: { method: "GET", path: "/..%2fCMakeLists.txt", headers: "", body: "" },
    method405: { method: "DELETE", path: "/health", headers: "", body: "" }
  };

  function applyPreset(presetKey) {
    const p = presets[presetKey];
    if (p) {
      apiMethod.value = p.method;
      apiPath.value = p.path;
      apiHeaders.value = p.headers;
      apiBody.value = p.body;
    }
  }

  document.querySelectorAll(".pixel-chip").forEach(chip => {
    chip.addEventListener("click", () => applyPreset(chip.dataset.preset));
  });

  // Quick Action Buttons on Overview Panel
  document.querySelectorAll(".quick-action-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      applyPreset(btn.dataset.preset);
      switchTab("tab-api");
      executeApi();
    });
  });

  async function executeApi() {
    apiSendBtn.disabled = true;
    apiSendBtn.textContent = "Sending...";
    totalRequests++;
    if (metricReqsCount) metricReqsCount.textContent = totalRequests;

    const method = apiMethod.value;
    const path = apiPath.value.trim() || "/";
    const headerLines = apiHeaders.value.split("\n");
    const headers = {};

    headerLines.forEach(line => {
      const idx = line.indexOf(":");
      if (idx > 0) {
        const k = line.substring(0, idx).trim();
        const v = line.substring(idx + 1).trim();
        if (k) headers[k] = v;
      }
    });

    const options = { method, headers };
    if (method !== "GET" && method !== "HEAD" && apiBody.value.trim()) {
      options.body = apiBody.value;
    }

    const t0 = performance.now();
    try {
      const res = await fetch(path, options);
      const elapsed = Math.round(performance.now() - t0);
      apiTimeStat.textContent = `${elapsed} ms`;

      // Status badge styling
      apiStatusBadge.textContent = `Status: ${res.status} ${res.statusText || ""}`;
      if (res.status === 200) {
        apiStatusBadge.className = "pixel-badge pixel-badge-green";
      } else if (res.status === 503) {
        apiStatusBadge.className = "pixel-badge pixel-badge-red";
      } else if (res.status === 404 || res.status === 405) {
        apiStatusBadge.className = "pixel-badge pixel-badge-yellow";
      } else {
        apiStatusBadge.className = "pixel-badge pixel-badge-blue";
      }

      // Headers string
      const respHeaders = [];
      res.headers.forEach((v, k) => respHeaders.push(`${k}: ${v}`));
      
      let bodyText = await res.text();
      try {
        const parsed = JSON.parse(bodyText);
        bodyText = JSON.stringify(parsed, null, 2);
      } catch (_) {}

      apiTerminal.textContent = 
`HTTP/1.1 ${res.status} ${res.statusText || 'OK'}
${respHeaders.join('\n')}

${bodyText}`;

    } catch (err) {
      const elapsed = Math.round(performance.now() - t0);
      apiTimeStat.textContent = `${elapsed} ms`;
      apiStatusBadge.textContent = "Status: Connection Error";
      apiStatusBadge.className = "pixel-badge pixel-badge-red";
      apiTerminal.textContent = `Error connecting to Farloom server: ${err.message}`;
    } finally {
      apiSendBtn.disabled = false;
      apiSendBtn.textContent = "Send Request (Ctrl+Enter)";
    }
  }

  apiSendBtn.addEventListener("click", executeApi);

  // Keyboard shortcut Ctrl/Cmd + Enter
  [apiPath, apiHeaders, apiBody].forEach(el => {
    el.addEventListener("keydown", e => {
      if ((e.ctrlKey || e.metaKey) && e.key === "Enter") {
        e.preventDefault();
        executeApi();
      }
    });
  });

  if (apiClearBtn) {
    apiClearBtn.addEventListener("click", () => {
      apiMethod.value = "GET";
      apiPath.value = "/health";
      apiHeaders.value = "";
      apiBody.value = "";
      apiTerminal.textContent = "Cleared.";
      apiStatusBadge.textContent = "Status: Ready";
      apiStatusBadge.className = "pixel-badge pixel-badge-blue";
    });
  }

  if (copyTerminalBtn) {
    copyTerminalBtn.addEventListener("click", () => {
      navigator.clipboard.writeText(apiTerminal.textContent).then(() => {
        copyTerminalBtn.textContent = "Copied!";
        setTimeout(() => copyTerminalBtn.textContent = "Copy Output", 1200);
      });
    });
  }

  // Segmented Progress Bar & Stress Tester
  const stressReqSelect = document.getElementById("stress-requests-select");
  const stressConcSelect = document.getElementById("stress-concurrency-select");
  const stressStartBtn = document.getElementById("stress-start-btn");
  const stressProgressBar = document.getElementById("pixel-progress-bar");
  const stressProgressCount = document.getElementById("stress-progress-count");
  const stressPct = document.getElementById("stress-pct");
  const stress200 = document.getElementById("stress-200");
  const stress503 = document.getElementById("stress-503");
  const stressThroughput = document.getElementById("stress-throughput");
  const stressAvgLat = document.getElementById("stress-avg-lat");

  let isStressActive = false;

  function updateSegmentedBar(percent) {
    stressPct.textContent = `${percent}%`;
    const totalSlots = Math.floor(stressProgressBar.clientWidth / 12) || 25;
    const activeSegments = Math.round((percent / 100) * totalSlots);
    
    stressProgressBar.innerHTML = "";
    for (let i = 0; i < activeSegments; i++) {
      const seg = document.createElement("div");
      seg.className = "pixel-progress-segment";
      stressProgressBar.appendChild(seg);
    }
  }

  async function runStress() {
    if (isStressActive) return;
    isStressActive = true;
    stressStartBtn.disabled = true;
    stressStartBtn.textContent = "Benchmarking...";

    const total = parseInt(stressReqSelect.value, 10);
    const concurrency = parseInt(stressConcSelect.value, 10);

    let completed = 0;
    let ok200 = 0;
    let shed503 = 0;
    const latencies = [];

    updateSegmentedBar(0);
    stressProgressCount.textContent = `0 / ${total}`;
    stress200.textContent = "0";
    stress503.textContent = "0";
    stressThroughput.textContent = "--";
    stressAvgLat.textContent = "--";

    const tStart = performance.now();
    let index = 0;

    async function stressWorker() {
      while (index < total) {
        index++;
        const t0 = performance.now();
        try {
          const res = await fetch("/health", { cache: "no-store" });
          const lat = performance.now() - t0;
          latencies.push(lat);
          if (res.status === 200) ok200++;
          else if (res.status === 503) shed503++;
        } catch (_) {
          latencies.push(performance.now() - t0);
        }
        completed++;
        totalRequests++;

        const pct = Math.round((completed / total) * 100);
        updateSegmentedBar(pct);
        stressProgressCount.textContent = `${completed} / ${total}`;
        stress200.textContent = ok200;
        stress503.textContent = shed503;
      }
    }

    const workers = [];
    for (let i = 0; i < concurrency; i++) {
      workers.push(stressWorker());
    }
    await Promise.all(workers);

    const elapsedSec = (performance.now() - tStart) / 1000;
    const tps = (total / elapsedSec).toFixed(1);
    const avg = (latencies.reduce((a, b) => a + b, 0) / (latencies.length || 1)).toFixed(1);

    stressThroughput.textContent = `${tps} req/s`;
    stressAvgLat.textContent = `${avg} ms`;
    if (metricReqsCount) metricReqsCount.textContent = totalRequests;

    isStressActive = false;
    stressStartBtn.disabled = false;
    stressStartBtn.textContent = "Start Benchmark";
  }

  if (stressStartBtn) {
    stressStartBtn.addEventListener("click", runStress);
  }

  // Security Lab Attack Buttons
  document.querySelectorAll(".test-attack-btn").forEach(btn => {
    btn.addEventListener("click", async () => {
      const path = btn.dataset.path;
      const method = btn.dataset.method || "GET";
      const resultBadge = btn.nextElementSibling;

      btn.disabled = true;
      btn.textContent = "Testing...";

      const t0 = performance.now();
      try {
        const res = await fetch(path, { method: method });
        const dur = Math.round(performance.now() - t0);
        if (res.status === 404) {
          resultBadge.textContent = `404 Defended (${dur}ms)`;
          resultBadge.className = "attack-result pixel-badge pixel-badge-green";
        } else if (res.status === 405) {
          resultBadge.textContent = `405 Blocked (${dur}ms)`;
          resultBadge.className = "attack-result pixel-badge pixel-badge-blue";
        } else {
          resultBadge.textContent = `Status: ${res.status}`;
          resultBadge.className = "attack-result pixel-badge pixel-badge-yellow";
        }
      } catch (err) {
        resultBadge.textContent = "Mitigated (Closed)";
        resultBadge.className = "attack-result pixel-badge pixel-badge-green";
      } finally {
        btn.disabled = false;
        btn.textContent = "Test";
      }
    });
  });

  // Menu bar quick actions
  const menuNewReq = document.getElementById("menu-new-req");
  if (menuNewReq) {
    menuNewReq.addEventListener("click", () => {
      switchTab("tab-api");
      apiPath.focus();
    });
  }

  const menuCopyRes = document.getElementById("menu-copy-res");
  if (menuCopyRes && copyTerminalBtn) {
    menuCopyRes.addEventListener("click", () => copyTerminalBtn.click());
  }

  const menuClearAll = document.getElementById("menu-clear-all");
  if (menuClearAll && apiClearBtn) {
    menuClearAll.addEventListener("click", () => apiClearBtn.click());
  }

  const menuAbout = document.getElementById("menu-about");
  if (menuAbout) {
    menuAbout.addEventListener("click", () => {
      alert("FARLOOM C++20\nMultithreaded HTTP/1.1 Web Server\nPOSIX sockets · Fixed 4-worker thread pool · 64-capacity bounded queue.");
    });
  }

  const menuDocs = document.getElementById("menu-docs");
  if (menuDocs) {
    menuDocs.addEventListener("click", () => {
      window.open("https://github.com/Fable98/farloom-C-plus-plus#readme", "_blank");
    });
  }
});
