
(function () {
  "use strict";

  var API_BASE = "http://127.0.0.1:8080";
  var REFRESH_INTERVAL = 3000;
  var MAX_OVERVIEW_PROCESSES = 8;

  var sections = Array.prototype.slice.call(
    document.querySelectorAll("main section.page")
  );
  var links = Array.prototype.slice.call(
    document.querySelectorAll("#nav a")
  );

  var title = document.getElementById("page-title");
  var backendStatus = document.getElementById("backend-status");
  var sidebarStatus = document.getElementById("sidebar-status");
  var connectionMessage = document.getElementById("connection-message");

  var cpuValue = document.getElementById("cpu-value");
  var memoryValue = document.getElementById("memory-value");
  var processCount = document.getElementById("process-count");
  var incidentCount = document.getElementById("incident-count");

  var processTable = document.getElementById("process-table");
  var overviewProcessTable =
    document.getElementById("overview-process-table");

  var overviewProcessStatus =
    document.getElementById("overview-process-status");
  var processPageStatus =
    document.getElementById("process-page-status");
  var monitorServiceStatus =
    document.getElementById("monitor-service-status");
  var httpServiceStatus =
    document.getElementById("http-service-status");

  var refreshTimer = null;

  function showSection() {
    var id = (window.location.hash || "#overview").slice(1);
    var selected = sections.find(function (section) {
      return section.id === id;
    });

    if (!selected) {
      selected = document.getElementById("overview");
      id = "overview";
    }

    sections.forEach(function (section) {
      section.classList.toggle("active", section === selected);
    });

    links.forEach(function (link) {
      var active = link.getAttribute("href") === "#" + id;
      link.classList.toggle("active", active);

      if (active) {
        link.setAttribute("aria-current", "page");
      } else {
        link.removeAttribute("aria-current");
      }
    });

    if (title && selected) {
      var heading = selected.querySelector("h2");
      if (heading) title.textContent = heading.textContent;
    }
  }

  function formatNumber(value) {
    var number = Number(value);
    return Number.isFinite(number) ? number.toFixed(1) : "0.0";
  }

  function setMetric(element, value, suffix) {
    if (!element) return;

    element.textContent = String(value);

    if (suffix) {
      var span = document.createElement("span");
      span.textContent = suffix;
      element.appendChild(span);
    }
  }

  function setServiceStatus(element, text) {
    if (element) element.textContent = text;
  }

  function setStatus(isConnected, message) {
    var status = isConnected
      ? "Backend connected"
      : "Backend disconnected";

    if (backendStatus) backendStatus.textContent = status;
    if (sidebarStatus) sidebarStatus.textContent = status;
    if (connectionMessage) connectionMessage.textContent = message;

    setServiceStatus(
      monitorServiceStatus,
      isConnected ? "API connected" : "Unavailable"
    );

    setServiceStatus(
      httpServiceStatus,
      isConnected ? "Connected" : "Disconnected"
    );
  }

  function showEmptyRow(table, message) {
    if (!table) return;

    table.replaceChildren();

    var row = document.createElement("tr");
    var cell = document.createElement("td");

    cell.colSpan = 5;
    cell.className = "table-empty";
    cell.textContent = message;

    row.appendChild(cell);
    table.appendChild(row);
  }

  function renderTable(table, processes) {
    if (!table) return;

    table.replaceChildren();

    if (processes.length === 0) {
      showEmptyRow(table, "No processes found.");
      return;
    }

    processes.forEach(function (process) {
      var row = document.createElement("tr");
      var values = [
        String(process.pid),
        String(process.name || "unknown"),
        formatNumber(process.cpu_percent) + "%",
        formatNumber(process.memory_mb) + " MB",
        String(process.state || "?")
      ];

      values.forEach(function (value) {
        var cell = document.createElement("td");
        cell.textContent = value;
        row.appendChild(cell);
      });

      table.appendChild(row);
    });
  }

  function renderProcesses(processes) {
    renderTable(processTable, processes);

    renderTable(
      overviewProcessTable,
      processes.slice(0, MAX_OVERVIEW_PROCESSES)
    );

    setServiceStatus(
      overviewProcessStatus,
      "Live · " + processes.length
    );

    setServiceStatus(
      processPageStatus,
      processes.length + " processes"
    );
  }

  function renderAnomalies(anomalies) {
    var table = document.getElementById("anomaly-table");
    var status = document.getElementById("anomaly-status");

    if (incidentCount) {
      incidentCount.textContent = String(anomalies.length);
    }

    if (status) {
      status.textContent = "Live · " + anomalies.length + " events";
    }

    if (table) {
      table.replaceChildren();

      if (anomalies.length === 0) {
        showEmptyRow(table, "No anomalies detected.");
      } else {
        anomalies.slice().reverse().forEach(function (anomaly) {
          var row = document.createElement("tr");

          var values = [
            String(anomaly.type || "unknown").replace(/_/g, " "),
            String(anomaly.pid),
            formatNumber(anomaly.observed_value),
            formatNumber(anomaly.threshold),
            String(anomaly.explanation || "Anomaly detected")
          ];

          values.forEach(function (value) {
            var cell = document.createElement("td");
            cell.textContent = value;
            row.appendChild(cell);
          });

          table.appendChild(row);
        });
      }
    }

    renderIncidents(anomalies);
    renderRecovery(anomalies);
  }

  function renderIncidents(anomalies) {
    var table = document.getElementById("incident-table");
    var status = document.getElementById("incident-status");

    if (status) {
      status.textContent = anomalies.length + " recorded";
    }

    if (!table) return;

    table.replaceChildren();

    if (anomalies.length === 0) {
      showEmptyRow(table, "No incidents recorded yet.");
      return;
    }

    anomalies.slice().reverse().forEach(function (item) {
      var row = document.createElement("tr");

      var timestamp = item.timestamp
        ? new Date(Number(item.timestamp) * 1000).toLocaleString()
        : "—";

      var values = [
        timestamp,
        String(item.pid),
        String(item.type || "unknown").replace(/_/g, " "),
        formatNumber(item.observed_value),
        formatNumber(item.threshold)
      ];

      values.forEach(function (value) {
        var cell = document.createElement("td");
        cell.textContent = value;
        row.appendChild(cell);
      });

      table.appendChild(row);
    });
  }

  function renderRecovery(anomalies) {
    var table = document.getElementById("recovery-table");
    var status = document.getElementById("recovery-status");
    var detectionStatus =
      document.getElementById("recovery-detection-status");

    if (status) status.textContent = "Monitoring connected";
    if (detectionStatus) detectionStatus.textContent = "Connected";

    if (!table) return;

    table.replaceChildren();

    if (anomalies.length === 0) {
      showEmptyRow(table, "No recent anomalies requiring review.");
      return;
    }

    anomalies.slice().reverse().forEach(function (item) {
      var row = document.createElement("tr");

      var values = [
        String(item.type || "unknown").replace(/_/g, " "),
        String(item.pid),
        formatNumber(item.observed_value),
        formatNumber(item.threshold),
        "Detected · no recovery action taken"
      ];

      values.forEach(function (value) {
        var cell = document.createElement("td");
        cell.textContent = value;
        row.appendChild(cell);
      });

      table.appendChild(row);
    });
  }

  function updateAnomalies() {
    return fetch(API_BASE + "/api/anomalies", {
      method: "GET",
      cache: "no-store"
    })
      .then(function (response) {
        if (!response.ok) {
          throw new Error("Anomaly API returned HTTP " + response.status);
        }

        return response.json();
      })
      .then(function (data) {
        if (!data || !Array.isArray(data.anomalies)) {
          throw new Error("Invalid anomaly API response.");
        }

        renderAnomalies(data.anomalies);
      })
      .catch(function (error) {
        console.error("Could not load anomalies:", error);

        [
          "anomaly-status",
          "incident-status",
          "recovery-status",
          "recovery-detection-status"
        ].forEach(function (id) {
          var element = document.getElementById(id);
          if (element) element.textContent = "Unavailable";
        });

        [
          ["anomaly-table", "Anomaly data is unavailable."],
          ["incident-table", "Incident history is unavailable."],
          ["recovery-table", "Recovery information is unavailable."]
        ].forEach(function (item) {
          var table = document.getElementById(item[0]);
          if (table) showEmptyRow(table, item[1]);
        });
      });
  }

  function updateServiceStatuses() {
    fetch(API_BASE + "/api/status", { cache: "no-store" })
      .then(function (response) {
        if (!response.ok) {
          throw new Error("Status API returned HTTP " + response.status);
        }

        return response.json();
      })
      .then(function (data) {
        var supervisor = document.getElementById("supervisor-service-status");
        var recovery = document.getElementById("recovery-engine-status");

        if (supervisor) {
          supervisor.textContent =
            data.supervisor === "cli_only"
              ? "CLI available · Not monitored"
              : "Unavailable";
        }

        if (recovery) {
          recovery.textContent =
            data.recovery === "detection_only"
              ? "Detection only"
              : "Unavailable";
        }
      })
      .catch(function (error) {
        console.error("Could not load service statuses:", error);
      });
  }


  function updateDashboard() {
    fetch(API_BASE + "/api/processes", {
      method: "GET",
      cache: "no-store"
    })
      .then(function (response) {
        if (!response.ok) {
          throw new Error("API returned HTTP " + response.status);
        }

        return response.json();
      })
      .then(function (data) {
        if (!data || !Array.isArray(data.processes)) {
          throw new Error("The API response has an invalid format.");
        }

        var processes = data.processes;
        var totalCpu = 0;
        var totalMemory = 0;

        processes.forEach(function (process) {
          totalCpu += Number(process.cpu_percent) || 0;
          totalMemory += Number(process.memory_mb) || 0;
        });

        setMetric(cpuValue, formatNumber(totalCpu), "%");
        setMetric(memoryValue, formatNumber(totalMemory), " MB");

        if (processCount) {
          processCount.textContent = String(processes.length);
        }

        renderProcesses(processes);

        setStatus(
          true,
          "Connected to the AegisOS C API. Data refreshes every 3 seconds."
        );

        updateAnomalies();
        updateServiceStatuses();
      })
      .catch(function (error) {
        setStatus(
          false,
          "Unable to fetch process data. Ensure the C API is running on port 8080. " +
            error.message
        );

        setServiceStatus(overviewProcessStatus, "Unavailable");
        setServiceStatus(processPageStatus, "Unavailable");

        showEmptyRow(processTable, "Process data is currently unavailable.");

        showEmptyRow(
          overviewProcessTable,
          "Process data is currently unavailable."
        );
      });
  }

  window.addEventListener("hashchange", showSection);

  window.addEventListener("beforeunload", function () {
    if (refreshTimer !== null) {
      window.clearInterval(refreshTimer);
    }
  });

  showSection();
  setStatus(false, "Connecting to the AegisOS C API...");

  updateDashboard();

  refreshTimer = window.setInterval(
    updateDashboard,
    REFRESH_INTERVAL
  );
})();
