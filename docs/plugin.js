// HOUZY hybrid panel: the static chrome is baked by the plugin itself (harness `webchrome`, 3x, knobs and
// sliders hidden), every live part is drawn here with the plugin's own formulas, in base 740x500 coordinates.
// Sources: drawRotarySlider / drawLinearSlider / getSliderLayout / drawLabel / drawDigits (PluginEditor.h),
// tubeArc / tubeLine / bubble / edgeLight (Glass.h, kGlassStyle 1 = CSS glass), orbRect / updateOrb (PluginEditor.cpp).
(function () {
  var panel = document.getElementById('panel'), cv = document.getElementById('live');
  if (!panel || !cv) return;
  var ctx = cv.getContext('2d');
  var BASE_W = 740, BASE_H = 500;
  var clamp = function (v, a, b) { return Math.max(a, Math.min(b, v)); };

  // ---- colours (Palette.h) and JUCE colour maths ----
  var CYAN = [141, 202, 212], VALUE = [162, 223, 233], BGBOT = [18, 25, 27], WHITE = [255, 255, 255];
  var brighter = function (c, k) { var a = 1 / (1 + k); return c.map(function (v) { return 255 - (255 - v) * a; }); };
  var darker = function (c, k) { var a = 1 / (1 + k); return c.map(function (v) { return v * a; }); };
  var mix = function (c, d, t) { return c.map(function (v, i) { return v + (d[i] - v) * t; }); };
  var rgba = function (c, a) { return 'rgba(' + Math.round(c[0]) + ',' + Math.round(c[1]) + ',' + Math.round(c[2]) + ',' + a + ')'; };

  // ---- the controls (layout.json from the harness; defaults = the plugin's) ----
  var KNOBS = [
    { id: 'amount', param: 'amount', r: [317, 164, 195, 221], min: 0, max: 100, v: 0, amount: true },
    { id: 'input', param: 'ingain', r: [29, 390, 62, 62], min: -12, max: 24, v: 0 },
    { id: 'output', param: 'outgain', r: [120, 390, 62, 62], min: -24, max: 0, v: 0 },
    { id: 'attack', param: 'atkcycles', r: [222, 229, 76, 76], min: 0.5, max: 4, v: 2.95, digits: 2 },
    { id: 'release', param: 'relms', r: [530, 229, 76, 76], min: 20, max: 1200, v: 234, digits: 0 },
    { id: 'sub', param: 'complow', r: [26, 118, 62, 62], min: 0, max: 100, v: 20 }, { id: 'low', param: 'compbass', r: [124, 118, 62, 62], min: 0, max: 100, v: 20 },
    { id: 'lomid', param: 'complomid', r: [26, 204, 62, 62], min: 0, max: 100, v: 20 }, { id: 'mid', param: 'compmid', r: [124, 204, 62, 62], min: 0, max: 100, v: 20 },
    { id: 'himid', param: 'comphimid', r: [26, 290, 62, 62], min: 0, max: 100, v: 20 }, { id: 'high', param: 'comphigh', r: [124, 290, 62, 62], min: 0, max: 100, v: 20 }
  ];
  var SLIDERS = [
    { id: 'upward', param: 'upcomp', r: [280, 306, 80, 20], min: 0, max: 100, v: 0 },
    { id: 'character', param: 'character', r: [280, 334, 80, 20], min: -1, max: 1, v: -0.4 },
    { id: 'clipshape', param: 'clipshape', r: [280, 362, 80, 20], min: 0, max: 1, v: 0 },
    { id: 'dash', param: 'dash', r: [280, 390, 80, 20], min: 0, max: 100, v: 0 }
  ];
  [[86, 120], [184, 120], [86, 206], [184, 206], [86, 292], [184, 292]].forEach(function (p, i) {
    SLIDERS.push({ id: 'gain' + i, param: ['complowgain', 'compbassgain', 'complomidgain', 'compmidgain', 'comphimidgain', 'comphighgain'][i], r: [p[0], p[1], 14, 58], min: -6, max: 6, v: 0, vertical: true });
  });
  var A0 = Math.PI * 1.2, A1 = Math.PI * 2.8;               // JUCE's default rotary range
  // the plugin draws these in Druk Wide (a paid font, not published): the site uses bold Sora of a similar size
  var DIGIT_FONT = '700 10.5px Sora, system-ui, sans-serif';
  var all = KNOBS.concat(SLIDERS);
  all.forEach(function (c) { c.def = c.v; c.hover = 0; c.lag = (c.v - c.min) / (c.max - c.min); });
  KNOBS.forEach(function (c) {
    var x = c.r[0], y = c.r[1], w = c.r[2], h = c.r[3], tbW = Math.min(64, w), tbH = 16;
    // getSliderLayout: AMOUNT (readoutDrop) — a square (w − tbH) centred in the top w × w; the others — text box below
    c.dial = c.amount ? { x: x + tbH / 2, y: y + tbH / 2, w: w - tbH, h: w - tbH } : { x: x, y: y, w: w, h: h - tbH };
    c.label = { x: x + (w - tbW) / 2, y: y + h - tbH, w: tbW, h: tbH };
    var b = { x: c.dial.x + 3, y: c.dial.y + 3, w: c.dial.w - 6, h: c.dial.h - 6 };   // bounds.reduced (3)
    c.radius = Math.min(b.w, b.h) / 2; c.cx = b.x + b.w / 2; c.cy = b.y + b.h / 2;
    c.thick = Math.max(2.7, c.radius * 0.1424); c.arcR = c.radius - c.thick / 2;
    c.discR = c.arcR - c.thick / 2 - 2;
  });
  var AMT = KNOBS[0];

  // orbRect(): from the dial's unscaled size (rings grew 5 %, kAmountRingScale), expanded 10, minus 4 and 1.6
  (function placeOrb() {
    var d = AMT.r[2] / 1.05 + 20, r1 = d / 2 - 4 - 1.6, half = r1 * 1.14, host = panel.querySelector('.porb');
    host.style.left = ((AMT.cx - half) / BASE_W * 100) + '%'; host.style.top = ((AMT.cy - half) / BASE_H * 100) + '%';
    host.style.width = (half * 2 / BASE_W * 100) + '%'; host.style.height = (half * 2 / BASE_H * 100) + '%';
  })();
  var orb = window.HouzyOrb ? window.HouzyOrb(panel.querySelector('.porb')) : null;
  var sprite = new Image(), SPRITE_N = 144, SPRITE_S = 160, SPRITE_COLS = 12;   // knob faces, the plugin's knob_sprite.png
  sprite.src = '/img/plugin/knobs.webp';

  // ---- drawing helpers ----
  function arcStroke(c, r, from, to, w, style) {
    ctx.beginPath(); ctx.arc(c.cx, c.cy, r, from - Math.PI / 2, to - Math.PI / 2);
    ctx.lineWidth = w; ctx.lineCap = 'round'; ctx.strokeStyle = style; ctx.stroke();
  }
  function lineStroke(p0, p1, w, style) {
    ctx.beginPath(); ctx.moveTo(p0[0], p0[1]); ctx.lineTo(p1[0], p1[1]);
    ctx.lineWidth = w; ctx.lineCap = 'round'; ctx.strokeStyle = style; ctx.stroke();
  }
  function lin(p0, p1, c0, c1) { var g = ctx.createLinearGradient(p0[0], p0[1], p1[0], p1[1]); g.addColorStop(0, c0); g.addColorStop(1, c1); return g; }
  function shape(x, y, w, h, r, ellipse) {
    ctx.beginPath();
    if (ellipse) ctx.ellipse(x + w / 2, y + h / 2, Math.max(0, w / 2), Math.max(0, h / 2), 0, 0, Math.PI * 2);
    else ctx.roundRect(x, y, w, h, Math.max(0, r));
  }
  // glass::edgeLight, kCssGlass branch
  function edgeLight(x, y, w, h, rad, strength, ellipse) {
    var s = Math.min(1, strength / 0.6), side = Math.min(w, h);
    ctx.save(); shape(x, y, w, h, rad, ellipse); ctx.clip();
    var band = clamp(side * 0.18, 1.5, 12);
    for (var k = 0; k < 6; k++) {
      var inn = band * (k + 0.5) / 6;
      shape(x + inn, y + inn, w - 2 * inn, h - 2 * inn, rad - inn, ellipse);
      ctx.lineWidth = band / 6 + 0.3; ctx.strokeStyle = 'rgba(255,255,255,' + (0.2 * 0.42 * Math.exp(-3 * k / 6) * s) + ')'; ctx.stroke();
    }
    var off = ellipse ? Math.min(1.5, side * 0.12) : 1.5;   // the bright top-left edge: the shape minus itself moved down-right
    ctx.beginPath();
    if (ellipse) { ctx.ellipse(x + w / 2, y + h / 2, w / 2, h / 2, 0, 0, Math.PI * 2); ctx.ellipse(x + off + w / 2, y + off + h / 2, w / 2, h / 2, 0, 0, Math.PI * 2); }
    else { ctx.roundRect(x, y, w, h, rad); ctx.roundRect(x + off, y + off, w, h, rad); }
    ctx.fillStyle = 'rgba(255,255,255,' + (0.5 * s) + ')'; ctx.fill('evenodd');
    shape(x + 0.5, y + 0.5, w - 1, h - 1, rad - 0.5, ellipse);
    ctx.lineWidth = 1; ctx.strokeStyle = 'rgba(255,255,255,' + (0.15 * Math.max(0.6, s)) + ')'; ctx.stroke();
    ctx.restore();
  }
  // glass::bubble, CSS branch
  function bubble(x, y, w, h, tint) {
    shape(x, y, w, h, 0, true); ctx.fillStyle = rgba(brighter(tint, 0.25), 0.50); ctx.fill();
    shape(x, y, w, h, 0, true); ctx.fillStyle = 'rgba(255,255,255,0.07)'; ctx.fill();
    edgeLight(x, y, w, h, w / 2, 1, true);
  }
  // glass::tubeArc: the CSS branch (AMOUNT) and the glassValue branch (every other knob)
  function tubeArc(c, r, aV, w, col, ta, glassValue) {
    var a0 = A0, a1 = A1;
    arcStroke(c, r, a0, a1, w, 'rgba(0,0,0,' + 0.30 * ta + ')');
    arcStroke(c, r, a0, a1, w, 'rgba(255,255,255,' + 0.05 * ta + ')');
    arcStroke(c, r + w * 0.5 - 0.4, a0, a1, 0.8, 'rgba(255,255,255,' + 0.26 * ta + ')');
    arcStroke(c, r - w * 0.5 + 0.4, a0, a1, 0.8, 'rgba(255,255,255,' + 0.14 * ta + ')');
    if (aV <= a0 + 0.001) return;
    var pt = function (a, rr) { return [c.cx + rr * Math.sin(a), c.cy - rr * Math.cos(a)]; };
    var p0 = pt(a0, r), pv = pt(aV, r);
    if (glassValue) {
      arcStroke(c, r, a0, aV, w * 1.05, lin(p0, pv, rgba(col, 0.10), rgba(col, 0.34)));
      arcStroke(c, r - w * 0.18, a0, aV, w * 0.30, 'rgba(0,0,0,0.12)');
      arcStroke(c, r + w * 0.24, a0, aV, 1.1, lin(p0, pv, 'rgba(255,255,255,0.25)', 'rgba(255,255,255,0.85)'));
      arcStroke(c, r + w * 0.50, a0, aV, 0.7, 'rgba(255,255,255,0.40)');
      arcStroke(c, r - w * 0.50, a0, aV, 0.7, 'rgba(255,255,255,0.22)');
      arcStroke(c, r - w * 0.32, a0, aV, 1.0, lin(p0, pv, rgba(col, 0.15), rgba(brighter(col, 0.6), 0.75)));
      return;
    }
    arcStroke(c, r, a0, aV, w, lin(p0, pv, rgba(col, 0.40), rgba(brighter(col, 0.2), 0.75)));
    arcStroke(c, r, a0, aV, w, 'rgba(255,255,255,0.07)');
    arcStroke(c, r, a0, aV, w * 0.92, 'rgba(0,0,0,0.22)');
    arcStroke(c, r, a0, aV, w * 0.5, lin(p0, pv, rgba(mix(col, WHITE, 0.35), 0.55), rgba(mix(col, WHITE, 0.55), 0.95)));
    arcStroke(c, r, a0, aV, w * 0.16, 'rgba(255,255,255,0.35)');
    var tl = [c.cx - r, c.cy - r], br = [c.cx + r, c.cy + r];
    arcStroke(c, r + w * 0.5 - 0.6, a0, aV, 1.1, lin(tl, br, 'rgba(255,255,255,0.85)', 'rgba(255,255,255,0.08)'));
    arcStroke(c, r - w * 0.5 + 0.6, a0, aV, 0.8, lin(tl, br, 'rgba(255,255,255,0.10)', 'rgba(255,255,255,0.40)'));
    arcStroke(c, r + w * 0.5 - 0.2, a0, aV, 0.6, 'rgba(255,255,255,0.15)');
    arcStroke(c, r - w * 0.5 + 0.2, a0, aV, 0.6, 'rgba(255,255,255,0.15)');
  }
  // glass::tubeLine (liquid-glass value)
  function tubeLine(p0, p1, pv, w, col) {
    var dx = p1[0] - p0[0], dy = p1[1] - p0[1], len = Math.hypot(dx, dy); if (len < 1) return;
    var n = [-dy / len, dx / len], sh = function (p, k) { return [p[0] + n[0] * k, p[1] + n[1] * k]; };
    lineStroke(p0, p1, w, 'rgba(0,0,0,0.30)'); lineStroke(p0, p1, w, 'rgba(255,255,255,0.05)');
    var e = w * 0.5 - 0.4;
    lineStroke(sh(p0, -e), sh(p1, -e), 0.8, 'rgba(255,255,255,0.24)'); lineStroke(sh(p0, e), sh(p1, e), 0.8, 'rgba(255,255,255,0.14)');
    if (Math.hypot(pv[0] - p0[0], pv[1] - p0[1]) < 0.5) return;
    lineStroke(p0, pv, w * 1.05, lin(p0, pv, rgba(col, 0.12), rgba(col, 0.38)));
    lineStroke(sh(p0, -w * 0.24), sh(pv, -w * 0.24), 1.0, lin(p0, pv, 'rgba(255,255,255,0.25)', 'rgba(255,255,255,0.85)'));
    lineStroke(sh(p0, -w * 0.5), sh(pv, -w * 0.5), 0.7, 'rgba(255,255,255,0.35)');
    lineStroke(sh(p0, w * 0.32), sh(pv, w * 0.32), 1.0, lin(p0, pv, rgba(col, 0.15), rgba(brighter(col, 0.6), 0.75)));
  }
  // drawDigits, kDigitStyle 3 (glass digits)
  function digits(text, box, col) {
    ctx.font = DIGIT_FONT; ctx.textBaseline = 'middle'; ctx.textAlign = 'center';
    var x = box.x + box.w / 2, y = box.y + box.h / 2 + 0.5, topC = mix(col, WHITE, 0.55), botC = darker(col, 0.15);
    ctx.fillStyle = 'rgba(0,0,0,0.40)'; ctx.fillText(text, x, y + 1);
    var g = ctx.createLinearGradient(0, y - 5, 0, y + 5);
    g.addColorStop(0, rgba(mix(topC, WHITE, 0.55), 1)); g.addColorStop(0.5, rgba(mix(topC, botC, 0.5), 1)); g.addColorStop(1, rgba(botC, 1));
    ctx.fillStyle = g; ctx.fillText(text, x, y);
    ctx.lineWidth = 0.5; ctx.strokeStyle = 'rgba(255,255,255,0.28)'; ctx.strokeText(text, x, y);
  }
  var prop = function (c) { return (c.v - c.min) / (c.max - c.min); };

  function drawKnob(c) {
    var pos = prop(c), angle = A0 + pos * (A1 - A0), hov = c.hover, hot = c === active || c === hovered;
    if (!c.amount && c.discR > 4) {        // the face: painted dome under the sprite frame at 50 %, a cyan rim
      var R = c.discR, gx = c.cx - R * 0.45, gy = c.cy - R * 0.5;
      var g = ctx.createRadialGradient(gx, gy, 0, gx, gy, Math.hypot(R * 1.05, R * 1.2));
      g.addColorStop(0, '#2b333d'); g.addColorStop(0.55, '#1e242d'); g.addColorStop(1, '#141920');
      shape(c.cx - R, c.cy - R, R * 2, R * 2, 0, true); ctx.fillStyle = g; ctx.fill();
      if (sprite.complete && sprite.naturalWidth) {
        var i = Math.round(pos * (SPRITE_N - 1));
        ctx.globalAlpha = 0.5;
        ctx.drawImage(sprite, (i % SPRITE_COLS) * SPRITE_S, Math.floor(i / SPRITE_COLS) * SPRITE_S, SPRITE_S, SPRITE_S, c.cx - R, c.cy - R, R * 2, R * 2);
        ctx.globalAlpha = 1;
      }
      shape(c.cx - R + 0.5, c.cy - R + 0.5, R * 2 - 1, R * 2 - 1, 0, true);
      ctx.lineWidth = 1; ctx.strokeStyle = rgba(CYAN, 0.10 + 0.18 * hov); ctx.stroke();
    }
    var col = hot ? brighter(CYAN, 0.12) : CYAN, tubeW = Math.max(3, c.thick * 0.8) + 1.5 * hov;
    if (c.amount && pos > 0.001) arcStroke(c, c.arcR, A0, angle, tubeW, 'rgba(0,0,0,0.55)');     // valueUnderlay
    tubeArc(c, c.arcR, pos > 0.001 ? angle : A0, tubeW, col, c.amount ? 1.0 : 0.7, !c.amount);
    // the tip: a glass bubble that stretches toward where the value lagged; a soft halo on hover
    var side = Math.min(c.dial.w, c.dial.h);
    var trBase = Math.max(2.5, c.thick * 0.5), trRoom = Math.max(trBase, side / 2 - c.arcR - 0.5);
    var tr = Math.min(trBase * (1 + 0.35 * hov), trRoom), d = tr * 2, tip = [c.cx + c.arcR * Math.sin(angle), c.cy - c.arcR * Math.cos(angle)];
    if (hov > 0) {
      var reach = Math.min(tr * 0.9, Math.max(0, side / 2 - c.arcR - tr - 0.5));
      if (reach > 0.5) for (var k = 3; k >= 1; k--) {
        var e = reach * k / 3; shape(tip[0] - tr - e, tip[1] - tr - e, (tr + e) * 2, (tr + e) * 2, 0, true);
        ctx.fillStyle = rgba(CYAN, 0.10 * hov * (1 - 0.20 * k)); ctx.fill();
      }
    }
    var lagA = A0 + c.lag * (A1 - A0), maxDA = d / Math.max(1, c.arcR), dA = clamp(lagA - angle, -maxDA, maxDA), st = Math.abs(dA) / maxDA;
    var len = dA * c.arcR, thin = d * (1 - 0.25 * st);
    ctx.save(); ctx.translate(tip[0], tip[1]); ctx.rotate(angle);
    bubble(len * 0.5 - (d + Math.abs(len)) / 2, -thin / 2, d + Math.abs(len), thin, CYAN);
    ctx.restore();
    var text = c.v.toFixed(c.digits != null ? c.digits : 1);
    if (c.amount) {       // AMOUNT's readout sits on a glass pill sized to the digits
      ctx.font = DIGIT_FONT;
      var pw = Math.min(c.label.w, ctx.measureText(text).width + 16), px = c.label.x + (c.label.w - pw) / 2;
      shape(px, c.label.y, pw, c.label.h, c.label.h / 2, false); ctx.fillStyle = rgba(BGBOT, 0.35); ctx.fill();
      edgeLight(px, c.label.y, pw, c.label.h, c.label.h / 2, 0.5, false);
    }
    digits(text, c.label, VALUE);
  }

  function sliderPos(c, p01) {
    var x = c.r[0], y = c.r[1], w = c.r[2], h = c.r[3], indent = Math.min(12, c.vertical ? w * 0.5 : h * 0.5);   // getSliderThumbRadius (V4)
    return c.vertical ? y + h - indent - p01 * (h - 2 * indent) : x + indent + p01 * (w - 2 * indent);
  }
  function drawSlider(c) {
    var x = c.r[0], y = c.r[1], w = c.r[2], h = c.r[3], hov = c.hover;
    var col = hov > 0 ? brighter(CYAN, 0.18 * hov) : CYAN, tw = 3.5 + hov, inset = tw * 0.5 + 1;
    var tr = Math.max(3.5, (c.vertical ? w : h) * 0.26) * (1 + 0.35 * hov), sp = sliderPos(c, prop(c)), lagP = sliderPos(c, c.lag), bead;
    if (c.off) ctx.globalAlpha = 0.22;
    if (c.vertical) {
      var cx = x + w / 2;
      tubeLine([cx, y + h - inset], [cx, y + inset], [cx, clamp(sp, y + inset, y + h - inset)], tw, col); bead = [cx, sp];
    } else {
      var cy = y + h / 2;
      tubeLine([x + inset, cy], [x + w - inset, cy], [clamp(sp, x + inset, x + w - inset), cy], tw, col); bead = [sp, cy];
    }
    if (hov > 0) {
      var maxR = Math.max(tr + 0.5, (c.vertical ? w : h) * 0.5 - 0.5);
      for (var i = 4; i >= 1; i--) {
        var e = (maxR - tr) * i / 4; shape(bead[0] - tr - e, bead[1] - tr - e, (tr + e) * 2, (tr + e) * 2, 0, true);
        ctx.fillStyle = rgba(CYAN, 0.09 * hov * (1 - 0.18 * i)); ctx.fill();
      }
    }
    // liquid thumb (liquidRect): stretched back toward where it lagged, at most one diameter, thinner across
    var d = tr * 2, off = clamp(lagP - sp, -d, d), bx = bead[0] - tr, by = bead[1] - tr, bw = d, bh = d;
    if (Math.abs(off) >= 0.3) {
      var thin = d * (1 - 0.25 * Math.abs(off) / d);
      if (c.vertical) { bw = thin; bh = d + Math.abs(off); bx = bead[0] - thin / 2; by = bead[1] + off * 0.5 - bh / 2; }
      else { bh = thin; bw = d + Math.abs(off); by = bead[1] - thin / 2; bx = bead[0] + off * 0.5 - bw / 2; }
    }
    bubble(bx, by, bw, bh, CYAN);
    ctx.globalAlpha = 1;
  }

  function draw() {
    var scale = cv.width / BASE_W;
    ctx.setTransform(1, 0, 0, 1, 0, 0); ctx.clearRect(0, 0, cv.width, cv.height);
    ctx.setTransform(scale, 0, 0, scale, 0, 0);
    drawSwitches();
    KNOBS.forEach(drawKnob); SLIDERS.forEach(drawSlider); drawShapingLock();
    drawLive();
  }
  function size() {
    // the LAYOUT size, not getBoundingClientRect: the window lands tilted and scaled (site.js), and a resize during that
    // animation sized the canvas to the tilted box - everything drawn on it then stretched and slid down
    var dpr = Math.min(window.devicePixelRatio || 1, 3);
    var w = Math.round(panel.offsetWidth * dpr), h = Math.round(panel.offsetHeight * dpr);
    if (cv.width === w && cv.height === h) return;   // assigning a size wipes the canvas, even the same size
    cv.width = w; cv.height = h; lastSig = '';      // wiped: the loop must redraw (zoom can keep the width and change only the height)
  }
  window.addEventListener('resize', size); size();

  // ---- interaction: knobs RotaryHorizontalVerticalDrag, 600 px = full range (setMouseDragSensitivity);
  //      sliders jump to the click and follow; wheel 1 % (Shift 5 %); double-click = default ----
  var active = null, hovered = null, last = null;
  function toBase(e) { var r = panel.getBoundingClientRect(); return [(e.clientX - r.left) / r.width * BASE_W, (e.clientY - r.top) / r.height * BASE_H]; }
  function hit(p) {
    for (var i = all.length - 1; i >= 0; i--) {
      var c = all[i], r = c.r;
      if (!c.off && p[0] >= r[0] && p[0] <= r[0] + r[2] && p[1] >= r[1] && p[1] <= r[1] + r[3]) return c;
    }
    return null;
  }
  function setFromSlider(c, p) {
    var x = c.r[0], y = c.r[1], w = c.r[2], h = c.r[3], indent = Math.min(12, c.vertical ? w * 0.5 : h * 0.5);
    var t = c.vertical ? 1 - (p[1] - y - indent) / (h - 2 * indent) : (p[0] - x - indent) / (w - 2 * indent);
    c.v = c.min + clamp(t, 0, 1) * (c.max - c.min);
  }
  var isSlider = function (c) { return SLIDERS.indexOf(c) >= 0; };
  cv.addEventListener('pointermove', function (e) {
    var p = toBase(e);
    if (active && last) {
      if (isSlider(active)) setFromSlider(active, p);
      // AMOUNT as in the plugin (PluginEditor: RotaryVerticalDrag, setMouseDragSensitivity 1200): vertical only, twice the travel
      else if (active === AMT) active.v = clamp(active.v + (last[1] - e.clientY) / 1200 * (active.max - active.min), active.min, active.max);
      else active.v = clamp(active.v + ((e.clientX - last[0]) + (last[1] - e.clientY)) / 600 * (active.max - active.min), active.min, active.max);
      last = [e.clientX, e.clientY];
    } else { hovered = hit(p); cv.style.cursor = hovered || overZone(p) ? 'pointer' : 'default'; }
  });
  cv.addEventListener('pointerdown', function (e) {
    var p = toBase(e);
    if (inR(p, R.relauto) || inR(p, R.truepeak)) { zoneClick(p); return; }   // the AUTO / TP words sit inside their knobs
    var c = hit(p);
    if (!c) { zoneClick(p); return; }
    active = hovered = c; last = [e.clientX, e.clientY]; cv.setPointerCapture(e.pointerId); e.preventDefault();
    if (isSlider(c)) setFromSlider(c, p);
  });
  cv.addEventListener('pointerup', function () { active = null; last = null; });
  cv.addEventListener('pointerleave', function () { if (!active) hovered = null; });
  cv.addEventListener('wheel', function (e) {
    var c = hit(toBase(e)); if (!c) return;
    e.preventDefault();
    c.v = clamp(c.v + (e.deltaY < 0 ? 1 : -1) * (e.shiftKey ? 0.05 : 0.01) * (c.max - c.min), c.min, c.max);
  }, { passive: false });
  cv.addEventListener('dblclick', function (e) { var c = hit(toBase(e)); if (c) c.v = c.def; });

  // ---- live demo (houzy-live.js): every switch the web demo has, the meters and the GR graph ----
  // The chrome is baked by the plugin with every switch at its default (harness `webchrome`), plus a
  // second bake with every switch flipped (chrome_alt) and one at 4x (chrome_os4). A switch away from
  // its default is drawn by pasting its own region from the other bake - the plugin's real look.
  // 2x of the 740 x 500 grid, WebP. The flipped bakes load only when a switch first leaves its default.
  var BAKE = 2, ALT = new Image(), OS4 = new Image(), T8 = new Image(), bakeDir = '/img/plugin/t4/';   // PULIK by default (24.10.2026)
  ALT.file = 'chrome_alt.webp'; OS4.file = 'chrome_os4.webp'; T8.file = 'chrome_t8.webp';
  var BAKE_V = '?v=20261019b';   // the bakes change with the panel (LOOKAHEAD gone 19.10.2026)
  var STATE = { allmix: 0, bypass: 0, gainmatch: 0, clipmode: 2, limmode: 1, relsmart: 1,
                osfactor: 4, autogain: 1, relauto: 1, truepeak: 0 };
  var DEF = JSON.parse(JSON.stringify(STATE));
  // GAIN MATCH starts OFF, as in the plugin (it was on in the demo, to compare at matched loudness; the visitor can switch it on)
  // layout.json (base 740 x 500)
  var R = {
    allmix: [12, 76, 67, 24], bypass: [95, 76, 68, 24], gainmatch: [541, 76, 90, 24],
    engine: [319, 78, 190, 20], clipmode: [508, 334, 100, 20], limmode: [508, 362, 100, 20], relsmart: [508, 390, 100, 20],
    osfactor: [86, 470, 64, 22], autogain: [368, 136, 92, 14], relauto: [551, 278, 34, 12], truepeak: [141, 428, 20, 12],
    bands: [8, 100, 198, 260], presets: [12, 22, 74, 17], themes: [94, 22, 74, 17], ui: [176, 22, 40, 17],
    a: [590, 22, 30, 17], copy: [624, 22, 24, 17], b: [652, 22, 30, 17], lang: [698, 22, 30, 17],
    rms: [630, 108, 44, 378], lufs: [678, 108, 44, 378], graph: [214, 430, 400, 62]
  };
  // CHARACTER (does nothing in HOUZY) and ALL MIX with its band controls are the full version: not draggable,
  // held in the .wasm too. UPWARD / CLIP SHAPE / DASH and T8 are open (owner, 19.10.2026); LOOKAHEAD is always on.
  all.forEach(function (c) { if (c.id === 'character' || /^(sub|low|lomid|mid|himid|high|gain\d)$/.test(c.id)) c.off = true; });
  R.character = [210, 330, 154, 28];
  function drawShapingLock() {      // the baked caption dimmed like the slider beside it
    var g = ctx.createLinearGradient(R.character[0], 0, R.character[0] + 80, 0);
    g.addColorStop(0, 'rgba(18,25,27,0.62)'); g.addColorStop(0.7, 'rgba(18,25,27,0.62)'); g.addColorStop(1, 'rgba(18,25,27,0)');
    ctx.fillStyle = g; ctx.fillRect(R.character[0] + 2, R.character[1], 80, R.character[3]);
  }
  var PATCHES = ['allmix', 'gainmatch', 'clipmode', 'limmode', 'relsmart', 'osfactor', 'autogain', 'relauto', 'truepeak'];
  function paste(img, r, pad) {
    if (!img.getAttribute('src')) img.src = bakeDir + img.file + BAKE_V;
    if (!img.complete || !img.naturalWidth) return;
    var x = r[0] - pad, y = r[1] - pad, w = r[2] + 2 * pad, h = r[3] + 2 * pad;
    ctx.drawImage(img, x * BAKE, y * BAKE, w * BAKE, h * BAKE, x, y, w, h);
  }
  function drawSwitches() {
    if (STATE.allmix !== DEF.allmix) paste(ALT, R.bands, 0);
    PATCHES.forEach(function (k) {
      if (STATE[k] === DEF[k]) return;
      var img = k === 'osfactor' ? (STATE.osfactor === 2 ? OS4 : ALT) : k === 'limmode' && STATE.limmode === 3 ? T8 : ALT;
      paste(img, R[k], 3);
    });
  }

  var T = function (s, v) { return window.houzyT ? window.houzyT(s, v) : s; };   // site.js: the page language
  var say = function (t) { if (window.houzySay) window.houzySay(t); };
  var locked = function (t) { if (window.houzyLocked) window.houzyLocked(t); };
  function setState(k, v) { STATE[k] = v; if (window.houzyParam) window.houzyParam(k, v); }
  function sendAll() { Object.keys(STATE).forEach(function (k) { if (k !== 'bypass') window.houzyParam(k, STATE[k]); }); }
  var bypassOn = false;
  document.addEventListener('houzy-bypass', function (e) { bypassOn = e.detail; STATE.bypass = e.detail ? 1 : 0; });

  // PRESETS (presetTable in PluginEditor.cpp, real units; their 32x becomes the demo's 16x)
  var PRESETS = [
    ['SNOOZE',   { amount: 25, ingain: 0, outgain: 0, character: -0.4, clipshape: 0, dash: 0, atkcycles: 3.2, upcomp: 40, relsmart: 1, autogain: 1, osfactor: 4 }],
    ['SMOOTH',   { amount: 25.2, ingain: 4.5, outgain: 0, character: -0.4, clipshape: 0, dash: 33, atkcycles: 3.2, upcomp: 0, relsmart: 1, autogain: 0, osfactor: 4 }],
    ['SMOOTHIE', { amount: 15.9, ingain: 2, outgain: 0, character: -0.4, clipshape: 0, dash: 33, atkcycles: 3.0, upcomp: 0, relsmart: 1, autogain: 1, osfactor: 4 }],
    ['SUSHI',    { amount: 30, ingain: 3, outgain: 0, character: -0.4, clipshape: 0.54, dash: 97.8, atkcycles: 3.35, upcomp: 32, relsmart: 0, autogain: 1, osfactor: 4 }],
    ['SQUISHY',  { amount: 30, ingain: 3, outgain: 0, character: -0.4, clipshape: 0.43, dash: 82, atkcycles: 3.25, upcomp: 32, relsmart: 0, autogain: 1, osfactor: 4 }]
  ];
  function applySettings(s, keepAuto) {
    all.forEach(function (c) { if (c.param && !c.off && s[c.param] != null) { c.v = s[c.param]; c.sent = c.v; if (window.houzyParam) window.houzyParam(c.param, c.v); } });
    Object.keys(STATE).forEach(function (k) { if (k !== 'bypass' && s[k] != null) setState(k, s[k]); });
    if (!keepAuto) setState('relauto', 1);   // presets keep RELEASE on AUTO, as in the plugin
  }
  function snapshot() {
    var s = {}; all.forEach(function (c) { if (c.param) s[c.param] = c.v; });
    Object.keys(STATE).forEach(function (k) { if (k !== 'bypass') s[k] = STATE[k]; });
    return s;
  }
  // A / B: two slots, like the plugin. Starts on A; "=" copies the current slot into the other one
  var slots = [null, null], slot = 0;
  function useSlot(i) {
    if (i === slot) return;
    slots[slot] = snapshot();
    slot = i;
    if (slots[i]) applySettings(slots[i], true);
    say(T('Slot {s}', { s: i ? 'B' : 'A' }));
  }
  function drawSlot() {      // the baked chrome has A lit; when B is active, light B and dim A
    if (slot === 0) return;
    shape(R.a[0] + 1, R.a[1] + 1, R.a[2] - 2, R.a[3] - 2, 4, false); ctx.fillStyle = 'rgba(30,36,40,0.78)'; ctx.fill();
    shape(R.b[0], R.b[1], R.b[2], R.b[3], 4, false); ctx.fillStyle = rgba(CYAN, 0.16); ctx.fill();
    ctx.lineWidth = 1; ctx.strokeStyle = rgba(CYAN, 0.85); ctx.stroke();
  }

  // PRESETS / THEMES / UI: the same glass list under each button (PresetMenu); picking does not close it
  var menus = [];
  function makeMenu(heading, names, leftPct, onPick, current) {
    var m = document.createElement('div');
    m.className = 'live-menu'; m.hidden = true; m.style.left = leftPct + '%';
    var h = document.createElement('p'); h.className = 'live-menu-head'; h.textContent = heading; m.appendChild(h);
    names.forEach(function (n, i) {
      var bt = document.createElement('button'); bt.type = 'button'; bt.textContent = n;
      if (i === current) bt.classList.add('on');
      bt.addEventListener('click', function () {
        onPick(i);
        m.querySelectorAll('button').forEach(function (x) { x.classList.toggle('on', x === bt); });
      });
      m.appendChild(bt);
    });
    panel.appendChild(m); menus.push(m);
    return m;
  }
  function toggleMenu(m) { var open = m.hidden; menus.forEach(function (x) { x.hidden = true; }); m.hidden = !open; }
  document.addEventListener('pointerdown', function (e) {
    if (e.target === cv) return;
    menus.forEach(function (m) { if (!m.hidden && !m.contains(e.target)) m.hidden = true; });
  });
  var presetMenu = makeMenu('PRESETS', PRESETS.map(function (p) { return p[0]; }), 1.6, function (i) { applySettings(PRESETS[i][1]); }, -1);

  // THEMES: the plugin's five backgrounds, each baked by the plugin (img/plugin/t<id>/), loaded when picked
  var THEMES = [['BLURIK', 0], ['DARKIK', 1], ['MEDIUMIK', 3], ['PULIK', 4], ['UILIK', 5]];
  var chromeImg = panel.querySelector('img');
  function setTheme(i) {
    var dir = '/img/plugin/t' + THEMES[i][1] + '/';
    var next = new Image();
    next.onload = function () {
      chromeImg.src = next.src; bakeDir = dir;
      [ALT, OS4, T8].forEach(function (im) { if (im.getAttribute('src')) im.src = dir + im.file + BAKE_V; });   // reload only what was in use
    };
    next.src = dir + 'chrome.webp' + BAKE_V;
  }
  var themeMenu = makeMenu('THEMES', THEMES.map(function (t) { return t[0]; }), 12.7, setTheme, 3);

  // UI: the four spheres (kOrbColours) and UI OFF, which stops the picture
  var ORBS = [['MINT', 0x33B9B9, 0xFFFFFF, 0xFFFFFF], ['GRAPHITE', 0x5A6B6B, 0xFFFFFF, 0xFFFFFF],
              ['RUBY', 0x622420, 0xA64747, 0xD03232], ['PLUM', 0x431B48, 0x97EAC5, 0xFF1818]];
  var uiOff = false;
  var uiMenu = makeMenu('UI', ORBS.map(function (o) { return o[0]; }).concat(['UI OFF']), 23.8, function (i) {
    uiOff = i === ORBS.length;
    if (!uiOff) setOrb(i);
  }, 1);
  // the sphere's colour is also --orb-tint: the BUY key's tendril (buy.js) takes it
  function setOrb(i) {
    if (orb && orb.setColours) orb.setColours(ORBS[i][1], ORBS[i][2], ORBS[i][3]);
    document.documentElement.style.setProperty('--orb-tint', '#' + ORBS[i][1].toString(16).padStart(6, '0'));
  }
  setOrb(1);   // GRAPHITE by default

  function inR(p, r) { return p[0] >= r[0] && p[0] <= r[0] + r[2] && p[1] >= r[1] && p[1] <= r[1] + r[3]; }
  function third(p, r) { return Math.min(2, Math.floor((p[0] - r[0]) / (r[2] / 3))); }
  function half(p, r) { return p[0] < r[0] + r[2] / 2 ? 0 : 1; }
  // the click sound (site.js) for the switches and buttons drawn on the window; knobs are dragged, not clicked
  var CLICK_ZONES = Object.keys(R).filter(function (k) { return ['bands', 'rms', 'lufs', 'graph'].indexOf(k) < 0; });
  function zoneClick(p) {
    if (window.houzyUiSound && CLICK_ZONES.some(function (k) { return inR(p, R[k]); })) window.houzyUiSound('click');
    if (inR(p, R.presets)) { toggleMenu(presetMenu); return; }
    if (inR(p, R.themes)) { toggleMenu(themeMenu); return; }
    if (inR(p, R.ui)) { toggleMenu(uiMenu); return; }
    menus.forEach(function (m) { m.hidden = true; });
    if (inR(p, R.bypass)) { if (window.houzyBypass) window.houzyBypass(); return; }
    ['gainmatch', 'autogain', 'relauto', 'truepeak'].forEach(function (k) {
      if (inR(p, R[k])) setState(k, STATE[k] ? 0 : 1);
    });
    if (inR(p, R.allmix) || inR(p, R.bands)) { locked('ALL MIX'); return; }
    if (inR(p, R.character)) { locked('CHARACTER'); return; }
    if (inR(p, R.engine)) { if (third(p, R.engine) < 2) locked('CLASSIC and MODERN'); return; }
    if (inR(p, R.clipmode)) setState('clipmode', half(p, R.clipmode) ? 2 : 1);
    if (inR(p, R.limmode)) { var t = third(p, R.limmode); setState('limmode', t === 2 ? 3 : t); }
    if (inR(p, R.relsmart)) setState('relsmart', half(p, R.relsmart));
    if (inR(p, R.osfactor)) {       // 16x -> 4x -> 8x -> 16x; 32x and 64x are the full version
      var next = STATE.osfactor === 4 ? 2 : STATE.osfactor === 2 ? 3 : 4;
      setState('osfactor', next); say(T('Oversampling {n}x · 32x and 64x in the full version', { n: [1, 2, 4, 8, 16][next] }));
    }
    if (inR(p, R.a)) useSlot(0);
    if (inR(p, R.b)) useSlot(1);
    if (inR(p, R.copy)) { slots[1 - slot] = snapshot(); say(T('Copied {from} to {to}', { from: slot ? 'B' : 'A', to: slot ? 'A' : 'B' })); }
    if (inR(p, R.lang)) say(T('English, Russian, Spanish and Portuguese in the plugin'));
  }
  var releaseKnob = KNOBS.filter(function (c) { return c.param === 'relms'; })[0];
  function sendChanged() {
    if (!window.houzyParam) return;
    if (!sendChanged.done) { sendChanged.done = true; sendAll(); }
    all.forEach(function (c) {
      if (!c.param || c.off || c.v === c.sent) return;
      // turning RELEASE takes it off AUTO, as in the plugin (not the first send of the default)
      if (c === releaseKnob && c.sent !== undefined && STATE.relauto) setState('relauto', 0);
      c.sent = c.v; window.houzyParam(c.param, c.v);
      if (c === AMT) document.dispatchEvent(new CustomEvent('houzy-amount', { detail: c.v }));
    });
  }
  window.houzySetKnob = function (param, v) { all.forEach(function (c) { if (c.param === param && !c.off) c.v = clamp(v, c.min, c.max); }); };

  // ---- meters (drawMeter + meterRects) and the GR graph (drawGrGraph), fed by houzy-worklet.js ----
  var AMBER = [224, 162, 74], OVER = [255, 82, 82], CYAN_DIM = [68, 106, 113], TEXT = [217, 232, 235];
  var COMBO_FONT = '700 8.5px Sora, system-ui, sans-serif';
  var meterState = [{ shown: -60, peak: -60, overUntil: 0 }, { shown: -60, peak: -60, overUntil: 0 }], compShown = 0, lastMs = 0;
  function drawMeter(r, st) {
    var x = r[0], y = r[1], w = r[2], h = r[3];
    var trackX = x + 20, trackY = y + 14 + 22 + 3 + 2, trackW = w - 20, trackH = h - 14 - 22 - 3 - 4, cx = trackX + trackW / 2;
    var now = performance.now(), over = now < st.overUntil;
    if (over) {        // OVER lamp left of the caption
      ctx.beginPath(); ctx.arc(cx - 23, y + 7, 3, 0, Math.PI * 2); ctx.fillStyle = rgba(OVER, 1); ctx.fill();
      ctx.beginPath(); ctx.arc(cx - 23, y + 7, 5.5, 0, Math.PI * 2); ctx.fillStyle = rgba(OVER, 0.35); ctx.fill();
    }
    var saved = DIGIT_FONT; DIGIT_FONT = COMBO_FONT;
    digits(st.shown.toFixed(1), { x: cx - 18, y: y + 14 + 2, w: 36, h: 18 }, VALUE);
    DIGIT_FONT = saved;
    var dbToY = function (db) { return trackY + trackH - (clamp(db, -60, 6) + 60) / 66 * trackH; };
    var yVal = dbToY(st.shown), bottom = trackY + trackH;
    if (yVal < bottom - 1) {
      var fx = trackX + 2, fw = trackW - 4, fh = bottom - yVal - 2, at = function (db) { return clamp((bottom - dbToY(db)) / trackH, 0, 1); };
      var g = ctx.createLinearGradient(0, bottom, 0, trackY);
      g.addColorStop(0, rgba(CYAN_DIM, 0.85)); g.addColorStop(at(-30), rgba(CYAN, 0.80)); g.addColorStop(at(-9), rgba(brighter(CYAN, 0.2), 0.85));
      g.addColorStop(at(-6), rgba(AMBER, 0.90)); g.addColorStop(at(0), rgba(OVER, 0.95)); g.addColorStop(1, rgba(OVER, 0.95));
      shape(fx, yVal, fw, fh, fw / 2, false); ctx.fillStyle = g; ctx.fill();
      var gl = ctx.createLinearGradient(fx, 0, fx + fw, 0); gl.addColorStop(0, 'rgba(255,255,255,0.28)'); gl.addColorStop(1, 'rgba(0,0,0,0.18)');
      ctx.fillStyle = gl; ctx.fill();
      shape(fx + 1, yVal, fw - 2, 1.4, 0.7, false); ctx.fillStyle = 'rgba(255,255,255,0.75)'; ctx.fill();
    }
    ctx.fillStyle = over ? rgba(OVER, 1) : 'rgba(255,255,255,0.9)';
    ctx.fillRect(trackX + 1.5, dbToY(st.peak) - 0.5, trackW - 3, 1.5);
  }
  function drawGraph(m) {
    var r = R.graph, ax = r[0], ay = r[1], aw = r[2], ah = r[3], n = 180, fullDb = 9, split = n / 2, wp = m[3] | 0, i;
    var TOT = 4, CMP = 4 + n, CLP = 4 + 2 * n, OUT = 4 + 3 * n, IN = 4 + 4 * n;
    var X = function (i) { return ax + i / (n - 1) * aw; }, Y = function (gr) { return ay + clamp(-gr / fullDb, 0, 1) * ah; };
    var at = function (base, i) { return m[base + (wp + i) % n]; };
    var curve = function (base) { ctx.beginPath(); for (var i = split; i < n; i++) ctx[i === split ? 'moveTo' : 'lineTo'](X(i), Y(at(base, i))); };
    ctx.save(); ctx.beginPath(); ctx.rect(ax + 1, ay + 1, aw - 2, ah - 2); ctx.clip();
    // compressor filled; the band down to the total is the limiter
    curve(CMP); ctx.lineTo(ax + aw, ay); ctx.lineTo(X(split), ay); ctx.closePath(); ctx.fillStyle = rgba(CYAN, 0.18); ctx.fill();
    curve(CMP); ctx.lineWidth = 1.5; ctx.strokeStyle = rgba(CYAN, 1); ctx.stroke();
    curve(TOT); for (i = n - 1; i >= split; i--) ctx.lineTo(X(i), Y(at(CMP, i)));
    ctx.closePath(); ctx.fillStyle = rgba(CYAN, 0.10); ctx.fill();
    curve(TOT); ctx.lineWidth = 1; ctx.strokeStyle = rgba(CYAN, 0.45); ctx.stroke();
    // the wave as mirrored bars: processed on the left, the dry input on the right, lighter
    var mid = ay + ah / 2, hh = ah * 0.40, bw = Math.max(1.2, aw / (n - 1) * 2 * 0.62);
    function bars(from, to, base, weight) {
      var ups = new Path2D(), downs = new Path2D();
      for (var i = from; i < to; i++) {
        if (((wp + i) % n) % 2 !== 0) continue;
        var v = Math.max(clamp(at(base, i), 0, 1), i + 1 < to ? clamp(at(base, i + 1), 0, 1) : 0);
        if (v <= 0.004) continue;
        var x = X(i + 0.5) - bw / 2, up = Math.max(bw / 2, v * hh * 0.85);
        ups.roundRect(x, mid - up, bw, up, bw / 2); downs.roundRect(x, mid + 0.8, bw, up, bw / 2);
      }
      var g = ctx.createLinearGradient(0, mid, 0, mid - hh * 0.85);
      g.addColorStop(0, rgba(AMBER, 0.70 * weight)); g.addColorStop(1, rgba(brighter(AMBER, 0.6), 0.95 * weight));
      ctx.fillStyle = g; ctx.fill(ups);
      g = ctx.createLinearGradient(0, mid, 0, mid + hh * 0.85);
      g.addColorStop(0, rgba(AMBER, 0.38 * weight)); g.addColorStop(1, rgba(AMBER, 0.04 * weight));
      ctx.fillStyle = g; ctx.fill(downs);
    }
    bars(0, split + 1, OUT, 1); bars(split, n, IN, 0.45);
    ctx.fillStyle = rgba(TEXT, 0.30); ctx.fillRect(Math.round(X(split)), ay + 2, 1, ah - 4);
    for (i = 0; i < n; i++) {          // clipper activity along the top edge
      var act = clamp(at(CLP, i), 0, 1); if (act <= 0.002) continue;
      ctx.fillStyle = rgba(AMBER, 0.30 + 0.60 * act); ctx.fillRect(X(i), ay + 1.5, aw / n + 0.5, 1.5 + 2.5 * act);
    }
    var deepest = 0; for (i = 0; i < n; i++) deepest = Math.max(deepest, -at(CMP, i));
    if (deepest > 0.25) {              // dashed peak hold of the compressor
      var py = Y(-deepest); ctx.fillStyle = rgba(CYAN, 0.35);
      for (var px = ax; px < ax + aw; px += 6) ctx.fillRect(px, py - 0.5, Math.min(3, ax + aw - px), 1);
    }
    ctx.restore();
    var text = 'COMP ' + compShown.toFixed(1);   // on its glass capsule (kDigitStyle 3)
    ctx.font = DIGIT_FONT; var tw = ctx.measureText(text).width + 12, bx = ax + 6 + 16, by = ay + 3 + 2;
    shape(bx, by, tw, 14, 7, false); ctx.fillStyle = rgba(BGBOT, 0.35); ctx.fill(); edgeLight(bx, by, tw, 14, 7, 0.5, false);
    digits(text, { x: bx, y: by, w: tw, h: 14 }, VALUE);
  }
  var EMPTY = new Float32Array(904);
  function drawLive() {
    var m = window.houzyMeters, now = performance.now(), dt = lastMs ? Math.min(100, now - lastMs) : 16; lastMs = now;
    var k = 1 - Math.exp(-dt / 205);     // meterui::approach, tau 205 ms; peak falls 4.5 dB/s; OVER held 1 s
    [m ? m[0] : -100, m ? m[1] : -100].forEach(function (v, i) {
      var st = meterState[i]; st.shown += (clamp(v, -60, 6) - st.shown) * k;
      st.peak = st.shown > st.peak ? st.shown : Math.max(-60, st.peak - 4.5 * dt / 1000);
      if (st.shown > -0.3) st.overUntil = now + 1000;
    });
    compShown = compShown * 0.80 + (m ? m[2] : 0) * 0.20;
    drawMeter(R.rms, meterState[0]); drawMeter(R.lufs, meterState[1]);
    drawGraph(m || EMPTY);
    drawSlot();
    if (bypassOn) {      // BYPASS: lit box and the processing zones veiled, as in the plugin
      shape(96, 81, 14, 14, 3, false); ctx.fillStyle = rgba(CYAN, 0.55); ctx.fill();
      ctx.shadowColor = rgba(CYAN, 0.9); ctx.shadowBlur = 8; ctx.fill(); ctx.shadowBlur = 0;
      ctx.fillStyle = 'rgba(10,14,16,0.45)';
      [R.bands, [214, 102, 400, 322], R.graph].forEach(function (z) { shape(z[0], z[1], z[2], z[3], 6, false); ctx.fill(); });
      ctx.font = '700 26px Sora, system-ui, sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillStyle = 'rgba(217,232,235,0.85)'; ctx.fillText('BYPASS', 414, 270);
    }
  }

  // ---- one loop: hover / liquid tips by elapsed time (τ 158 ms / 67 ms, WheelSlider); the sphere follows AMOUNT (updateOrb) ----
  var t0 = 0, prevAmt = prop(AMT), vYaw = 0, yaw = 0, onScreen = true, lastSig = '', lastMeters = null;
  if ('IntersectionObserver' in window) new IntersectionObserver(function (e) { onScreen = e[0].isIntersecting; }).observe(panel);
  // canvas text is drawn with whatever font exists at that moment: the first frame came out in the default serif until a hover
  // redrew it. Ask for the font explicitly and draw again when it arrives (and once everything on the page has loaded).
  if (document.fonts) {
    var again = function () { lastSig = ''; };
    document.fonts.load('700 10px Sora').then(again, again);
    document.fonts.ready.then(again);
  }
  (function loop(now) {
    if (!onScreen || document.hidden) { t0 = 0; sendChanged(); requestAnimationFrame(loop); return; }   // asleep off screen
    var dt = t0 ? Math.min(0.05, (now - t0) / 1000) : 0.016; t0 = now;
    all.forEach(function (c) {
      c.hover += ((c === hovered || c === active ? 1 : 0) - c.hover) * (1 - Math.exp(-dt / 0.158));
      c.lag += (prop(c) - c.lag) * (1 - Math.exp(-dt / 0.067));
    });
    var pos = prop(AMT);
    vYaw = clamp(vYaw + (pos - prevAmt) * 40, -12, 12); prevAmt = pos;
    yaw += vYaw * dt; vYaw *= Math.exp(-dt * 2.2);
    if (orb && orb.st && uiOff) { orb.st.speed = 0; vYaw = 0; }
    else if (orb && orb.st) { orb.st.yaw = yaw; orb.st.pitch = 0; orb.st.spin = 0; orb.st.amp = 1.6 * (0.70 + 0.50 * pos); orb.st.speed = 0.8 + 0.8 * pos; }
    // draw only when something moved: a value, hover or liquid tip still settling, new meter data or meters still
    // easing, a switch, the bypass veil, a bake that just loaded, a resize. At rest the panel costs nothing.
    var m = window.houzyMeters, sig = all.map(function (c) { return c.v; }).join() + JSON.stringify(STATE) + bypassOn + slot + uiOff +
      chromeImg.src + (ALT.complete && ALT.naturalWidth) + (OS4.complete && OS4.naturalWidth) + cv.width;
    var settling = all.some(function (c) {
      return Math.abs(c.hover - (c === hovered || c === active ? 1 : 0)) > 0.002 || Math.abs(c.lag - prop(c)) > 0.0005;
    }) || meterState.some(function (st, i) {
      var target = m ? clamp(m[i], -60, 6) : -60;
      return Math.abs(st.shown - target) > 0.05 || st.peak > st.shown + 0.05 || performance.now() < st.overUntil + 50;
    }) || Math.abs(compShown - (m ? m[2] : 0)) > 0.02;
    if (sig !== lastSig || m !== lastMeters || settling) { draw(); lastSig = sig; lastMeters = m; } else lastMs = 0;
    sendChanged();
    requestAnimationFrame(loop);
  })(0);
})();
