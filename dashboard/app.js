// AegisOS dashboard - scaffold.
// Only section navigation exists. There is no API client yet and NO demo data:
// metrics must come from the C backend (see docs/API_SPECIFICATION.md).
(function () {
  "use strict";
  var sections = Array.prototype.slice.call(document.querySelectorAll("main section"));
  var links = Array.prototype.slice.call(document.querySelectorAll("#nav a"));

  function show() {
    var id = (location.hash || "#overview").slice(1);
    if (!sections.some(function (s) { return s.id === id; })) id = "overview";
    sections.forEach(function (s) { s.classList.toggle("active", s.id === id); });
    links.forEach(function (a) { a.classList.toggle("active", a.getAttribute("href") === "#" + id); });
  }

  window.addEventListener("hashchange", show);
  show();
})();
