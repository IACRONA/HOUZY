// BUY - the shop is not open yet: a click shows "Coming soon".
// Hover = the plugin licence window's Neon Border (LicenceCard, PluginEditor.h, #D5BB9B): two arcs on opposite sides
// glide corner to corner round the edge, glow outside only. (Its orange Starfield was tried here and taken off.)
// And a tendril in the colour of the plugin's sphere (--orb-tint, set by plugin.js) reaches out of the sphere and wraps the key.
(function () {
  var keys = [].slice.call(document.querySelectorAll('.buy'));
  if (!keys.length) return;
  var NEON = '213,187,155', REACH = 14, LAP = 10;
  var porb = document.querySelector('.porb');

  // a point on a rounded rectangle's outline, t in 0..1 from the top middle, clockwise (onRoundRect)
  function onRR(t, x, y, w, h, r) {
    r = Math.max(0, Math.min(r, w / 2, h / 2));
    var sx = Math.max(0, w - 2 * r), sy = Math.max(0, h - 2 * r), arc = Math.PI / 2 * r, total = 2 * sx + 2 * sy + 4 * arc;
    var d = ((((t % 1) + 1) % 1) * total + sx / 2) % total, a;
    if (d < sx) return [x + r + d, y]; d -= sx;
    if (d < arc) { a = d / r; return [x + w - r + r * Math.sin(a), y + r - r * Math.cos(a)]; } d -= arc;
    if (d < sy) return [x + w, y + r + d]; d -= sy;
    if (d < arc) { a = d / r; return [x + w - r + r * Math.cos(a), y + h - r + r * Math.sin(a)]; } d -= arc;
    if (d < sx) return [x + w - r - d, y + h]; d -= sx;
    if (d < arc) { a = d / r; return [x + r - r * Math.sin(a), y + h - r + r * Math.cos(a)]; } d -= arc;
    if (d < sy) return [x, y + h - r - d]; d -= sy;
    a = d / r; return [x + r - r * Math.cos(a), y + r - r * Math.sin(a)];
  }
  function rrPath(c, x, y, w, h, r) { c.beginPath(); c.roundRect(x, y, w, h, r); }
  // cubic-bezier(.65,0,.35,1) (glideEase)
  function glide(x) {
    var b = function (a, c, s) { var u = 1 - s; return 3 * u * u * s * a + 3 * u * s * s * c + s * s * s; }, s = x;
    for (var i = 0; i < 8; i++) {
      var u = 1 - s, dx = 3 * u * u * .65 + 6 * u * s * (.35 - .65) + 3 * s * s * (1 - .35);
      if (Math.abs(dx) < 1e-6) break;
      s = Math.min(1, Math.max(0, s - (b(.65, .35, s) - x) / dx));
    }
    return b(0, 1, s);
  }
  // where the neon is after sec seconds: one lap per 10 s, stopping at each corner (neonLap)
  function neonLap(sec, w, h, r) {
    var sx = Math.max(0, w - 2 * r), sy = Math.max(0, h - 2 * r), arc = Math.PI / 2 * r, T = 2 * sx + 2 * sy + 4 * arc;
    var at = [(sx / 2 + arc / 2) / T, (sx / 2 + arc * 1.5 + sy) / T, (sx * 1.5 + arc * 2.5 + sy) / T, (sx * 1.5 + arc * 3.5 + sy * 2) / T];
    var corner = function (k) { var q = Math.floor(k / 4); return q + at[k - q * 4]; };
    var beats = sec / (LAP / 4), k = Math.floor(beats);
    return corner(k) + (corner(k + 1) - corner(k)) * glide(beats - k);
  }

  // the "Coming soon" bubble, one for the page
  var tip = document.createElement('div'); tip.className = 'buy-tip'; tip.setAttribute('role', 'status'); tip.textContent = window.houzyT ? window.houzyT('Coming soon') : 'Coming soon';
  document.body.appendChild(tip);
  var tipTimer = 0;
  function showTip(el) {
    var r = el.getBoundingClientRect();
    tip.style.left = (r.left + r.width / 2) + 'px'; tip.style.top = (r.bottom + 10) + 'px';
    if (window.houzyT) tip.textContent = window.houzyT('Coming soon');
    tip.classList.add('on'); clearTimeout(tipTimer); tipTimer = setTimeout(function () { tip.classList.remove('on'); }, 1800);
  }

  // the tendril: one fixed SVG over the page
  var NS = 'http://www.w3.org/2000/svg', svg = document.createElementNS(NS, 'svg');
  svg.setAttribute('class', 'buy-arm'); svg.setAttribute('aria-hidden', 'true');
  svg.innerHTML = '<path class="b-body"/><path class="b-core"/><path class="b-glint"/>';
  document.body.appendChild(svg);
  var pBody = svg.children[0], pCore = svg.children[1], pGlint = svg.children[2];

  var S = keys.map(function (el) {
    var cv = document.createElement('canvas'); cv.className = 'buy-fx'; cv.setAttribute('aria-hidden', 'true');
    el.appendChild(cv);
    var st = { el: el, cv: cv, c: cv.getContext('2d'), hover: false, glow: 0, t0: 0 };
    el.addEventListener('pointerenter', function (e) {
      if (e.pointerType !== 'mouse') return;
      st.hover = true; wake();
      if (window.houzyUiSound) window.houzyUiSound('buy');   // site.js: quiet while the demo track is audible
    });
    el.addEventListener('pointerleave', function () { st.hover = false; wake(); });
    el.addEventListener('focus', function () { st.hover = true; wake(); });
    el.addEventListener('blur', function () { st.hover = false; wake(); });
    el.addEventListener('click', function () { showTip(el); });
    return st;
  });
  var reach = 0, reachV = 0, armFor = null, running = false, last = 0;

  function drawKey(st, now) {
    var el = st.el, w = el.offsetWidth, h = el.offsetHeight, dpr = Math.min(2, window.devicePixelRatio || 1);
    var W = w + 2 * REACH, H = h + 2 * REACH, c = st.c, g = st.glow, r = h / 2;
    if (st.cv.width !== Math.round(W * dpr) || st.cv.height !== Math.round(H * dpr)) { st.cv.width = Math.round(W * dpr); st.cv.height = Math.round(H * dpr); }
    c.setTransform(dpr, 0, 0, dpr, 0, 0); c.clearRect(0, 0, W, H);
    if (g <= 0.002) return;
    var x0 = REACH, y0 = REACH;
    // NEON BORDER outside the edge
    var th = 2.5, ex = x0 - th / 2, ey = y0 - th / 2, ew = w + th, eh = h + th, er = r + th / 2;
    var lap = neonLap((now - st.t0) / 1000, ew, eh, er), N = 48;
    c.save(); c.beginPath(); c.rect(0, 0, W, H); c.roundRect(x0, y0, w, h, r); c.clip('evenodd');
    c.lineCap = 'butt';
    for (var arcI = 0; arcI < 2; arcI++) {
      var pts = [], al = [];
      for (var j = 0; j <= N; j++) {
        var f = j / N, tt = Math.abs(f - 0.5) * 2, kk = tt <= 0.5 ? 1 : 1 - (tt - 0.5) / 0.5;
        al.push(kk * kk * (3 - 2 * kk)); pts.push(onRR(lap + 0.5 * arcI + (f - 0.5) * 0.25, ex, ey, ew, eh, er));
      }
      for (j = 0; j < N; j++) {
        var aa = 0.5 * (al[j] + al[j + 1]) * g; if (aa < 0.01) continue;
        c.beginPath(); c.moveTo(pts[j][0], pts[j][1]); c.lineTo(pts[j + 1][0], pts[j + 1][1]);
        c.strokeStyle = 'rgba(' + NEON + ',' + (0.18 * aa) + ')'; c.lineWidth = REACH * 1.2; c.stroke();
        c.strokeStyle = 'rgba(' + NEON + ',' + (0.30 * aa) + ')'; c.lineWidth = 7; c.stroke();
        c.strokeStyle = 'rgba(' + NEON + ',' + aa + ')'; c.lineWidth = th; c.stroke();
      }
    }
    c.restore();
  }

  // the tendril: a liquid tube out of the sphere to the key, then once round the key's own outline (same radius, 5 px off).
  // Like the sphere's surface it never sits still: its width ripples, the line sways, a glint runs along it.
  function drawArm(dt) {
    var target = null;
    S.forEach(function (st) { if (st.hover) target = st; });
    var sr = porb && porb.getBoundingClientRect(), vis = sr && sr.width > 0 && sr.bottom > 0 && sr.top < innerHeight;
    if (target && vis) armFor = target;
    var goal = target && vis && armFor === target ? 1 : 0;
    var kS = 70, cS = 2 * Math.sqrt(kS) * 0.7;                                 // a little overshoot, like the stage's arms
    reachV += (kS * (goal - reach) - cS * reachV) * dt; reach += reachV * dt;
    if (goal === 0 && reach < 0.002) { reach = 0; reachV = 0; armFor = null; svg.style.opacity = '0'; return false; }
    if (!armFor || !sr) return true;
    var b = armFor.el.getBoundingClientRect(), pad = 5;
    var rad = Math.min(b.height / 2, parseFloat(getComputedStyle(armFor.el).borderTopLeftRadius) || 0) + pad;
    var bx = b.left - pad, by = b.top - pad, bw = b.width + 2 * pad, bh = b.height + 2 * pad;
    var ox = sr.left + sr.width / 2, oy = sr.top + sr.height / 2, R = sr.width / 2 * 0.8;
    // the point on the key's outline that faces the sphere
    var bestT = 0, bd = 1e9, i;
    for (i = 0; i < 96; i++) { var q = onRR(i / 96, bx, by, bw, bh, rad), dq = Math.hypot(q[0] - ox, q[1] - oy); if (dq < bd) { bd = dq; bestT = i / 96; } }
    var e = onRR(bestT, bx, by, bw, bh, rad), ang = Math.atan2(e[1] - oy, e[0] - ox);
    var s0 = [ox + Math.cos(ang) * R, oy + Math.sin(ang) * R], dist = Math.hypot(e[0] - s0[0], e[1] - s0[1]);
    var c1 = [s0[0] + Math.cos(ang) * dist * 0.35, s0[1] + Math.sin(ang) * dist * 0.35 + dist * 0.12];
    var c2 = [e[0] - Math.cos(ang) * dist * 0.3, e[1] - Math.sin(ang) * dist * 0.3 - dist * 0.08];
    // the centreline: the reach, then a little more than one lap so the end tucks over the start
    var pts = [], NB = 40, NW = 140, dir = e[0] < bx + bw / 2 ? 1 : -1;
    for (i = 0; i <= NB; i++) {
      var u = i / NB, v = 1 - u;
      pts.push([v * v * v * s0[0] + 3 * v * v * u * c1[0] + 3 * v * u * u * c2[0] + u * u * u * e[0],
                v * v * v * s0[1] + 3 * v * v * u * c1[1] + 3 * v * u * u * c2[1] + u * u * u * e[1]]);
    }
    for (i = 1; i <= NW; i++) pts.push(onRR(bestT + dir * i / NW * 1.04, bx, by, bw, bh, rad));
    var cum = [0];
    for (i = 1; i < pts.length; i++) cum.push(cum[i - 1] + Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]));
    var L = cum[cum.length - 1], Lb = cum[NB], shown = Math.max(0, Math.min(1, reach)) * L, t = performance.now() / 1000;
    var Lp = [], Rp = [], H = [], C = [];
    for (i = 0; i < pts.length && cum[i] <= shown; i++) {
      var a = pts[Math.max(0, i - 1)], c = pts[Math.min(pts.length - 1, i + 1)];
      var dx = c[0] - a[0], dy = c[1] - a[1], dl = Math.hypot(dx, dy) || 1, nx = -dy / dl, ny = dx / dl, sa = cum[i];
      var base = sa < Lb ? 10 - 6 * (sa / Lb) : 4;                              // thick out of the sphere, thinner round the key
      var ripple = 1 + 0.3 * Math.sin(sa * 0.07 - t * 4.2) + 0.12 * Math.sin(sa * 0.19 + t * 2.7);
      var w = base * ripple * Math.sqrt(Math.min(1, (shown - sa) / 16));         // a rounded, growing tip
      var wob = Math.sin(sa * 0.045 - t * 3.1) * (sa < Lb ? 2.4 * Math.min(1, sa / 40) : 0.9);   // the line itself sways
      var px = pts[i][0] + nx * wob, py = pts[i][1] + ny * wob;
      Lp.push([px + nx * w / 2, py + ny * w / 2]); Rp.push([px - nx * w / 2, py - ny * w / 2]);
      H.push([px - nx * ny * w * 0.3, py - ny * ny * w * 0.3]);                  // the specular, on the upper side of the tube
      C.push([px, py]);
    }
    if (Lp.length < 2) { svg.style.opacity = '0'; return true; }
    var f = function (p) { return p[0].toFixed(1) + ' ' + p[1].toFixed(1); };
    var line = function (P) { return 'M' + P.map(f).join('L'); };
    var tint = getComputedStyle(document.documentElement).getPropertyValue('--orb-tint').trim() || '#33b9b9';
    pBody.setAttribute('d', line(Lp) + 'L' + Rp.reverse().map(f).join('L') + 'Z');
    pBody.style.fill = tint; pBody.style.filter = 'drop-shadow(0 0 6px ' + tint + ')';
    pCore.setAttribute('d', line(H));
    pGlint.setAttribute('d', line(C));
    var Lc = cum[C.length - 1];
    pGlint.style.strokeDasharray = '16 ' + (Lc + 30);                          // a glint running along it once it is out
    pGlint.style.strokeDashoffset = (-((performance.now() / 5) % (Lc + 46))).toFixed(1);
    pGlint.style.opacity = shown > L * 0.98 ? '0.85' : '0';
    svg.style.opacity = '1';
    return true;
  }

  function frame(now) {
    var dt = last ? Math.min(0.05, (now - last) / 1000) : 0.016; last = now;
    var busy = false;
    S.forEach(function (st) {
      var goal = st.hover ? 1 : 0;
      if (goal && st.glow === 0) st.t0 = now;
      st.glow += (goal - st.glow) * (1 - Math.exp(-dt / 0.15));                // time-based, tau 150 ms
      if (!goal && st.glow < 0.003) st.glow = 0;
      drawKey(st, now);
      if (st.glow > 0) busy = true;
    });
    if (drawArm(dt)) busy = true;
    if (busy) requestAnimationFrame(frame); else { running = false; last = 0; }
  }
  function wake() { if (!running) { running = true; requestAnimationFrame(frame); } }
})();
