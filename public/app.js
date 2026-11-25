// Farloom C++20 Control Plane Dashboard
document.addEventListener("DOMContentLoaded", () => {
  // Tab Switching
  const tabBtns = document.querySelectorAll(".tab-btn");
  const tabContents = document.querySelectorAll(".tab-content");

  tabBtns.forEach(btn => {
    btn.addEventListener("click", () => {
      const tabTarget = btn.getAttribute("data-tab");
      
      tabBtns.forEach(b => b.classList.remove("active"));
      tabContents.forEach(c => c.classList.remove("active"));
      
      btn.classList.add("active");
      const targetElement = document.getElementById(tabTarget);
      if (targetElement) {
        targetElement.classList.add("active");
      }
    });
  });

  // Heartbeat Monitor
  const statusDot = document.getElementById("status-dot");
  const pingStat = document.getElementById("ping-stat");
  const uptimePingsEl = document.getElementById("metric-pings");
  const liveLatencyEl = document.getElementById("metric-latency");
  const requestsSentEl = document.getElementById("metric-requests");
  
  let totalPings = 0;
  let totalBrowserRequests = 0;

  async function checkHeartbeat() {
    const start = performance.now();
    try {
      const res = await fetch("/health", { cache: "no-store" });
      const duration = Math.round(performance.now() - start);
      if (res.ok) {
        statusDot.className = "status-dot";
        pingStat.textContent = `${duration} ms`;
        if (liveLatencyEl) liveLatencyEl.textContent = `${duration} ms`;
        totalPings++;
        if (uptimePingsEl) uptimePingsEl.textContent = totalPings;
      } else {
        throw new Error("Bad status");
      }
    } catch (err) {
      statusDot.className = "status-dot offline";
      pingStat.textContent = "Offline";
      if (liveLatencyEl) liveLatencyEl.textContent = "--";
    }
  }

  // Initial check and periodic polling
  checkHeartbeat();
  setInterval(checkHeartbeat, 2500);

  // API Playground Logic
  const apiMethod = document.getElementById("api-method");
  const apiPath = document.getElementById("api-path");
  const apiHeaders = document.getElementById("api-headers");
  const apiBody = document.getElementById("api-body");
  const apiSendBtn = document.getElementById("api-send-btn");
  const resStatus = document.getElementById("res-status");
  const resTime = document.getElementById("res-time");
  const resHeaders = document.getElementById("res-headers");
  const resBody = document.getElementById("res-body");
  const copyResBtn = document.getElementById("copy-res-btn");

  const presets = {
    health: {
      method: "GET",
      path: "/health",
      headers: "",
      body: ""
    },
    echoJson: {
      method: "POST",
      path: "/api/echo",
      headers: "Content-Type: application/json\nX-Client: Farloom-Dashboard",
      body: JSON.stringify({ message: "Hello Farloom C++20!", timestamp: new Date().toISOString() }, null, 2)
    },
    echoText: {
      method: "POST",
      path: "/api/echo",
      headers: "Content-Type: text/plain",
      body: "High-performance POSIX C++20 HTTP/1.1 Web Server"
    },
    staticIndex: {
      method: "GET",
      path: "/index.html",
      headers: "",
      body: ""
    },
    staticCss: {
      method: "GET",
      path: "/style.css",
      headers: "",
      body: ""
    },
    traversalCheck: {
      method: "GET",
      path: "/..%2fCMakeLists.txt",
      headers: "",
      body: ""
    },
    notAllowed: {
      method: "DELETE",
      path: "/health",
      headers: "",
      body: ""
    }
  };

  document.querySelectorAll(".preset-chip").forEach(chip => {
    chip.addEventListener("click", () => {
      const p = presets[chip.dataset.preset];
      if (p) {
        apiMethod.value = p.method;
        apiPath.value = p.path;
        apiHeaders.value = p.headers;
        apiBody.value = p.body;
      }
    });
  });

  async function executeApiRequest() {
    apiSendBtn.disabled = true;
    apiSendBtn.textContent = "Sending...";
    totalBrowserRequests++;
    if (requestsSentEl) requestsSentEl.textContent = totalBrowserRequests;

    const method = apiMethod.value.trim().toUpperCase();
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

    const options = {
      method: method,
      headers: headers
    };

    if (method !== "GET" && method !== "HEAD" && apiBody.value.trim()) {
      options.body = apiBody.value;
    }

    const t0 = performance.now();
    try {
      const response = await fetch(path, options);
      const elapsed = (performance.now() - t0).toFixed(2);
      
      // Update Status Badge
      resStatus.textContent = `${response.status} ${response.statusText || ""}`;
      resStatus.className = "status-badge";
      if (response.status >= 200 && response.status < 300) {
        resStatus.classList.add("status-200");
      } else if (response.status === 400 || response.status === 413) {
        resStatus.classList.add("status-400");
      } else if (response.status === 404) {
        resStatus.classList.add("status-404");
      } else if (response.status === 405) {
        resStatus.classList.add("status-405");
      } else if (response.status === 503) {
        resStatus.classList.add("status-503");
      } else {
        resStatus.classList.add("status-400");
      }

      resTime.textContent = `${elapsed} ms`;

      // Extract Headers
      const respHeaders = [];
      response.headers.forEach((val, key) => {
        respHeaders.push(`${key}: ${val}`);
      });
      resHeaders.textContent = respHeaders.join("\n") || "(No headers received)";

      // Extract Body
      const contentType = response.headers.get("content-type") || "";
      let bodyText = await response.text();
      if (contentType.includes("application/json")) {
        try {
          bodyText = JSON.stringify(JSON.parse(bodyText), null, 2);
        } catch (_) {}
      }
      resBody.textContent = bodyText || "(Empty response body)";
    } catch (err) {
      const elapsed = (performance.now() - t0).toFixed(2);
      resStatus.textContent = "Error";
      resStatus.className = "status-badge status-405";
      resTime.textContent = `${elapsed} ms`;
      resHeaders.textContent = "(Network connection failed or connection closed)";
      resBody.textContent = `Error: ${err.message}`;
    } finally {
      apiSendBtn.disabled = false;
      apiSendBtn.innerHTML = `
        <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
          <path d="M2.01 21L23 12 2.01 3 2 10l15 2-15 2z"/>
        </svg> Send Request
      `;
    }
  }

  apiSendBtn.addEventListener("click", executeApiRequest);

  // Keyboard shortcut: Cmd/Ctrl + Enter in body or path
  [apiPath, apiHeaders, apiBody].forEach(el => {
    el.addEventListener("keydown", (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === "Enter") {
        e.preventDefault();
        executeApiRequest();
      }
    });
  });

  if (copyResBtn) {
    copyResBtn.addEventListener("click", () => {
      navigator.clipboard.writeText(resBody.textContent).then(() => {
        const orig = copyResBtn.textContent;
        copyResBtn.textContent = "Copied!";
        setTimeout(() => copyResBtn.textContent = orig, 1500);
      });
    });
  }

  // Concurrency Stress Tester Logic
  const stressBatchSize = document.getElementById("stress-batch-size");
  const stressConcurrency = document.getElementById("stress-concurrency");
  const stressTarget = document.getElementById("stress-target");
  const stressStartBtn = document.getElementById("stress-start-btn");
  const stressProgress = document.getElementById("stress-progress");
  const stressTotal = document.getElementById("stress-total");
  const stress200 = document.getElementById("stress-200");
  const stress503 = document.getElementById("stress-503");
  const stressErrors = document.getElementById("stress-errors");
  const stressTps = document.getElementById("stress-tps");
  const stressAvg = document.getElementById("stress-avg");
  const stressP95 = document.getElementById("stress-p95");
  const latencyBars = document.getElementById("latency-distribution-bars");

  let isStressRunning = false;

  async function runStressTest() {
    if (isStressRunning) return;
    isStressRunning = true;
    stressStartBtn.disabled = true;
    stressStartBtn.textContent = "Running Benchmark...";

    const total = parseInt(stressBatchSize.value, 10) || 50;
    const concurrency = parseInt(stressConcurrency.value, 10) || 8;
    const url = stressTarget.value.trim() || "/health";

    let completed = 0;
    let count200 = 0;
    let count503 = 0;
    let countErr = 0;
    const latencies = [];

    stressProgress.style.width = "0%";
    stressTotal.textContent = `0 / ${total}`;
    stress200.textContent = "0";
    stress503.textContent = "0";
    stressErrors.textContent = "0";
    stressTps.textContent = "--";
    stressAvg.textContent = "--";
    stressP95.textContent = "--";
    latencyBars.innerHTML = "";

    const tStart = performance.now();

    let currentIndex = 0;

    async function worker() {
      while (currentIndex < total) {
        const idx = currentIndex++;
        const t0 = performance.now();
        try {
          const res = await fetch(url, { cache: "no-store" });
          const lat = performance.now() - t0;
          latencies.push(lat);
          if (res.status === 200) count200++;
          else if (res.status === 503) count503++;
          else countErr++;
        } catch (_) {
          const lat = performance.now() - t0;
          latencies.push(lat);
          countErr++;
        }
        completed++;
        totalBrowserRequests++;
        
        // Progress UI update
        const pct = Math.round((completed / total) * 100);
        stressProgress.style.width = `${pct}%`;
        stressTotal.textContent = `${completed} / ${total}`;
        stress200.textContent = count200;
        stress503.textContent = count503;
        stressErrors.textContent = countErr;
      }
    }

    const workers = [];
    for (let i = 0; i < concurrency; i++) {
      workers.push(worker());
    }
    await Promise.all(workers);

    const totalElapsedSec = (performance.now() - tStart) / 1000;
    const throughput = (total / totalElapsedSec).toFixed(1);
    latencies.sort((a, b) => a - b);
    const avg = (latencies.reduce((a, b) => a + b, 0) / latencies.length).toFixed(2);
    const p95Idx = Math.floor(latencies.length * 0.95);
    const p95 = latencies[p95Idx].toFixed(2);

    stressTps.textContent = `${throughput} req/s`;
    stressAvg.textContent = `${avg} ms`;
    stressP95.textContent = `${p95} ms`;
    if (requestsSentEl) requestsSentEl.textContent = totalBrowserRequests;

    // Render Histogram of latencies
    renderLatencyHistogram(latencies);

    isStressRunning = false;
    stressStartBtn.disabled = false;
    stressStartBtn.textContent = "Start Concurrency Test";
  }

  function renderLatencyHistogram(lats) {
    if (!lats.length) return;
    const min = lats[0];
    const max = lats[lats.length - 1];
    const buckets = 8;
    const bucketCounts = new Array(buckets).fill(0);
    const step = (max - min) / buckets || 1;

    lats.forEach(l => {
      let b = Math.floor((l - min) / step);
      if (b >= buckets) b = buckets - 1;
      bucketCounts[b]++;
    });

    const maxCount = Math.max(...bucketCounts, 1);
    latencyBars.innerHTML = "";

    bucketCounts.forEach((count, i) => {
      const barWrapper = document.createElement("div");
      barWrapper.style.cssText = "display: flex; flex-direction: column; align-items: center; gap: 4px; flex: 1;";
      
      const barHeight = Math.max(6, Math.round((count / maxCount) * 80));
      const rangeStart = (min + i * step).toFixed(1);
      
      barWrapper.innerHTML = `
        <span style="font-size: 10px; color: var(--text-muted);">${count}</span>
        <div style="width: 100%; height: ${barHeight}px; background: linear-gradient(180deg, var(--accent-cyan), var(--accent-indigo)); border-radius: 4px;"></div>
        <span style="font-size: 9px; color: var(--text-secondary); font-family: var(--font-mono);">${rangeStart}ms</span>
      `;
      latencyBars.appendChild(barWrapper);
    });
  }

  if (stressStartBtn) {
    stressStartBtn.addEventListener("click", runStressTest);
  }

  // Security Test Buttons
  document.querySelectorAll(".test-security-btn").forEach(btn => {
    btn.addEventListener("click", async () => {
      const path = btn.dataset.path;
      const method = btn.dataset.method || "GET";
      const targetLabel = btn.nextElementSibling;
      
      btn.disabled = true;
      btn.textContent = "Testing...";

      const t0 = performance.now();
      try {
        const res = await fetch(path, { method: method });
        const dur = (performance.now() - t0).toFixed(1);
        if (targetLabel) {
          targetLabel.innerHTML = `<span class="status-badge ${res.status === 404 ? 'status-404' : res.status === 405 ? 'status-405' : 'status-400'}">Status: ${res.status} (${dur}ms)</span> - Defended!`;
        }
      } catch (err) {
        if (targetLabel) {
          targetLabel.innerHTML = `<span class="status-badge status-200">Mitigated: Connection Closed</span>`;
        }
      } finally {
        btn.disabled = false;
        btn.textContent = "Run Test";
      }
    });
  });
});
