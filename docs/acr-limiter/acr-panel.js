// ACR LIMITER live panel (19.11.2026). Everything you see is the plugin's own pixels, baked by the plugin itself
// (ACR LIMITER Source/WebBake.inc, env ACR_BAKE): the panel without its moving parts, every switch state, slider and
// AMOUNT arc frames, the Druk glyphs, layout.json. This file only puts them together and moves them, as the plugin
// window does (PluginEditor.cpp: paintContent / paintGraph / timerCallback / updateOrb), and sends every change to the
// engine through acr-live.js (houzyParam). The sphere is orb.js (the plugin's LiquidOrb port) in PLUM.
(function () {
  var panel = document.getElementById('panel');
  if (!panel) return;
  var BASE = '/acr-limiter/panel/', V = '?v=20261119e';
  var lang = (document.documentElement.lang || 'en').slice(0, 2) === 'ru' ? 'ru' : 'en';
  var GW = 740, GH = 500;
  var RANGE = 36, GRBAR = 24;   // kRangeDb, kGrBarDb

  fetch(BASE + 'layout.json' + V).then(function (r) { return r.json(); }).then(build).catch(function (e) { console.warn('acr panel', e); });

  function build(L) {
    // ---- the stage: one box in the 740 x 500 grid, every piece placed in % of it ----
    var img = panel.querySelector('img');
    var stage = document.createElement('div');
    stage.className = 'acr-stage';
    stage.style.cssText = 'position:absolute;inset:0;overflow:hidden;border-radius:inherit;touch-action:none;user-select:none';
    if (getComputedStyle(panel).position === 'static') panel.style.position = 'relative';
    var T = 1;   // the theme: WARPIK, ACR LIMITER's default (kThemeRowId order in L.themes)
    var chromeUrl = function () { return BASE + 't' + T + '_chrome.webp' + V; };   // English key; the key itself is a layer
    var lng = 0;   // the panel's language: EN on the site, both pages (user 19.11.2026); the plugin's 7 in its LANGUAGE card
    if (img) img.src = chromeUrl();
    panel.appendChild(stage);
    var place = function (el, r) {
      el.style.position = 'absolute';
      el.style.left = (r[0] / GW * 100) + '%'; el.style.top = (r[1] / GH * 100) + '%';
      el.style.width = (r[2] / GW * 100) + '%'; el.style.height = (r[3] / GH * 100) + '%';
      stage.appendChild(el); return el;
    };
    var pic = function (r) { var i = new Image(); i.alt = ''; i.draggable = false; i.style.display = 'block'; return place(i, r); };
    // layer order as the plugin paints: the graph (content) under the "+X dB" pill, the controls, the sphere and the arc
    var gcv = place(document.createElement('canvas'), [0, 0, GW, GH]); gcv.style.pointerEvents = 'none';
    var gctx = gcv.getContext('2d');
    var pill = place(document.createElement('div'), L.readout);   // the pill itself, cut from the baked panel
    var pillBg = function () {
      pill.style.background = 'url(' + chromeUrl() + ') ' + (L.readout[0] / (GW - L.readout[2]) * 100) + '% ' +
        (L.readout[1] / (GH - L.readout[3]) * 100) + '% / ' + (GW / L.readout[2] * 100) + '% ' + (GH / L.readout[3] * 100) + '% no-repeat';
    };
    pillBg();
    pill.style.borderRadius = (11 / L.readout[2] * 100) + '% / ' + (11 / L.readout[3] * 100) + '%';

    // ---- state, as the plugin's defaults (a fresh window) ----
    var P = { amount: 0, acr: 1, truepeak: 1, bypass: 0, limmode: 1, phasemode: 0, osfactor: 1, ceiling: -0.1, character: 0, output: 0 };
    var dirty = true;
    var set = function (id, v) { P[id] = v; if (window.houzyParam) window.houzyParam(id, v); dirty = true; show(); };

    // ---- switches: one picture per state ----
    var sw = {
      lim: { id: 'limmode', n: 2 }, acr: { id: 'acr', n: 2 }, tp: { id: 'truepeak', n: 2 },
      bypass: { id: 'bypass', n: 2 }, phase: { id: 'phasemode', n: 3 }, os: { id: 'osfactor', n: 5 }
    };
    // the header keys are baked per theme (with their background), T8 | T9 alone on transparency (over the graph)
    var swUrl = function (k, i) { return BASE + (k === 'lim' ? '' : 't' + T + '_') + k + '_' + i + '.webp' + V; };
    var preload = function () { Object.keys(sw).forEach(function (k) { for (var i = 0; i < sw[k].n; i++) { var pre = new Image(); pre.src = swUrl(k, i); } }); };
    Object.keys(sw).forEach(function (k) { sw[k].el = pic(L[k]); });
    preload();   // no flash on the first click
    var langEl = pic(L.langBtn);   // the language key, baked per theme and language
    var langUrl = function () { return BASE + 't' + T + '_lang_' + lng + '.webp' + V; };
    // ---- sliders: one strip each, the frame by background position ----
    var sl = {
      ceil: { id: 'ceiling', lo: -12, hi: 0, n: L.sliderFrames[0] }, char: { id: 'character', lo: 0, hi: 100, n: L.sliderFrames[1] },
      out: { id: 'output', lo: -12, hi: 0, n: L.sliderFrames[2] }
    };
    Object.keys(sl).forEach(function (k) {
      var s = sl[k]; s.el = place(document.createElement('div'), L[k]);
      s.el.style.background = 'url(' + BASE + k + '_strip.webp' + V + ') 0 0 / 100% ' + (s.n * 100) + '% no-repeat';
    });
    // ---- AMOUNT: the sphere (orb.js, PLUM) under the arc frames ----
    var orbHost = place(document.createElement('div'), L.sphere);
    orbHost.dataset.orb = 'plum';
    var orb = window.HouzyOrb ? window.HouzyOrb(orbHost) : null;
    var orbPick = 3;   // PLUM in kOrbColours (MINT, GRAPHITE, RUBY, PLUM)
    var cvO = orbHost.querySelector('canvas'); if (cvO) cvO.style.cssText = 'width:100%;height:100%;display:block';
    // the tube: empty under, lit on top through a conic mask up to the value (any value, as smooth as the plugin's),
    // and the glass bubble at its end - all three baked by the plugin's own paintArc
    var arcE = pic(L.amount), arcF = pic(L.amount), bead = pic([0, 0, 16, 16]);
    arcE.src = BASE + 'arc_empty.webp' + V; arcF.src = BASE + 'arc_full.webp' + V; bead.src = BASE + 'bead.webp' + V;
    [arcE, arcF, bead].forEach(function (e) { e.style.pointerEvents = 'none'; });
    var acx = (L.arcC[0] - L.amount[0]) / L.amount[2] * 100, acy = (L.arcC[1] - L.amount[1]) / L.amount[3] * 100;
    var shown = 0;   // the arc's eased value, as SphereKnob::shownProp (tau 67 ms)
    function arcAt(prop) {
      var deg = 270 * prop, m = 'conic-gradient(from 225deg at ' + acx + '% ' + acy + '%, #000 0deg, #000 ' + deg + 'deg, transparent ' + deg + 'deg)';
      arcF.style.webkitMaskImage = m; arcF.style.maskImage = m;
      var a = (-135 + 270 * prop) * Math.PI / 180;
      var x = L.arcC[0] + L.arcR * Math.sin(a), y = L.arcC[1] - L.arcR * Math.cos(a);
      bead.style.left = ((x - 8) / GW * 100) + '%'; bead.style.top = ((y - 8) / GH * 100) + '%';
    }
    arcAt(0);
    // ---- one canvas for the words, the meters and the graph ----
    var cv = place(document.createElement('canvas'), [0, 0, GW, GH]); cv.style.pointerEvents = 'none';
    var ctx = cv.getContext('2d');   // the top canvas: words and meters (draw() points it at the graph canvas for a while)

    function show() {
      Object.keys(sw).forEach(function (k) { var s = sw[k], src = swUrl(k, Math.round(P[s.id])); if (s.el.getAttribute('src') !== src) s.el.src = src; });
      if (langEl.getAttribute('src') !== langUrl()) langEl.src = langUrl();
      Object.keys(sl).forEach(function (k) {
        var s = sl[k], f = Math.round((P[s.id] - s.lo) / (s.hi - s.lo) * (s.n - 1));
        s.el.style.backgroundPositionY = (s.n > 1 ? f / (s.n - 1) * 100 : 0) + '%';
      });
      var amt = document.querySelector('.live-amount input'), out = document.querySelector('.live-amount output');
      if (amt && document.activeElement !== amt) { amt.value = P.amount; if (out) out.textContent = '+' + P.amount.toFixed(1) + ' dB'; }
    }

    // ---- glyphs: the plugin's Druk, cell by cell ----
    var G = {}, chars = L.chars;
    Object.keys(L.glyphs).forEach(function (k) { var i = new Image(); i.onload = function () { dirty = true; }; i.src = BASE + 'glyph_' + k + '.webp' + V; G[k] = { img: i, m: L.glyphs[k] }; });
    function text(style, s, r, align) {
      var g = G[style]; if (!g || !g.img.complete) return;
      var m = g.m, w = 0, i, idx;
      for (i = 0; i < s.length; i++) { idx = chars.indexOf(s[i]); w += idx < 0 ? m.adv[0] : m.adv[idx]; }
      var x = align === 'l' ? r[0] : align === 'r' ? r[0] + r[2] - w : r[0] + (r[2] - w) / 2;
      var y = r[1] + (r[3] - m.cellH) / 2;
      for (i = 0; i < s.length; i++) {
        idx = chars.indexOf(s[i]); if (idx < 0) idx = 0;
        if (idx > 0) ctx.drawImage(g.img, idx * m.cellW * 2, 0, m.cellW * 2, m.cellH * 2, x, y, m.cellW, m.cellH);
        x += m.adv[idx];
      }
    }
    var f1 = function (v) { var s = v.toFixed(1); return s === '-0.0' ? '0.0' : s; };

    // ---- meters, as the plugin's timerCallback ----
    var N = L.hist, hin = new Float32Array(N), hout = new Float32Array(N), hgr = new Float32Array(N).fill(1), hw = 0, seen = -1, lastM = null;
    var grShown = 0, grMax = 0, outPeak = -100, lm = -100, ls = -100, li = -100;
    var db = function (lin, floor) { return lin > 0 ? Math.max(floor, 20 * Math.log10(lin)) : floor; };
    function meters(m) {
      if (!m || m === lastM) return;
      lastM = m;
      var total = m[4];
      if (seen < 0 || total < seen) seen = Math.max(0, total - N);
      for (var t = seen; t < total; t++) {
        var at = 5 + 3 * (t % N);
        hin[hw] = m[at]; hout[hw] = m[at + 1]; hgr[hw] = m[at + 2]; hw = (hw + 1) % N;
        outPeak = Math.max(outPeak, db(m[at + 1], -100));
        grMax = Math.min(grMax, db(Math.max(m[at + 2], 1e-6), -60));
      }
      seen = total;
      var g = m[0]; grShown = g < grShown ? g : grShown + (g - grShown) * 0.25;
      lm = m[1]; ls = m[2]; li = m[3];
      dirty = true;
    }

    var C = L.colours, rgba = function (hex, a) { var n = parseInt(hex.slice(1), 16); return 'rgba(' + (n >> 16 & 255) + ',' + (n >> 8 & 255) + ',' + (n & 255) + ',' + a + ')'; };
    function rr(x, y, w, h, r) { ctx.beginPath(); if (ctx.roundRect) ctx.roundRect(x, y, w, h, r); else ctx.rect(x, y, w, h); }
    function draw() {
      var box = stage.getBoundingClientRect(), dpr = Math.min(window.devicePixelRatio || 1, 2);
      var w = Math.round(box.width * dpr), h = Math.round(box.height * dpr);
      if (!w) return;
      if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; gcv.width = w; gcv.height = h; }
      ctx.setTransform(w / GW, 0, 0, h / GH, 0, 0);
      ctx.clearRect(0, 0, GW, GH);
      gctx.setTransform(w / GW, 0, 0, h / GH, 0, 0);
      gctx.clearRect(0, 0, GW, GH);
      var top = ctx; ctx = gctx;   // the graph goes to the bottom canvas
      // graph (paintGraph): input, output, gain reduction, the ceiling; under the layout-B glass panels the plugin
      // lays black 18 % + white 6 % over the curves - here the curves are drawn there at 76 %
      var a = L.graph, yOf = function (d) { return a[1] + a[3] * Math.min(1, Math.max(0, -d / RANGE)); };
      for (var pass = 0; pass < 2; pass++) {
      ctx.save(); rr(a[0], a[1], a[2], a[3], 6); ctx.clip();
      ctx.beginPath(); ctx.rect(0, 0, GW, GH);
      [L.panelL, L.panelR].forEach(function (q) { if (ctx.roundRect) ctx.roundRect(q[0], q[1], q[2], q[3], 6); else ctx.rect(q[0], q[1], q[2], q[3]); });
      if (pass === 0) ctx.clip('evenodd');
      else { ctx.beginPath(); [L.panelL, L.panelR].forEach(function (q) { if (ctx.roundRect) ctx.roundRect(q[0], q[1], q[2], q[3], 6); else ctx.rect(q[0], q[1], q[2], q[3]); }); ctx.clip(); ctx.globalAlpha = 0.76; }
      var step = a[2] / (N - 1), i, k, x;
      var path = function (arr, base, val) {
        ctx.beginPath(); ctx.moveTo(a[0], base);
        for (i = 0; i < N; i++) { k = (hw + i) % N; ctx.lineTo(a[0] + step * i, yOf(val(arr[k]))); }
        ctx.lineTo(a[0] + a[2], base); ctx.closePath();
      };
      var line = function (arr, val) {
        ctx.beginPath();
        for (i = 0; i < N; i++) { k = (hw + i) % N; x = a[0] + step * i; if (i) ctx.lineTo(x, yOf(val(arr[k]))); else ctx.moveTo(x, yOf(val(arr[k]))); }
      };
      var lvl = function (v) { return db(v, -RANGE - 1); }, gain = function (v) { return db(Math.max(v, 1e-6), -200); };
      path(hin, a[1] + a[3], lvl); ctx.fillStyle = 'rgba(255,255,255,0.13)'; ctx.fill();
      var og = ctx.createLinearGradient(0, a[1], 0, a[1] + a[3]); og.addColorStop(0, rgba(C.cyan, 0.5)); og.addColorStop(1, rgba(C.cyan, 0.18));
      path(hout, a[1] + a[3], lvl); ctx.fillStyle = og; ctx.fill();
      line(hout, lvl); ctx.strokeStyle = rgba(C.cyan, 0.85); ctx.lineWidth = 1; ctx.stroke();
      var gg = ctx.createLinearGradient(0, a[1], 0, a[1] + a[3] * 0.3); gg.addColorStop(0, rgba(C.orange, 0.65)); gg.addColorStop(1, rgba(C.orange, 0.3));
      path(hgr, a[1], gain); ctx.fillStyle = gg; ctx.fill();
      line(hgr, gain); ctx.strokeStyle = rgba(C.orangeHi, 0.95); ctx.lineWidth = 1.2; ctx.stroke();
      var cy = yOf(P.ceiling) + 0.5;
      ctx.setLineDash([4, 4]); ctx.beginPath(); ctx.moveTo(a[0], cy); ctx.lineTo(a[0] + a[2], cy); ctx.strokeStyle = 'rgba(255,255,255,0.45)'; ctx.lineWidth = 1; ctx.stroke(); ctx.setLineDash([]);
      text('dim', 'CEILING ' + f1(P.ceiling) + ' dBTP', [a[0] + 8, cy + 4, 160, 10], 'l');
      ctx.restore();
      }
      ctx = top;
      // side columns (paintContent): readouts and bars
      var cell = function (r, v, hot) { text(hot ? 'hot' : 'val', v <= -99 ? '-INF' : Math.abs(v) < 0.05 ? '0.0' : v.toFixed(1), r, 'c'); };
      cell(L.cellsL[0], grShown, grShown < -0.05); cell(L.cellsL[1], grMax, grMax < -0.05); cell(L.cellsL[2], outPeak, false);
      cell(L.cellsR[0], lm, false); cell(L.cellsR[1], ls, false); cell(L.cellsR[2], li, false);
      var bar = function (b, frac, fromTop, col) {
        frac = Math.min(1, Math.max(0, frac)); if (frac <= 0.001) return;
        var fh = b[3] * frac, fy = fromTop ? b[1] : b[1] + b[3] - fh;
        var gr = ctx.createLinearGradient(0, fromTop ? b[1] : b[1] + b[3], 0, fromTop ? fy + fh : fy);
        gr.addColorStop(0, rgba(col, 0.95)); gr.addColorStop(1, rgba(col, 0.55));
        ctx.fillStyle = gr; rr(b[0], fy, b[2], fh, 4); ctx.fill();
      };
      bar(L.barL, -grShown / GRBAR, true, C.orange);
      bar(L.barR, 1 - Math.min(1, Math.max(0, -lm / RANGE)), false, C.cyan);
      // layout B values (paintPanels) and the AMOUNT readout
      text('cap', f1(P.ceiling) + ' dB', L.valCeil, 'r');
      text('cap', String(Math.round(P.character)), L.valChar, 'r');
      text('cap', f1(P.output) + ' dB', L.valOut, 'r');
      text('amt', '+' + P.amount.toFixed(1) + ' dB', L.readout, 'c');
    }

    // ---- the sphere, as updateOrb: AMOUNT spins and deepens it, the limiter makes it breathe ----
    var amtPrev = -1, vYaw = 0, last = 0;
    function frame(now) {
      var dt = last ? Math.min(0.05, (now - last) / 1000) : 0; last = now;
      var amt = P.amount / 18;
      if (amtPrev >= 0) vYaw = Math.max(-12, Math.min(12, vYaw + (amt - amtPrev) * 40));
      amtPrev = amt;
      if (orb && orb.st && uiOff) { orb.st.speed = 0; vYaw = 0; }
      else if (orb && orb.st) {
        orb.st.yaw += vYaw * dt; vYaw *= Math.exp(-dt * 2.2);
        var comp = Math.min(1, Math.max(0, -grShown / 12));
        orb.st.amp = 1.6 * (0.70 + 0.50 * amt) * (1 + 0.30 * comp);
        orb.st.speed = 0.8 + 0.8 * amt; orb.st.spin = 0;
      }
      var target = P.amount / 18;
      if (shown !== target) { shown += (target - shown) * (1 - Math.exp(-dt * 1000 / 67.1)); if (Math.abs(target - shown) < 0.0005) shown = target; arcAt(shown); }
      meters(window.houzyMeters);
      if (dirty) { dirty = false; draw(); }
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);

    // ---- the cards of THEMES / UI / OS (PresetMenu): baked open, one picture per lit row, per theme ----
    var menuOpen = null, uiOff = false;
    var menuEl = pic([0, 0, 1, 1]);
    menuEl.style.cssText += ';transform-origin:50% 0;transition:opacity .16s ease,transform .2s cubic-bezier(.2,.8,.2,1);opacity:0;transform:scale(.96);pointer-events:none;visibility:hidden';
    var lit = function (name) { return name === 'themes' ? L.themes.indexOf(T) : name === 'ui' ? (uiOff ? 4 : orbPick) : name === 'lang' ? lng : P.osfactor; };
    var menuUrl = function (name) { return BASE + 't' + T + '_menu_' + name + '_' + lit(name) + '.webp' + V; };
    function paintMenu() {
      if (!menuOpen) return;
      var M = L.menus[menuOpen], c = M.card;
      menuEl.style.left = (c[0] / GW * 100) + '%'; menuEl.style.top = (c[1] / GH * 100) + '%';
      menuEl.style.width = (c[2] / GW * 100) + '%'; menuEl.style.height = (c[3] / GH * 100) + '%';
      var u = menuUrl(menuOpen), pre = new Image();
      pre.onload = function () { if (menuOpen) menuEl.src = u; };
      pre.src = u;
    }
    function openMenu(name) {
      if (menuOpen === name) { closeMenu(); return; }
      menuOpen = name; paintMenu();
      menuEl.style.visibility = 'visible';
      requestAnimationFrame(function () { menuEl.style.opacity = '1'; menuEl.style.transform = 'scale(1)'; });
    }
    function closeMenu() {
      menuOpen = null; menuEl.style.opacity = '0'; menuEl.style.transform = 'scale(.96)';
      setTimeout(function () { if (!menuOpen) menuEl.style.visibility = 'hidden'; }, 220);
    }
    function pick(name, i) {
      if (name === 'themes') {
        T = L.themes[i];
        var ci = new Image(); ci.onload = function () { if (img) img.src = ci.src; pillBg(); show(); paintMenu(); }; ci.src = chromeUrl(); preload();
      } else if (name === 'ui') {
        uiOff = i === 4;
        if (!uiOff) { orbPick = i; var o = L.orbs[i]; if (orb && orb.setColours) orb.setColours(o[1], o[2], o[3]); }
        paintMenu();
      } else if (name === 'lang') { lng = i; show(); paintMenu(); }   // the plugin: the key shows the code (its texts are tips)
      else { set('osfactor', i); paintMenu(); }
    }
    function menuClick(p) {
      var M = L.menus[menuOpen];
      if (!inR(p, M.card) || inR(p, M.close)) { closeMenu(); return; }   // outside or the cross: shut (a pick does not)
      for (var i = 0; i < M.rows.length; i++) if (inR(p, M.rows[i])) { pick(menuOpen, i); return; }
    }

    // ---- the mouse / finger, as the plugin's controls ----
    var toGrid = function (e) { var b = stage.getBoundingClientRect(); return [(e.clientX - b.left) / b.width * GW, (e.clientY - b.top) / b.height * GH]; };
    var inR = function (p, r) { return p[0] >= r[0] && p[0] <= r[0] + r[2] && p[1] >= r[1] && p[1] <= r[1] + r[3]; };
    var drag = null;
    stage.addEventListener('pointerdown', function (e) {
      var p = toGrid(e);
      if (menuOpen) { menuClick(p); e.preventDefault(); return; }   // an open card takes the click (as PresetMenu does)
      if (inR(p, L.sphere)) { drag = { kind: 'amount', y: p[1] }; }
      else {
        var hit = null;
        Object.keys(sl).forEach(function (k) { if (inR(p, L[k])) hit = k; });
        if (hit) { drag = { kind: 'slider', k: hit }; slide(hit, p); }
        else if (inR(p, L.lim)) set('limmode', p[0] < L.lim[0] + L.lim[2] / 2 ? 0 : 1);
        else if (inR(p, L.acr)) set('acr', P.acr ? 0 : 1);
        else if (inR(p, L.tp)) set('truepeak', P.truepeak ? 0 : 1);
        else if (inR(p, L.bypass)) { if (window.houzyBypass) window.houzyBypass(); else set('bypass', P.bypass ? 0 : 1); }
        else if (inR(p, L.phase)) set('phasemode', (P.phasemode + 1) % 3);
        else if (inR(p, L.themesBtn)) openMenu('themes');   // the plugin's cards hang off these three keys
        else if (inR(p, L.uiBtn)) openMenu('ui');
        else if (inR(p, L.os)) openMenu('os');
        else if (inR(p, L.langBtn)) openMenu('lang');
        else if (inR(p, L.mmBtn)) { if (window.houzySay) window.houzySay(lang === 'ru' ? 'MIX & MASTER — каталог инженеров, работает в самом плагине' : 'MIX & MASTER — the engineers catalogue, inside the plugin'); }
        else if (inR(p, L.cellsR[2])) { if (window.houzySend) window.houzySend('lufsreset', 1); }
        else if (inR(p, L.cellsL[1]) || inR(p, L.cellsL[2])) { grMax = 0; outPeak = -100; dirty = true; }
        else return;
      }
      e.preventDefault();
      if (drag) stage.setPointerCapture(e.pointerId);
    });
    stage.addEventListener('pointermove', function (e) {
      var p = toGrid(e), over = inR(p, L.sphere) || ['lim', 'acr', 'tp', 'bypass', 'phase', 'os', 'ceil', 'char', 'out', 'themesBtn', 'uiBtn', 'mmBtn', 'langBtn'].some(function (k) { return inR(p, L[k]); });
      stage.style.cursor = over ? 'pointer' : '';
      if (!drag) return;
      if (drag.kind === 'amount') {
        var fine = e.ctrlKey || e.shiftKey || e.metaKey;
        var speed = (fine ? 1 / 1200 : 1.13 / 1200) * 0.65;   // SphereKnob::mouseDrag, per point of the 740 grid
        var prop = Math.min(1, Math.max(0, P.amount / 18 - (p[1] - drag.y) * speed));
        drag.y = p[1];
        set('amount', Math.round(prop * 1800) / 100);
      } else slide(drag.k, p);
    });
    var end = function () { drag = null; };
    stage.addEventListener('pointerup', end); stage.addEventListener('pointercancel', end);
    stage.addEventListener('dblclick', function (e) { if (inR(toGrid(e), L.sphere)) set('amount', 0); });
    function slide(k, p) {
      var s = sl[k], r = L[k], f = Math.min(1, Math.max(0, (p[0] - r[0] - 9) / (r[2] - 18)));
      var v = s.lo + (s.hi - s.lo) * f;
      set(s.id, s.id === 'character' ? Math.round(v) : Math.round(v * 10) / 10);
    }

    // the player's AMOUNT slider and A/B button stay in step with the panel
    var amtIn = document.querySelector('.live-amount input');
    if (amtIn) amtIn.addEventListener('input', function () { P.amount = +amtIn.value; dirty = true; show(); });
    document.addEventListener('houzy-bypass', function (e) { P.bypass = e.detail ? 1 : 0; show(); });
    show();
  }
})();
