/* The visual Morse tree: T side to the left (dah), E side to the right (dit).
 * Rectangles are letters ending in a dah, circles letters ending in a dit. */
"use strict";

const TREE = {
  // letter: [x, y, labelX, labelY]
  nodes: {
    T: [362, 257, 362, 212], M: [275, 257, 275, 212], O: [187, 257, 187, 212],
    E: [537, 257, 537, 210], I: [625, 257, 625, 210], S: [712, 257, 712, 210], H: [800, 257, 800, 210],
    G: [275, 349, 275, 302], Q: [187, 349, 187, 304], Z: [275, 441, 275, 394],
    U: [625, 349, 625, 304], V: [712, 349, 712, 304], F: [625, 441, 625, 394],
    N: [362, 533, 362, 486], K: [275, 533, 275, 488], Y: [187, 533, 187, 488], C: [275, 625, 275, 578],
    A: [537, 533, 537, 488], R: [625, 533, 625, 486], L: [712, 533, 712, 486],
    D: [362, 716, 362, 669], X: [275, 716, 275, 671], B: [362, 808, 362, 761],
    W: [537, 716, 537, 671], P: [625, 716, 625, 669], J: [537, 808, 537, 763],
  },
  root: [450, 257],
};

function buildTree(host, opts = {}) {
  const NS = "http://www.w3.org/2000/svg";
  const mk = (tag, attrs, parent) => {
    const e = document.createElementNS(NS, tag);
    for (const k in attrs) e.setAttribute(k, attrs[k]);
    if (parent) parent.appendChild(e);
    return e;
  };
  const svg = mk("svg", { viewBox: "130 128 720 718", class: "tree", role: "img", "aria-label": "Morse tree" });
  const gEdges = mk("g", { class: "edges" }, svg);
  const gNodes = mk("g", { class: "nodes" }, svg);
  const gLabels = mk("g", { class: "labels" }, svg);

  mk("text", { x: 265, y: 172, class: "title" }, svg).textContent = "MORSE";
  mk("text", { x: 668, y: 172, class: "title" }, svg).textContent = "TREE";
  mk("path", { d: "M450 188 v-34 M440 166 l10 -12 l10 12", class: "arrow" }, svg);

  const edges = {};
  const nodes = {};
  const labels = {};
  const parentOf = (letter) => {
    const p = MORSE[letter];
    return p.length === 1 ? null : DECODE[p.slice(0, -1)];
  };

  for (const L in TREE.nodes) {
    const [x, y] = TREE.nodes[L];
    const par = parentOf(L);
    const [px, py] = par ? TREE.nodes[par] : TREE.root;
    edges[L] = mk("line", { x1: px, y1: py, x2: x, y2: y }, gEdges);
  }
  mk("circle", { cx: TREE.root[0], cy: TREE.root[1], r: 9, class: "root" }, gNodes);
  for (const L in TREE.nodes) {
    const [x, y, lx, ly] = TREE.nodes[L];
    const dah = MORSE[L].endsWith("-");
    const g = mk("g", { class: "node", "data-l": L, tabindex: 0, role: "button", "aria-label": L + " " + MORSE[L] }, gNodes);
    if (dah) mk("rect", { x: x - 27, y: y - 14, width: 54, height: 28, rx: 6 }, g);
    else mk("circle", { cx: x, cy: y, r: 17 }, g);
    mk("circle", { cx: x, cy: y, r: 34, class: "hit" }, g); // larger touch target
    nodes[L] = g;
    const t = mk("text", { x: lx, y: ly + 15, class: "lbl" }, gLabels);
    t.textContent = L;
    labels[L] = t;
    const tap = () => opts.onTap && opts.onTap(L);
    g.addEventListener("click", tap);
    g.addEventListener("keydown", (e) => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); tap(); } });
  }
  host.appendChild(svg);

  let lit = [];
  const api = {
    // Light the path of a dit/dah pattern; returns the letter it ends on.
    path(pattern) {
      for (const L of lit) {
        nodes[L].classList.remove("on", "cur");
        edges[L].classList.remove("on");
        labels[L].classList.remove("on");
      }
      lit = [];
      svg.classList.toggle("off", false);
      let end = "";
      for (let i = 1; i <= pattern.length; i++) {
        const L = DECODE[pattern.slice(0, i)];
        if (!L || !nodes[L]) { svg.classList.toggle("off", true); break; }
        nodes[L].classList.add("on");
        edges[L].classList.add("on");
        labels[L].classList.add("on");
        lit.push(L);
        end = L;
      }
      if (end) nodes[end].classList.add("cur");
      return end;
    },
    flash(letter, kind) {
      const n = nodes[letter];
      if (!n) return;
      n.classList.remove("ok", "bad");
      void n.getBBox();
      n.classList.add(kind);
      setTimeout(() => n.classList.remove(kind), 700);
    },
    clear() { api.path(""); },
  };
  return api;
}
