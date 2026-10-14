// HOUZY 5 site. Libraries: framer-motion dom-mini (window.Motion). The sphere is a WebGL2 port of the plugin's
// LiquidOrb::render (PluginEditor.h), GRAPHITE colours from kOrbColours (PluginEditor.cpp).
// ponytail: motion runs for everyone, including the OS "reduce motion" setting (owner's decision, 12.10.2026).
(function () {
  var html = document.documentElement;
  var M = window.Motion && window.Motion.animate ? window.Motion : null;
  var clamp = function (v, a, b) { return Math.max(a, Math.min(b, v)); };

  /* ---------- text the scripts write themselves (toasts, the player, the terms card) is looked up here ----------
     English phrase -> the page language. A translated page carries its strings in <script id="i18n-js">; the switcher
     (i18n.js) swaps them with houzyI18nSet. {name}-style holes are filled by the caller. */
  var I18N = {};
  try { I18N = JSON.parse(document.getElementById('i18n-js').textContent) || {}; } catch (e) {}
  window.houzyI18nSet = function (d) { I18N = d || {}; };
  window.houzyT = function (s, v) {
    var r = I18N[s] || s;
    return v ? r.replace(/\{(\w+)\}/g, function (m, k) { return v[k] != null ? v[k] : m; }) : r;
  };
  var T = window.houzyT;

  /* ---------- UI sounds: audio/click.wav on a click or tap of anything clickable, audio/grab.wav when a tendril grabs a card.
     Silent while a track plays in the HOUZY demo (houzy-live.js sets window.houzyPlaying), back on pause or when it is cleared.
     Browsers let a page make sound only after a click / tap / key (hover and scroll do not count), so grabs before that are
     silent. The first such gesture plays the grab muted INSIDE it: Safari on iPhone unlocks an audio element only that way. */
  // Web Audio, not <audio> elements: the three files are fetched and decoded at once, so a sound starts the instant it is asked
  // for; ONE gesture (pointer down, key, tap) resumes the context for every sound, on iPhone too. Nothing is ever scheduled
  // while the context is suspended - a sound asked for before the first gesture is dropped, never played late as a burst.
  var AC = window.AudioContext || window.webkitAudioContext, actx = null, bufs = {}, lastSnd = {};
  var SND = { click: ['/audio/click.wav', 1],     // the click peaks at -17 dBFS
              grab:  ['/audio/grab.wav', 0.5],    // peaks at -1 dBFS: half is plenty for a UI sound
              buy:   ['/audio/buy.mp3', 0.354] };  // "bye.wav", on hovering a Buy key (buy.js): 50% quieter, then -3 dB more (owner, 13-14.10)
  function ctxOf() {
    if (actx || !AC) return actx;
    try { actx = new AC({ latencyHint: 'interactive' }); } catch (e) { AC = null; return null; }
    Object.keys(SND).forEach(function (k) {
      fetch(SND[k][0]).then(function (r) { return r.arrayBuffer(); }).then(function (ab) {
        return new Promise(function (ok, fail) { actx.decodeAudioData(ab, ok, fail); });   // callback form: old Safari has no promise
      }).then(function (b) { bufs[k] = b; }).catch(function () {});
    });
    return actx;
  }
  var GESTURES = ['pointerdown', 'mousedown', 'keydown', 'touchstart', 'touchend', 'click'];
  function unlock() {
    var c = ctxOf(); if (!c) return;
    if (c.state !== 'running') {
      c.resume().catch(function () {});
      try { var s = c.createBufferSource(); s.buffer = c.createBuffer(1, 1, 22050); s.connect(c.destination); s.start(0); } catch (e) {}  // iPhone
    }
    if (c.state === 'running') GESTURES.forEach(function (t) { document.removeEventListener(t, unlock, true); });
  }
  GESTURES.forEach(function (t) { document.addEventListener(t, unlock, true); });
  ctxOf();                              // fetch + decode now (a suspended context may already do that)
  function uiSound(k) {
    var now = performance.now();
    // quiet while the demo track is audible; when it goes quiet (the gap before it loops, a break, its end) they are back
    var quietTrack = window.houzyMeters && window.houzyMeters[0] < -45;
    if ((window.houzyPlaying && !quietTrack) || now - (lastSnd[k] || 0) < 120) return;      // a fast run through chapters: no stutter
    var c = ctxOf(), b = bufs[k];
    if (!c || !b) return;
    var start = function () {
      if (c.state !== 'running' || performance.now() - now > 300) return;                   // too late to still mean anything
      lastSnd[k] = performance.now();
      var s = c.createBufferSource(), g = c.createGain();
      s.buffer = b; g.gain.value = SND[k][1]; s.connect(g); g.connect(c.destination); s.start(0);
    };
    if (c.state === 'running') start();
    else c.resume().then(start, function () {});       // inside a click this runs a few ms later; with no gesture it never does
  }
  window.houzyUiSound = uiSound;        // plugin.js and buy.js: the switches drawn on the plugin window, the Buy keys
  // the click is played inside the click itself
  document.addEventListener('click', function (e) {
    if (e.target.closest && !e.target.closest('.lang') && e.target.closest('a, button, label, summary, [role="button"], .tab')) uiSound('click');   // the language switch is silent (owner, 14.10)
  }, true);

  /* ---------- user agreement: asked once, before anything else ----------
     The first visit shows it as a card in the corner; the site stays usable, the card stays until "I agree".
     Not shown on the legal pages themselves. */
  var TERMS = 'houzy-terms', TERMS_V = '1';     // raise TERMS_V when the agreement changes: everyone is asked again
  var legal = /(terms|eula|privacy|cookies).html$/.test(location.pathname);
  function termsOk() { try { return localStorage.getItem(TERMS) === TERMS_V; } catch (e) { return true; } }
  if (!termsOk() && !legal) {
    var gate = document.createElement('div');
    gate.className = 'terms-gate';
    gate.innerHTML = '<div class="terms-card glass" role="dialog" aria-labelledby="terms-h">' +
      '<p class="kicker">HOUZY 5</p><h2 id="terms-h">' + T('Before you continue') + '</h2>' +
      '<p>' + T('By using this site and the HOUZY demo you agree to the {terms} and the {eula}. How we handle data is in the {privacy}.', {
        terms: '<a href="/terms.html">' + T('Terms of Use & Sale') + '</a>',
        eula: '<a href="/eula.html">' + T('Licence Agreement (EULA)') + '</a>',
        privacy: '<a href="/privacy.html">' + T('Privacy Policy') + '</a>' }) + '</p>' +
      '<button class="cta" type="button">' + T('I agree') + '</button></div>';
    document.body.appendChild(gate);
    var agree = gate.querySelector('button');
    agree.addEventListener('click', function () {
      try { localStorage.setItem(TERMS, TERMS_V); } catch (e) {}
      gate.classList.add('out'); setTimeout(function () { gate.remove(); }, 300);
    });
  }
  document.addEventListener('click', function (e) {
    if (e.target.closest('.navpanel a')) { var n = document.getElementById('nav-open'); if (n) n.checked = false; }
    var b = e.target.closest('.btn,.pill,.cta');       // glass press flash
    if (b && e.detail) {
      var r = b.getBoundingClientRect(), f = document.createElement('span');
      f.className = 'flash'; f.style.setProperty('--fx', (e.clientX - r.left) + 'px'); f.style.setProperty('--fy', (e.clientY - r.top) + 'px');
      b.appendChild(f); setTimeout(function () { f.remove(); }, 500);
    }
  });

  /* ---------- background video: endless forward motion (two copies crossfade at the seam) ---------- */
  (function () {
    var vids = [].slice.call(document.querySelectorAll('.bg video'));
    if (!vids.length) return;
    // the background is served from the site itself: a light 640 px cut on phones, 1280 px elsewhere (both copies share one file)
    var src = '/video/bg-' + (Math.min(screen.width, innerWidth) < 760 ? 640 : 1280) + '.mp4';
    vids.forEach(function (v) { v.src = src; });
    var p0 = vids[0].play(); if (p0) p0.catch(function () {});
    if (vids.length < 2) { vids[0].loop = true; return; }
    var FADE = 1.5, a = vids[0], b = vids[1], busy = false;
    a.style.zIndex = 1; b.style.zIndex = 0;
    (function tick() {
      if (!busy && a.duration && a.currentTime >= a.duration - FADE) {
        busy = true;
        b.currentTime = 0; b.style.zIndex = 2; b.style.opacity = 0;
        var p = b.play(); if (p) p.catch(function () {});
        var done = function () { a.pause(); a.style.zIndex = 0; b.style.zIndex = 1; var t = a; a = b; b = t; busy = false; };
        if (M) M.animate(b, { opacity: [0, 1] }, { duration: FADE, ease: 'linear' }).then(function () { b.style.opacity = 1; done(); });
        else { b.style.opacity = 1; setTimeout(done, FADE * 1000); }
      }
      requestAnimationFrame(tick);
    })();
  })();

  /* ---------- the sphere: orb.js (LiquidOrb port) ---------- */
  // created when the stage comes near (WebGL setup is the costliest thing at load); plugin.html runs its own
  var orb = null;
  (function () {
    var host = document.getElementById('orb'), st = document.querySelector('.stage');
    if (!window.HouzyOrb || !st || !host) return;
    var make = function () { if (!orb) orb = window.HouzyOrb(host); };
    if (!('IntersectionObserver' in window)) return make();
    var io = new IntersectionObserver(function (e) { if (e[0].isIntersecting) { make(); io.disconnect(); } }, { rootMargin: '600px 0px' });
    io.observe(st);
  })();

  /* ---------- the stage: tabs left / right of the sphere, the chapter opens in the reading panel on top ----------
     Point at a tab or at the sphere on that side (hover intent 110 ms), or click / tap / arrow keys / scroll.
     ui-ux-pro-max: hover never the only way in; spring motion (critically damped, time based); swaps cancel cleanly. */
  var stage = document.querySelector('.stage'), pin = document.querySelector('.pin');
  var tabs = [].slice.call(document.querySelectorAll('.tab'));
  var panels = {}; [].slice.call(document.querySelectorAll('.panel')).forEach(function (p) { panels[p.getAttribute('data-ch')] = p; });
  var N = tabs.length, current = -1;
  var ids = tabs.map(function (t) { return t.getAttribute('data-ch'); });
  var angles = tabs.map(function (t) { return +t.getAttribute('data-angle'); });
  var wrap = document.getElementById('orb-wrap'), ring = wrap && wrap.querySelector('.orb-ring');
  // the stage is pinned (sticky) above 900 px - site.css; a media query instead of asking for computed styles every frame
  var pinMq = window.matchMedia('(min-width: 901px)');
  var isPinned = function () { return !!pin && pinMq.matches; };
  var stageRange = function () { return stage.offsetHeight - window.innerHeight; };
  var adiff = function (a, b) { return ((a - b) % 360 + 540) % 360 - 180; };       // shortest signed angle a − b
  function nearest(a) {
    var best = 0, bd = 999;
    angles.forEach(function (x, i) { var d = Math.abs(adiff(a, x)); if (d < bd) { bd = d; best = i; } });
    return best;
  }
  var unclear = function (el) { el.style.opacity = ''; el.style.transform = ''; };
  function setActive(i, quiet) {
    if (i === current || i < 0) return;
    var old = current >= 0 ? panels[ids[current]] : null, next = panels[ids[i]];
    current = i;
    tabs.forEach(function (t, k) { t.setAttribute('aria-current', k === i ? 'true' : 'false'); });
    Object.keys(panels).forEach(function (k) { if (panels[k] !== next && panels[k] !== old) panels[k].classList.remove('is-active', 'is-leaving'); });
    if (old && old !== next) {
      old.classList.remove('is-active'); old.classList.add('is-leaving');
      if (M) M.animate(old, { opacity: [1, 0], transform: ['none', 'translateY(-8px)'] }, { duration: 0.16, ease: [0.4, 0, 1, 1] })
        .then(function () { if (!old.classList.contains('is-active')) old.classList.remove('is-leaving'); unclear(old); });
      else old.classList.remove('is-leaving');
    }
    next.classList.remove('is-leaving'); next.classList.add('is-active'); next.scrollTop = 0;
    if (M && old) M.animate(next, { opacity: [0, 1], transform: ['translateY(12px)', 'none'] }, { duration: 0.42, delay: 0.06, ease: [0.22, 1, 0.36, 1] })
      .then(function () { unclear(next); });
    if (!quiet) history.replaceState(null, '', '#' + ids[i]);
  }

  if (stage && N && wrap) {
    var pend = null, hovering = false, pointerA = 0;
    // a chosen chapter interrupts a coasting sphere: it turns straight to it
    function pick(i) { mode = 'spring'; setActive(i); }
    function intent(i) { clearTimeout(pend); pend = setTimeout(function () { pick(i); }, 110); }
    tabs.forEach(function (t, k) {
      t.addEventListener('mouseenter', function () { intent(k); });
      t.addEventListener('mouseleave', function () { clearTimeout(pend); });
      t.addEventListener('click', function () {
        clearTimeout(pend); pick(k);
        // phones: the reading card is on top; bring it into view when a chapter below it is tapped
        if (!isPinned() && reader) {
          var rr = reader.getBoundingClientRect();
          if (rr.top < 80 || rr.bottom > window.innerHeight) window.scrollTo({ top: window.scrollY + rr.top - 84, behavior: 'smooth' });
        }
      });
      t.addEventListener('focus', function () { pick(k); });
    });
    // pointing at the RING round the sphere (between 1 and 1.7 radii): the marker follows the pointer, that side opens.
    // Inside the sphere the pointer grabs it instead (below).
    pin.addEventListener('pointermove', function (e) {
      if (drag) return;
      var r = wrap.getBoundingClientRect(), d = Math.hypot(e.clientX - (r.left + r.width / 2), e.clientY - (r.top + r.height / 2));
      hovering = d > r.width * 0.5 && d < r.width * 0.85;
      if (!hovering) return;
      pointerA = (Math.atan2(e.clientX - (r.left + r.width / 2), -(e.clientY - (r.top + r.height / 2))) * 180 / Math.PI + 360) % 360;
      var i = nearest(pointerA); if (i !== current) intent(i);
    });
    pin.addEventListener('pointerleave', function () { hovering = false; });

    // GRAB AND SPIN: drag turns the sphere under the pointer (x → yaw, y → tilt); let go and it coasts,
    // then a spring settles it on the nearest chapter, which opens. The chapter follows the spin meanwhile.
    var drag = null, mode = 'spring', tilt = 0;          // mode: 'drag' | 'coast' | 'spring'
    wrap.addEventListener('pointerdown', function (e) {
      var r = wrap.getBoundingClientRect();
      if (Math.hypot(e.clientX - (r.left + r.width / 2), e.clientY - (r.top + r.height / 2)) > r.width * 0.55) return;
      e.preventDefault();
      drag = { x: e.clientX, y: e.clientY, t: performance.now() }; mode = 'drag'; hovering = false;
      wrap.setPointerCapture(e.pointerId); wrap.classList.add('dragging');
    });
    wrap.addEventListener('pointermove', function (e) {
      if (!drag) return;
      var now = performance.now(), dts = Math.max(0.008, (now - drag.t) / 1000);
      var dYaw = (e.clientX - drag.x) * 0.45;                  // degrees per pixel
      yaw += dYaw; yV = yV * 0.6 + (dYaw / dts) * 0.4;            // smoothed velocity for the throw
      tilt = clamp(tilt + (e.clientY - drag.y) * 0.004, -0.7, 0.7);
      drag.x = e.clientX; drag.y = e.clientY; drag.t = now;
      var i = nearest(((yaw % 360) + 360) % 360); if (i !== current) setActive(i);
    });
    function letGo() {
      if (!drag) return;
      if (performance.now() - drag.t > 120) yV = 0;              // held still before letting go: no throw, just settle
      drag = null; mode = 'coast'; wrap.classList.remove('dragging');
    }
    wrap.addEventListener('pointerup', letGo); wrap.addEventListener('pointercancel', letGo);
    // scrolling through the pinned stage walks the chapters in order
    // only when the scroll crosses into another chapter's stretch: a hover pick is never undone by a small scroll
    var lastIdx = -1;
    window.addEventListener('scroll', function () {
      if (!isPinned()) return;
      var range = stageRange(), y = window.scrollY - stage.offsetTop;
      if (range <= 0 || y < 0 || y > range + 2) { lastIdx = -1; return; }
      var i = Math.round(y / range * (N - 1));
      if (i !== lastIdx) { if (lastIdx >= 0) pick(i); lastIdx = i; }
    }, { passive: true });
    document.addEventListener('keydown', function (e) {
      if (/INPUT|TEXTAREA|SELECT/.test(document.activeElement.tagName)) return;
      var r = stage.getBoundingClientRect();
      if (r.bottom < window.innerHeight * 0.5 || r.top > window.innerHeight * 0.5) return;
      var step = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? -1 : 0;
      if (!step) return;
      e.preventDefault(); pick((current + step + N) % N);
    });

    // ARMS: a tendril of the sphere's own liquid graphite reaches out of it to the edge of the open chapter's card
    // and grabs it with two curled fingers; when the chapter changes (click, hover, drag, scroll) it lets go and is
    // drawn back into the sphere while a new one stretches out to the next card. A thinner one holds the reading panel.
    // The material copies the GRAPHITE sphere: dark tinted body, a white specular along the lit (upper) side, a dark
    // rim, ripples running along it (the width and the line breathe like the sphere's surface) and glints that travel.
    // Drawn as filled outlines of varying width in one SVG under the sphere (so it comes OUT of it), by the same
    // time-based loop; off on phones.
    var SVGNS = 'http://www.w3.org/2000/svg';
    var arms = document.createElementNS(SVGNS, 'svg'); arms.setAttribute('class', 'arms'); arms.setAttribute('aria-hidden', 'true');
    arms.innerHTML = '<defs><linearGradient id="armBody" gradientUnits="userSpaceOnUse">' +
      '<stop offset="0" stop-color="#26302f"/><stop offset=".35" stop-color="#5a6b6b"/><stop offset=".8" stop-color="#3b4747"/>' +
      '<stop offset="1" stop-color="#1c2323"/></linearGradient></defs>' +
      '<path class="arm-ghost"/><path class="arm-up-body"/><path class="arm-up-hl"/>' +
      '<path class="arm-body"/><path class="arm-hl"/><path class="arm-hl2"/><path class="arm-glint"/>' +
      '<path class="arm-finger"/><path class="arm-finger"/>' +
      '<path class="arm-finger arm-up-finger"/><path class="arm-finger arm-up-finger"/>';
    pin.appendChild(arms);
    var Q = function (c) { return arms.querySelector(c); };
    var bodyEl = Q('.arm-body'), hlEl = Q('.arm-hl'), hl2El = Q('.arm-hl2'), glintEl = Q('.arm-glint'), ghostEl = Q('.arm-ghost'),
        upBody = Q('.arm-up-body'), upHl = Q('.arm-up-hl'), fingers = arms.querySelectorAll('.arm-finger'),
        gradEl = Q('#armBody'), reader = pin.querySelector('.reader'), upFingers = arms.querySelectorAll('.arm-up-finger');
    var armFor = -1, reach = 0, reachV = 0, ghost = 0, flow = 0, grabbed = false, upPulse = 0, grip = 0, gripV = 0, lastGhost = '';
    var f1 = function (v) { return v.toFixed(1); };
    function bez(P, t) {
      var u = 1 - t, a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
      return [a * P[0][0] + b * P[1][0] + c * P[2][0] + d * P[3][0], a * P[0][1] + b * P[1][1] + c * P[2][1] + d * P[3][1]];
    }
    // a liquid tube along the bezier P from t=0 to t=upto: width w0 → w1, rippling; returns outline, centre and lit-side lines
    function tube(P, upto, w0, w1, phase) {
      var n = 44, L = [], R = [], C = [], H = [], t, p, nx, ny, w, rip, k;
      for (k = 0; k <= n; k++) {
        t = upto * k / n; p = bez(P, t);
        var q = bez(P, Math.min(1, t + 0.01)), dx = q[0] - p[0], dy = q[1] - p[1], dl = Math.hypot(dx, dy) || 1;
        nx = -dy / dl; ny = dx / dl; if (ny > 0) { nx = -nx; ny = -ny; }   // the normal that points up = the lit side
        rip = Math.sin(t * 22 - phase * 5.5) * 0.12 + Math.sin(t * 9 + phase * 3.1) * 0.06;
        w = (w0 + (w1 - w0) * Math.pow(t, 0.7)) * (1 + rip);
        var wob = Math.sin(t * 13 - phase * 4.2) * 1.4 * (1 - t * 0.6);      // the line itself breathes a little
        var cx = p[0] + nx * wob, cy = p[1] + ny * wob;
        L.push([cx + nx * w / 2, cy + ny * w / 2]); R.push([cx - nx * w / 2, cy - ny * w / 2]);
        C.push([cx, cy]); H.push([cx + nx * w * 0.24, cy + ny * w * 0.24, w]);
      }
      // the outline, with a rounded drop at the tip
      var tip = C[n], tw = H[n][2], dxT = C[n][0] - C[n - 1][0], dyT = C[n][1] - C[n - 1][1], dT = Math.hypot(dxT, dyT) || 1;
      var cap = [tip[0] + dxT / dT * tw * 0.55, tip[1] + dyT / dT * tw * 0.55];
      var d = 'M' + f1(L[0][0]) + ' ' + f1(L[0][1]);
      for (k = 1; k <= n; k++) d += 'L' + f1(L[k][0]) + ' ' + f1(L[k][1]);
      d += 'Q' + f1(cap[0]) + ' ' + f1(cap[1]) + ' ' + f1(R[n][0]) + ' ' + f1(R[n][1]);
      for (k = n - 1; k >= 0; k--) d += 'L' + f1(R[k][0]) + ' ' + f1(R[k][1]);
      return { d: d + 'Z', C: C, H: H, tip: tip, n: n };
    }
    var line = function (pts, from, to) {
      var d = '', k; for (k = from; k <= to; k++) d += (k === from ? 'M' : 'L') + f1(pts[k][0]) + ' ' + f1(pts[k][1]); return d;
    };
    function cubic(s, c1, c2, e) { return 'M' + f1(s[0]) + ' ' + f1(s[1]) + 'C' + f1(c1[0]) + ' ' + f1(c1[1]) + ' ' + f1(c2[0]) + ' ' + f1(c2[1]) + ' ' + f1(e[0]) + ' ' + f1(e[1]); }
    // The thinner arm up to the reading card. It grows out of the sphere with the same spring as the side arm, ends just
    // INSIDE the card's lower edge (so no drop is left dangling under it, and the end no longer slides along the edge)
    // and rests two short fingers along that edge once it has arrived. Phones draw only this arm.
    function drawUp(dt, pr, wr, shown, thick, gripNow) {
      var cx = wr.left - pr.left + wr.width / 2, cy = wr.top - pr.top + wr.height / 2;
      var rd = reader && reader.getBoundingClientRect();
      var U0 = [cx, cy - wr.height * 0.36], U1 = rd ? [cx, rd.bottom - pr.top - 2] : null;
      if (!U1 || U1[1] > U0[1] - 10) {                     // no room between the card and the sphere: nothing stale is left on screen
        upBody.setAttribute('d', ''); upHl.setAttribute('d', '');
        upFingers[0].style.opacity = 0; upFingers[1].style.opacity = 0;
        return;
      }
      upPulse = Math.max(0, upPulse - dt / 0.7);
      var UP = [U0, [U0[0] + Math.sin(flow * 1.3) * 6, U0[1] - (U0[1] - U1[1]) * 0.4], [U1[0] - Math.sin(flow * 1.1) * 5, U1[1] + 26], U1];
      var w = thick * (0.5 + 0.2 * upPulse), UT = tube(UP, shown, w, w * 0.75, flow + 1.7);
      upBody.setAttribute('d', UT.d); upHl.setAttribute('d', line(UT.H, Math.round(UT.n * 0.25), UT.n));
      upHl.style.strokeWidth = f1(Math.max(1, w * 0.16));
      var g = clamp(gripNow, 0, 1.15), edge = U1[1] + 3.5, span = (10 + w) * Math.min(1, g);
      [-1, 1].forEach(function (s, i) {
        var tipF = [U1[0] + s * span, edge];
        upFingers[i].setAttribute('d', cubic([U1[0], edge - 1], [U1[0] + s * span * 0.4, edge + 2], [U1[0] + s * span * 0.8, edge + 2.5], tipF));
        upFingers[i].style.opacity = (shown > 0.97 ? Math.min(1, g * 1.5) : 0).toFixed(3);
        upFingers[i].style.strokeWidth = f1(Math.max(2.5, thick * 0.2));
      });
    }
    var grabSound = function () { uiSound('grab'); };
    // phones: no side tendril (the chapters sit under the sphere), only the arm up to the card that is open on top
    function drawPhoneArm(dt) {
      arms.style.display = '';
      var pr = pin.getBoundingClientRect(), wr = wrap.getBoundingClientRect();
      arms.setAttribute('viewBox', '0 0 ' + pr.width + ' ' + pr.height);
      [bodyEl, hlEl, hl2El, glintEl, ghostEl].forEach(function (el) { el.setAttribute('d', ''); });
      fingers[0].style.opacity = 0; fingers[1].style.opacity = 0;
      if (armFor !== current) { armFor = current; reach = 0; reachV = 0; grabbed = false; upPulse = 1; }
      var k = 120, c = 2 * Math.sqrt(k) * 0.6;
      reachV += (k * (1 - reach) - c * reachV) * dt; reach += reachV * dt;
      if (!grabbed && reach > 0.96) { grabbed = true; grabSound(); }
      var gk = 200, gc = 2 * Math.sqrt(gk) * 0.55;
      gripV += (gk * ((grabbed ? 1 : 0) - grip) - gc * gripV) * dt; grip += gripV * dt;
      flow += dt;
      drawUp(dt, pr, wr, clamp(reach, 0.02, 1), wr.width * 0.085, grip);
    }
    function drawArms(dt, aDeg) {
      if (current < 0) { arms.style.display = 'none'; return; }
      if (!isPinned()) { drawPhoneArm(dt); return; }
      arms.style.display = '';
      var pr = pin.getBoundingClientRect(), wr = wrap.getBoundingClientRect();
      arms.setAttribute('viewBox', '0 0 ' + pr.width + ' ' + pr.height);
      var cx = wr.left - pr.left + wr.width / 2, cy = wr.top - pr.top + wr.height / 2, a = aDeg * Math.PI / 180;
      var dir = [Math.sin(a), -Math.cos(a)], R0 = wr.width * 0.36;              // starts INSIDE the sphere: it grows out of it
      var S = [cx + dir[0] * R0, cy + dir[1] * R0];
      var tab = tabs[current], tr = tab.getBoundingClientRect(), left = !!tab.closest('.tabs.left'), sx = left ? 1 : -1;
      var E = [(left ? tr.right : tr.left) - pr.left + sx * 3, tr.top - pr.top + tr.height / 2];
      var Ld = Math.hypot(E[0] - S[0], E[1] - S[1]);
      var P = [S, [S[0] + dir[0] * Ld * 0.5, S[1] + dir[1] * Ld * 0.5], [E[0] + sx * Ld * 0.4, E[1]], E];
      if (armFor !== current) {                                            // let go of the old card, reach for the new one
        if (armFor >= 0) { lastGhost = bodyEl.getAttribute('d') || ''; ghost = 1; }
        tabs.forEach(function (t) { t.classList.remove('grabbed'); });
        armFor = current; reach = 0; reachV = 0; grabbed = false; upPulse = 1;
      }
      var k = 120, c = 2 * Math.sqrt(k) * 0.6;                             // reach: underdamped spring, a little overshoot
      reachV += (k * (1 - reach) - c * reachV) * dt; reach += reachV * dt;
      var shown = clamp(reach, 0.02, 1);
      flow += dt;
      var thick = wr.width * 0.085;                                         // as thick as a ripple on the sphere at its root
      var T = tube(P, shown, thick, Math.max(5, thick * 0.38), flow);
      bodyEl.setAttribute('d', T.d);
      gradEl.setAttribute('x1', f1(S[0])); gradEl.setAttribute('y1', f1(S[1])); gradEl.setAttribute('x2', f1(E[0])); gradEl.setAttribute('y2', f1(E[1]));
      // the specular along the lit side (skips the root, which is under the sphere), and a fainter second one
      hlEl.setAttribute('d', line(T.H, Math.round(T.n * 0.18), T.n)); hlEl.style.strokeWidth = f1(Math.max(1.2, thick * 0.16));
      hl2El.setAttribute('d', line(T.C, Math.round(T.n * 0.3), T.n));
      // glints that travel from the sphere to the card once it holds
      glintEl.setAttribute('d', line(T.H, 0, T.n));
      var gl = glintEl.getTotalLength ? glintEl.getTotalLength() : 200;
      glintEl.style.strokeDasharray = '6 ' + f1(gl * 0.45); glintEl.style.strokeDashoffset = f1(-(flow * 140) % (gl * 0.45 + 6));
      glintEl.style.opacity = grabbed ? 0.95 : 0;
      if (!grabbed && reach > 0.96) { grabbed = true; tab.classList.add('grabbed'); grabSound(); }
      // fingers: two curls of the same graphite close round the card's edge, above and below the grip
      var gk = 200, gc = 2 * Math.sqrt(gk) * 0.55;
      gripV += (gk * ((grabbed ? 1 : 0) - grip) - gc * gripV) * dt; grip += gripV * dt;
      var g = clamp(grip, 0, 1.15), reachY = (tr.height / 2 - 6) * Math.min(1, g), tip = T.tip;
      [-1, 1].forEach(function (up, i) {
        var tipF = [tip[0] - sx * 10 * g, tip[1] + up * reachY];
        fingers[i].setAttribute('d', cubic(tip, [tip[0] + sx * 8, tip[1] + up * reachY * 0.2], [tip[0] + sx * 7, tip[1] + up * reachY], tipF));
        fingers[i].style.opacity = Math.min(1, g * 1.5).toFixed(3); fingers[i].style.strokeWidth = f1(Math.max(2.5, thick * 0.24));
      });
      // the old arm is drawn back into the sphere: it shrinks toward the centre and fades
      if (ghost > 0) {
        ghost = Math.max(0, ghost - dt / 0.3);
        ghostEl.setAttribute('d', lastGhost); ghostEl.style.opacity = (ghost * 0.85).toFixed(3);
        ghostEl.style.transformOrigin = f1(cx) + 'px ' + f1(cy) + 'px'; ghostEl.style.transform = 'scale(' + (0.55 + 0.45 * ghost).toFixed(3) + ')';
      } else ghostEl.style.opacity = 0;
      // the thinner one up to the reading panel: grows with the same reach, swells as the chapter changes
      drawUp(dt, pr, wr, shown, thick, grip);
    }

    // Two critically damped springs, integrated by real time (no "fraction per frame"):
    //   marker — fast (k 70): follows the pointer round the ring, settles on the open chapter;
    //   sphere — slow (k 22): turns only when the chapter changes, so pointer jitter never spins it.
    var mA = angles[0], mV = 0, yaw = 0, yV = 0, last = 0, spin = 0, tiltV = 0, started = false;
    function spring(x, v, target, k, dt) { var c = 2 * Math.sqrt(k); var a = k * target - k * x - c * v; v += a * dt; return [x + v * dt, v]; }
    var stageOn = true;      // asleep while the stage is off screen or the tab is hidden (sphere, arms, marker)
    if ('IntersectionObserver' in window) new IntersectionObserver(function (e) { stageOn = e[0].isIntersecting; }).observe(stage);
    (function loop(now) {
      if (!stageOn || document.hidden) { last = 0; requestAnimationFrame(loop); return; }
      var dt = last ? Math.min(0.05, (now - last) / 1000) : 0.016; last = now;
      if (current < 0) { requestAnimationFrame(loop); return; }   // no chapter picked yet (first frame)
      if (!started) { yaw = mA = angles[current]; started = true; }
      var s;
      if (mode === 'coast') {                                     // after a throw: friction, picking chapters as it passes them
        yV *= Math.exp(-dt * 2.6); yaw += yV * dt;
        var i = nearest(((yaw % 360) + 360) % 360); if (i !== current) setActive(i);
        if (Math.abs(yV) < 40) mode = 'spring';
      } else if (mode === 'spring') {                             // settle on the open chapter, the short way round
        s = spring(yaw, yV, yaw + adiff(angles[current], yaw), 22, dt); yaw = s[0]; yV = s[1];
      }
      if (!drag) { s = spring(tilt, tiltV, 0, 30, dt); tilt = s[0]; tiltV = s[1]; }
      var mT = drag || mode === 'coast' ? yaw : hovering ? pointerA : angles[current];
      var mt = mA + adiff(mT, mA); s = spring(mA, mV, mt, 70, dt); mA = s[0]; mV = s[1];
      if (ring) ring.style.setProperty('--a', mA.toFixed(2) + 'deg');
      drawArms(dt, mA);
      spin += (clamp(Math.abs(yV) / 300, 0, 1) - spin) * Math.min(1, dt * 6);       // ripples swell gently while it turns
      if (orb && orb.st) { orb.st.yaw = yaw * Math.PI / 180; orb.st.pitch = -0.12 + tilt; orb.st.spin = spin; }
      requestAnimationFrame(loop);
    })(0);

    var h0 = ids.indexOf(location.hash.slice(1));
    setActive(h0 >= 0 ? h0 : 0, true);
    if (h0 > 0 && isPinned()) {
      if ('scrollRestoration' in history) history.scrollRestoration = 'manual';
      setTimeout(function () { window.scrollTo(0, stage.offsetTop + stageRange() * h0 / (N - 1)); }, 50);
    }
  }

  /* ---------- header + veil under the long-text sections ---------- */
  var veil = document.querySelector('.veil');
  function onScroll() {
    if (veil && stage) {
      var end = stage.offsetTop + stage.offsetHeight - window.innerHeight;
      veil.style.setProperty('--veil', clamp((window.scrollY - end) / (window.innerHeight * 0.6), 0, 1).toFixed(3));
    }
  }
  window.addEventListener('scroll', onScroll, { passive: true }); onScroll();

  /* ---------- framer-motion: page transitions + scroll reveal ---------- */
  var parts = [].slice.call(document.querySelectorAll('main, .foot'));
  if (!M) { html.classList.remove('pt'); return; }
  var D = 14, IN = 0.42, OUT = 0.26, leaving = false;
  function enter(back) {
    leaving = false;
    parts.forEach(function (el) { el.style.opacity = '0'; });
    html.classList.remove('pt');
    parts.forEach(function (el) {
      M.animate(el, { opacity: [0, 1], transform: ['translateY(' + (back ? -D : D) + 'px)', 'none'] }, { duration: IN, ease: [0.16, 1, 0.3, 1] })
        .then(function () { el.style.opacity = ''; el.style.transform = ''; });
    });
  }
  var nav = performance.getEntriesByType && performance.getEntriesByType('navigation')[0];
  enter(nav && nav.type === 'back_forward');
  window.addEventListener('pageshow', function (e) { if (e.persisted) enter(true); });
  document.addEventListener('click', function (e) {
    var a = e.target.closest('a[href]');
    if (!a || e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
    if (a.target && a.target !== '_self' || a.hasAttribute('download')) return;
    var url = new URL(a.href, location.href);
    if (url.origin !== location.origin || !/^(https?|file):$/.test(url.protocol)) return;
    if (url.pathname === location.pathname) return;
    e.preventDefault();
    if (leaving) { location.href = url.href; return; }
    leaving = true;
    var go = function () { location.href = url.href; };
    var safety = setTimeout(go, OUT * 1000 + 150);
    Promise.all(parts.map(function (el) {
      return M.animate(el, { opacity: [1, 0], transform: ['none', 'translateY(' + -D + 'px)'] }, { duration: OUT, ease: [0.4, 0, 1, 1] });
    })).then(function () { clearTimeout(safety); go(); }, go);
  });
  /* ---------- framer-motion dom.js: springs, stagger, inView, hover / press, light on the glass ----------
     ui-ux-pro-max: motion carries meaning (things arrive in reading order), springs for touch, exit faster than
     enter, hover feedback on everything clickable. Transforms and opacity only - nothing animates layout. */
  var spring = { type: 'spring', stiffness: 260, damping: 24, mass: 0.9 };
  var ease = [0.16, 1, 0.3, 1];

  // the claim at the top: its words rise in one after another, out of a blur
  [].slice.call(document.querySelectorAll('.hero-title')).forEach(function (h) {
    h = h.querySelector('.t') || h;
    var words = h.textContent.trim().split(' ').filter(Boolean);
    h.textContent = '';
    words.forEach(function (w, i) {
      var sp = document.createElement('span'); sp.className = 'w'; sp.textContent = w; h.appendChild(sp);
      if (i < words.length - 1) h.appendChild(document.createTextNode(' '));
    });
    M.animate(h.querySelectorAll('.w'), { opacity: [0, 1], y: [22, 0], filter: ['blur(8px)', 'blur(0px)'] },
      { duration: 0.8, ease: ease, delay: M.stagger ? M.stagger(0.07, { startDelay: 0.12 }) : 0 });
  });

  // the plugin window lands like a device put on the desk: tilted back, then flat
  var plug = document.getElementById('panel');
  if (plug && M.inView) {
    html.classList.add('plug-ready');
    M.inView(plug, function () {
      M.animate(plug, { opacity: [0, 1], rotateX: [16, 0], y: [70, 0], scale: [0.93, 1] }, { duration: 1.15, ease: ease })
        .then(function () { plug.style.transform = ''; });
    }, { amount: 0.12 });
  }

  // everything else: in view -> spring up, siblings one after another
  if (M.inView) {
    html.classList.add('reveal-ready');
    [].slice.call(document.querySelectorAll('[data-reveal]')).forEach(function (el) {
      var sib = [].slice.call(el.parentElement.children).filter(function (c) { return c.hasAttribute('data-reveal'); });
      M.inView(el, function () {
        M.animate(el, { opacity: [0, 1], y: [34, 0], scale: [0.965, 1] },
          { opacity: { duration: 0.5, ease: ease }, default: { type: 'spring', stiffness: 170, damping: 22 }, delay: sib.indexOf(el) * 0.09 })
          .then(function () { el.removeAttribute('data-reveal'); el.style.opacity = ''; el.style.transform = ''; });
      }, { amount: 0.12 });
    });
  }

  // touch: buttons swell under the pointer and give under the press; the download cards lift
  var BTN = '.btn, .pill, .cta, .live-menu button, .login';
  if (M.hover) {
    M.hover(BTN, function (el) {
      M.animate(el, { scale: 1.04, y: -2 }, spring);
      return function () { M.animate(el, { scale: 1, y: 0 }, spring); };
    });
    M.hover('.dl, .about > .glass', function (el) {
      M.animate(el, { y: -6 }, spring);
      return function () { M.animate(el, { y: 0 }, spring); };
    });
  }
  if (M.press) M.press(BTN, function (el) {
    M.animate(el, { scale: 0.95 }, { type: 'spring', stiffness: 600, damping: 30 });
    return function () { M.animate(el, { scale: 1.04 }, spring); };
  });

  // liquid glass: a soft light follows the pointer across whatever glass it is over
  var LIT = '.glass, .card, .btn, .pill, .cta, .block .head, .prose, .note', litEl = null;
  document.addEventListener('pointermove', function (e) {
    var el = e.target.closest ? e.target.closest(LIT) : null;
    if (el !== litEl) { if (litEl) litEl.style.setProperty('--lit', '0'); litEl = el; }
    if (!el) return;
    var r = el.getBoundingClientRect();
    el.style.setProperty('--mx', (e.clientX - r.left) + 'px'); el.style.setProperty('--my', (e.clientY - r.top) + 'px');
    el.style.setProperty('--lit', '1');
  }, { passive: true });
  document.addEventListener('pointerleave', function () { if (litEl) litEl.style.setProperty('--lit', '0'); litEl = null; });

  /* ---------- VECTOR WORDMARK on the header's HOUZY: the plugin's HouzyWordmark (PluginEditor.h), same rules ----------
     Only while the pointer is over the word (at rest the ordinary text shows and this costs nothing): the letters dissolve
     toward their bottom, turn into a dotted outline around the pointer, and three vector handles - squares joined by a
     dashed triangle, each labelled x, y - snap to the grid cells next to the pointer and drift. Pointer, fade and drift
     run on elapsed time. Composed per pixel like the plugin (~15k px); takes no clicks, the link under it still works. */
  (function () {
    var word = document.querySelector('.bar .brand b');
    if (!word || !window.matchMedia('(hover: hover)').matches) return;
    var brand = word.parentElement, bar = brand.closest('.bar'), PAD = 14;
    var cv = document.createElement('canvas'); cv.className = 'wordmark'; cv.setAttribute('aria-hidden', 'true');
    brand.appendChild(cv);
    var c = cv.getContext('2d');
    var W = 0, H = 0, cssW = 0, cssH = 0, dpr = 1, fill, soft, dots, img, tTop = 0, tH = 1, dirty = true;
    var hover = 0, active = false, hasPtr = false, ptr = [0, 0], eased = [0, 0], drift = 0, last = 0, running = false;
    var cells = [[0, 0], [0, 0], [0, 0]], verts = [[0, 0], [0, 0], [0, 0]];
    var INK = [22, 25, 31], SHADE = [31, 111, 125], ACC = '31,111,125';    // --ink, --accent-ink

    function layer(draw) {
      var o = document.createElement('canvas'); o.width = W; o.height = H;
      var x = o.getContext('2d'); x.setTransform(dpr, 0, 0, dpr, 0, 0); draw(x, o); return o;
    }
    function alphaOf(o) {
      var d = o.getContext('2d').getImageData(0, 0, W, H).data, a = new Float32Array(W * H);
      for (var i = 0; i < a.length; i++) a[i] = d[i * 4 + 3] / 255;
      return a;
    }
    // the layers - rebuilt only when the size or the screen scale changes
    function build() {
      var br = brand.getBoundingClientRect(), wr = word.getBoundingClientRect();
      dpr = Math.min(window.devicePixelRatio || 1, 2);
      cssW = wr.width + 2 * PAD; cssH = wr.height + 2 * PAD;
      cv.style.left = (wr.left - br.left - PAD) + 'px'; cv.style.top = (wr.top - br.top - PAD) + 'px';
      cv.style.width = cssW + 'px'; cv.style.height = cssH + 'px';
      W = Math.round(cssW * dpr); H = Math.round(cssH * dpr); cv.width = W; cv.height = H;
      var cs = getComputedStyle(word), font = '650 ' + cs.fontSize + ' ' + cs.fontFamily;
      var mc = document.createElement('canvas').getContext('2d'); mc.font = font;
      var asc = mc.measureText('H').fontBoundingBoxAscent, gm = mc.measureText('HOUZY');
      // every letter exactly where the page put it (letter-spacing included), from the text's own boxes
      var node = word.firstChild, rng = document.createRange(), chars = [];
      for (var i = 0; i < node.length; i++) {
        rng.setStart(node, i); rng.setEnd(node, i + 1);
        var r = rng.getBoundingClientRect();
        chars.push([node.data[i], r.left - wr.left + PAD, r.top - wr.top + PAD + asc]);
      }
      tTop = chars[0][2] - gm.actualBoundingBoxAscent; tH = gm.actualBoundingBoxAscent + gm.actualBoundingBoxDescent;
      var each = function (x, how) { x.font = font; x.textBaseline = 'alphabetic'; chars.forEach(function (ch) { how(x, ch); }); };
      var fillC = layer(function (x) { x.fillStyle = '#fff'; each(x, function (x, ch) { x.fillText(ch[0], ch[1], ch[2]); }); });
      var dotD = Math.max(1.1 / dpr, tH * 0.045), pitch = dotD * 2.6;     // the dotted outline: round dots along every contour
      var dotC = layer(function (x) {
        x.strokeStyle = '#fff'; x.lineWidth = dotD; x.lineCap = 'round'; x.setLineDash([0.001, pitch]);
        each(x, function (x, ch) { x.strokeText(ch[0], ch[1], ch[2]); });
      });
      var softC = layer(function (x) {                                      // the soft copy: a small blur of the fill
        x.setTransform(1, 0, 0, 1, 0, 0); x.filter = 'blur(' + Math.max(1, Math.round(tH * 0.06 * dpr)) + 'px)'; x.drawImage(fillC, 0, 0);
      });
      fill = alphaOf(fillC); dots = alphaOf(dotC); soft = alphaOf(softC);
      img = c.createImageData(W, H);
      dirty = false;
    }
    // the frame, per pixel like the plugin: crisp at the top, soft and fading toward the bottom, dots around the pointer
    function compose() {
      var d = img.data, px = eased[0] * dpr, py = eased[1] * dpr, reach = tH * 1.9 * dpr, T = tTop * dpr, TH = Math.max(1, tH * dpr);
      for (var y = 0; y < H; y++) {
        var ey = Math.min(1, Math.max(0, 1 - (y - T) / TH));               // 0 bottom .. 1 top
        var softMix = Math.min(1, Math.max(0, 1 - ey * 3.5)), fade = Math.pow(ey, 0.7), g = ey * ey * (3 - 2 * ey);
        var cr = SHADE[0] + (INK[0] - SHADE[0]) * g, cg = SHADE[1] + (INK[1] - SHADE[1]) * g, cb = SHADE[2] + (INK[2] - SHADE[2]) * g;
        for (var x = 0; x < W; x++) {
          var i = y * W + x, sf = fill[i] + (soft[i] - fill[i]) * softMix, k = 0;
          if (hasPtr) { var t = Math.min(1, Math.hypot(x - px, y - py) / reach), s = t * t * (3 - 2 * t); k = 1 - s * s * s; }
          var a = Math.min(1, Math.max(0, (sf + (dots[i] - sf) * k) * fade * hover)), j = i * 4;
          d[j] = cr; d[j + 1] = cg; d[j + 2] = cb; d[j + 3] = a * 255;
        }
      }
      c.putImageData(img, 0, 0);
    }
    function handles() {
      if (!hasPtr) return;
      var a = 0.55 * hover, half = tH * 0.27, i;
      c.setTransform(dpr, 0, 0, dpr, 0, 0);
      c.strokeStyle = 'rgba(' + ACC + ',' + a + ')'; c.lineWidth = 0.7; c.setLineDash([2, 2]);
      c.beginPath();
      for (i = 0; i < 3; i++) { var p = verts[i], q = verts[(i + 1) % 3]; c.moveTo(p[0], p[1]); c.lineTo(q[0], q[1]); }
      c.stroke(); c.setLineDash([]);
      c.font = '7px ui-monospace, Consolas, monospace'; c.textBaseline = 'alphabetic';
      var taken = [];                                                       // labels drawn so far: never overlap one
      for (i = 0; i < 3; i++) {
        var v = verts[i];
        c.strokeStyle = 'rgba(' + ACC + ',' + a + ')'; c.strokeRect(v[0] - half, v[1] - half, half * 2, half * 2);
        var txt = Math.round(clamp(v[0] / cssW * 100, 0, 100)) + ', ' + Math.round(clamp((1 - v[1] / cssH) * 100, 0, 100));
        var lr = [v[0] - half, v[1] - half - 9, c.measureText(txt).width + 2, 7];
        var clash = taken.some(function (t) { return lr[0] < t[0] + t[2] && t[0] < lr[0] + lr[2] && lr[1] < t[1] + t[3] && t[1] < lr[1] + lr[3]; });
        if (!clash) { c.fillStyle = 'rgba(' + ACC + ',' + Math.min(1, a * 1.1) + ')'; c.fillText(txt, lr[0], lr[1] + 7); taken.push([lr[0] - 2, lr[1] - 1, lr[2] + 4, lr[3] + 2]); }
      }
      c.setTransform(1, 0, 0, 1, 0, 0);
    }
    function snapCells() {
      var cw = tH * 1.6, ch = cw * 0.5, cx = Math.floor(ptr[0] / cw), cy = Math.floor(ptr[1] / ch), found = [];
      for (var i = -1; i <= 1; i++) for (var j = -1; j <= 1; j++) {
        var p = [(cx + i + 0.5) * cw, (cy + j + 0.5) * ch];
        found.push([p, Math.hypot(p[0] - ptr[0], p[1] - ptr[1])]);
      }
      found.sort(function (a, b) { return a[1] - b[1]; });
      for (i = 0; i < 3; i++) cells[i] = found[i + 1][0];
    }
    function updateVerts() {
      var cw = tH * 1.6, ch = cw * 0.5, fr = function (v) { return v - Math.floor(v); };
      for (var i = 0; i < 3; i++) {
        var sx = Math.round(cells[i][0] / cw - 0.5), sy = Math.round(cells[i][1] / ch - 0.5);
        var h1 = fr(Math.sin(sx * 127.1 + sy * 311.7) * 43758.5453), h2 = fr(Math.sin(sx * 269.5 + sy * 183.3) * 43758.5453);
        verts[i] = [cells[i][0] + 0.08 * cw * Math.sin(drift * 1.3 + h1 * 6.2832), cells[i][1] + 0.04 * ch * Math.sin(drift * 1.69 + h2 * 6.2832)];
        verts[i][1] = clamp(verts[i][1], tTop + 2, tTop + tH - 2);          // on the lettering, inside this box
        verts[i][0] = clamp(verts[i][0], 8, cssW - 40);
      }
    }
    function loop(now) {
      var dt = last ? Math.min(0.1, (now - last) / 1000) : 0.016; last = now;
      hover += ((active ? 1 : 0) - hover) * (1 - Math.exp(-dt / 0.12));
      if (!active && hover < 0.003) hover = 0;
      var damp = 1 - Math.exp(-dt / 0.08);
      eased[0] += (ptr[0] - eased[0]) * damp; eased[1] += (ptr[1] - eased[1]) * damp;
      drift += dt;
      if (hasPtr) { snapCells(); updateVerts(); }
      word.style.opacity = (1 - hover).toFixed(3);                          // the ordinary word fades out under the effect
      c.setTransform(1, 0, 0, 1, 0, 0); c.clearRect(0, 0, W, H);
      if (hover > 0.002) { compose(); handles(); }
      if (!active && hover === 0) { hasPtr = false; running = false; last = 0; word.style.opacity = ''; return; }
      requestAnimationFrame(loop);
    }
    function setActive(a) {
      active = a;
      if (a && !running) { running = true; last = 0; requestAnimationFrame(loop); }
    }
    bar.addEventListener('pointermove', function (e) {
      if (e.pointerType !== 'mouse') return;
      var r = word.getBoundingClientRect();
      var inside = e.clientX > r.left - 10 && e.clientX < r.right + 10 && e.clientY > r.top - 6 && e.clientY < r.bottom + 6;
      if (inside) {
        if (dirty || !W) build();
        var cr = cv.getBoundingClientRect();
        ptr = [e.clientX - cr.left, e.clientY - cr.top];
        if (!hasPtr) eased = ptr.slice();
        hasPtr = true;
      }
      setActive(inside);
    });
    bar.addEventListener('pointerleave', function () { setActive(false); });
    window.addEventListener('resize', function () { dirty = true; });
    if (document.fonts) document.fonts.ready.then(function () { dirty = true; });   // the letters are measured in Sora
  })();

  /* ---------- the header's selection: a DROP of liquid glass, like the selected key of the plugin's switches ----------
     It sits under ACRONA AUDIO at the top of the page and flows to Technology / About / FAQ / Contact as the page scrolls
     to them (to Manual on the manual page). Like SegmentedSwitch in the plugin: the leading edge runs ahead, the trailing
     one follows, and the drop thins across while it stretches, then gathers on the new place. Time based, no bounce. */
  (function () {
    var bar = document.querySelector('.bar'), small = bar && bar.querySelector('.brand small');
    var links = bar ? [].slice.call(bar.querySelectorAll('.menu a')) : [];
    if (!bar || !small || !links.length) return;
    var items = [small].concat(links);
    var at = function (end) { for (var i = 1; i < items.length; i++) if (items[i].getAttribute('href').slice(-end.length) === end) return i; return 0; };
    var IDX = { tech: at('#top'), manual: at('manual.html'), about: at('#about'), faq: at('#faq'), contact: at('#contact') };
    var drop = document.createElement('i'); drop.className = 'bar-drop'; drop.setAttribute('aria-hidden', 'true');
    bar.insertBefore(drop, bar.firstChild);
    var PADX = 12, PADY = 5, L = 0, R = 0, tL = 0, tR = 0, tTop = 0, tH = 0, cur = -1, lock = -1, lockUntil = 0, running = false, last = 0;

    function place(i) {
      var br = bar.getBoundingClientRect(), r = items[i].getBoundingClientRect();
      var px = i === 0 ? 7 : PADX;                                          // ACRONA AUDIO sits close to HOUZY: a tighter drop
      tL = r.left - br.left - px; tR = r.right - br.left + px; tTop = r.top - br.top - PADY; tH = r.height + 2 * PADY;
    }
    function draw() {
      var w = R - L, tw = Math.max(1, tR - tL);
      var stretch = Math.min(1, Math.max(0, (w - tw) / (tw * 1.5)));      // how far the drop is pulled out
      var h = tH * (1 - 0.28 * stretch);                                   // narrower across while it flows (as in the plugin)
      drop.style.width = w.toFixed(1) + 'px'; drop.style.height = h.toFixed(1) + 'px';
      drop.style.transform = 'translate(' + L.toFixed(1) + 'px,' + (tTop + (tH - h) / 2).toFixed(1) + 'px)';
    }
    function frame(now) {
      var dt = last ? Math.min(0.05, (now - last) / 1000) : 0.016; last = now;
      var kLead = 1 - Math.exp(-dt / 0.045), kTrail = 1 - Math.exp(-dt / 0.13);
      var right = (tL + tR) >= (L + R);                                    // moving right: the right edge leads
      R += (tR - R) * (right ? kLead : kTrail);
      L += (tL - L) * (right ? kTrail : kLead);
      draw();
      if (Math.abs(R - tR) < 0.15 && Math.abs(L - tL) < 0.15) { R = tR; L = tL; draw(); running = false; last = 0; return; }
      requestAnimationFrame(frame);
    }
    function go(i, instant) {
      place(i);
      if (instant || cur < 0) { L = tL; R = tR; draw(); drop.classList.add('on'); }
      else if (!running) { running = true; last = 0; requestAnimationFrame(frame); }
      cur = i;
    }
    function which() {
      if (/manual\.html$/.test(location.pathname)) return IDX.manual;
      var vh = window.innerHeight * 0.4, top = function (s) { var e = document.querySelector(s); return e ? e.getBoundingClientRect().top : Infinity; };
      if (top('#contact') <= vh) return IDX.contact;
      if (top('#faq') <= vh) return IDX.faq;
      if (top('#about') <= vh) return IDX.about;
      if (top('.stage') <= vh) return IDX.tech;
      return 0;
    }
    var pending = false;
    function update() {
      pending = false;
      var want = performance.now() < lockUntil ? lock : which();           // after a click the drop goes straight to its item
      if (want !== cur) go(want);
    }
    function ask() { if (!pending) { pending = true; requestAnimationFrame(update); } }
    items.forEach(function (el, i) {
      el.addEventListener('click', function () { lock = i; lockUntil = performance.now() + 1100; ask(); });
    });
    window.addEventListener('scroll', ask, { passive: true });
    window.addEventListener('resize', function () { if (cur >= 0) { place(cur); L = tL; R = tR; draw(); } });
    if (document.fonts) document.fonts.ready.then(function () { if (cur >= 0) { place(cur); L = tL; R = tR; draw(); } });
    go(which(), true);
  })();
})();
