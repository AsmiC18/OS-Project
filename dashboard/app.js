
(function () {
  "use strict";

  var sections = Array.prototype.slice.call(
    document.querySelectorAll("main section.page")
  );
  var links = Array.prototype.slice.call(
    document.querySelectorAll("#nav a")
  );
  var title = document.getElementById("page-title");

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

    if (title) {
      title.textContent =
        selected.querySelector("h2").textContent;
    }
  }

  window.addEventListener("hashchange", showSection);
  showSection();

  // The backend endpoints are not implemented yet.
  // Keep the dashboard honest: do not display fabricated metrics.
  var backendStatus = document.getElementById("backend-status");
  var sidebarStatus = document.getElementById("sidebar-status");
  var connectionMessage = document.getElementById("connection-message");

  function setDisconnected() {
    if (backendStatus) {
      backendStatus.textContent = "Backend not connected";
    }

    if (sidebarStatus) {
      sidebarStatus.textContent = "Backend disconnected";
    }

    if (connectionMessage) {
      connectionMessage.textContent =
        "Live metrics will appear after the C API is connected.";
    }
  }

  setDisconnected();
})();
