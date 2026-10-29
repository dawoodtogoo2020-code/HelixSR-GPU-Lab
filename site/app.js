"use strict";
// Original local-only table filter. No analytics, API requests or console access.
const search = document.getElementById("game-search");
const rows = Array.from(document.querySelectorAll("#game-rows tr"));
const result = document.getElementById("search-result");
search.addEventListener("input", () => {
  const query = search.value.trim().toLowerCase();
  let count = 0;
  for (const row of rows) {
    row.hidden = !row.dataset.search.includes(query);
    if (!row.hidden) count += 1;
  }
  result.textContent = count
    ? `${count} ${count === 1 ? "entry" : "entries"}. Detection does not establish safe activation.`
    : "No matching entry. Unlisted games are unsupported; this search does not inspect your console.";
});
