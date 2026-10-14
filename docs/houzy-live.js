// HOUZY live demo, page side: pick an MP3/WAV, decode it HERE (nothing is uploaded - the site is
// static and this file sends no request with the audio), play it through the plugin's engine in an
// AudioWorklet (houzy-worklet.js), and let the panel's knobs drive it (plugin.js calls houzyParam).
(function () {
  var ui = document.getElementById('live-ui');
  if (!ui) return;
  var $ = function (s) { return ui.querySelector(s); };
  var pick = $('input[type=file]'), row = $('.live-row'), prog = $('.live-progress'), progBar = $('.live-progress i'),
      progText = $('.live-progress span'), playBtn = $('.live-play'), abBtn = $('.live-ab'), clearBtn = $('.live-clear'),
      bar = $('.live-bar'), barFill = $('.live-bar i'), timeEl = $('.live-time'), nameEl = $('.live-name');
  var toast = document.querySelector('.live-toast');
  var T = function (s, v) { return window.houzyT ? window.houzyT(s, v) : s; };   // site.js: the page language

  var ctx = null, node = null, track = null, src = null, startedAt = 0, offset = 0, playing = false, bypass = false;
  var params = {};          // last value of every knob: replayed into a freshly started engine
  var MAX_MB = 300;

  function say(text, ms) {
    if (!toast) return;
    toast.textContent = text; toast.classList.add('on');
    clearTimeout(say.t); say.t = setTimeout(function () { toast.classList.remove('on'); }, ms || 2200);
  }
  function progress(frac, text) {
    prog.hidden = false;
    progBar.style.width = frac == null ? '' : Math.round(frac * 100) + '%';
    prog.classList.toggle('busy', frac == null);
    progText.textContent = text;
  }
  function send(id, value) { if (node) node.port.postMessage({ id: id, value: value }); }

  // plugin.js: every knob / slider change lands here (also its defaults once at start)
  window.houzyParam = function (id, value) {
    params[id] = value;
    send(id, value);
  };
  window.houzyLocked = function (what) { say(T('{what} — only in the full version', { what: T(what) })); };
  window.houzySay = function (text) { say(text); };
  window.houzyBypass = function () { setBypass(!bypass); };

  function setBypass(on) {
    bypass = on; params.bypass = on ? 1 : 0; send('bypass', params.bypass);
    abBtn.setAttribute('aria-pressed', on ? 'true' : 'false');
    abBtn.textContent = on ? T('Original') : 'HOUZY';
    document.dispatchEvent(new CustomEvent('houzy-bypass', { detail: on }));
  }

  // the engine: fetched once, with progress; the worklet gets the bytes
  var engineReady = null;
  function startEngine() {
    if (engineReady) return engineReady;
    engineReady = (async function () {
      ctx = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: 'playback' });
      // browsers give a page AudioWorklet only over https (or on localhost): say so instead of blaming the browser
      if (!ctx.audioWorklet) throw new Error(window.isSecureContext ? T('this browser has no AudioWorklet') : T('the live demo needs the secure (https) version of this page'));
      var res = await fetch('/houzy-engine.wasm?v=20261014j');
      if (!res.ok) throw new Error(T('engine not found'));
      var total = +res.headers.get('content-length') || 0, got = 0, parts = [], reader = res.body.getReader();
      for (;;) {
        var r = await reader.read(); if (r.done) break;
        parts.push(r.value); got += r.value.length;
        progress(total ? got / total : null, T('Loading the HOUZY engine…'));
      }
      var wasm = new Uint8Array(got), at = 0;
      parts.forEach(function (p) { wasm.set(p, at); at += p.length; });
      await ctx.audioWorklet.addModule('/houzy-worklet.js');
      node = new AudioWorkletNode(ctx, 'houzy', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2],
        processorOptions: { wasm: wasm.buffer, host: location.hostname } });
      node.connect(ctx.destination);
      await new Promise(function (ok, fail) {
        node.port.onmessage = function (e) {
          var m = e.data;
          if (m.type === 'ready') ok();
          else if (m.type === 'error') fail(new Error(m.message));
          else if (m.type === 'locked') say(LOCKED[m.id] ? T('{what} — only in the full version', { what: T(LOCKED[m.id]) }) : T('Only in the full version'));
          else if (m.type === 'meters') window.houzyMeters = m.m;   // plugin.js draws the meters and the GR graph
        };
      });
      Object.keys(params).forEach(function (k) { send(k, params[k]); });
    })();
    engineReady.catch(function () { engineReady = null; });
    return engineReady;
  }
  var LOCKED = { limmode: 'T8', osfactor: 'Oversampling above 16x', enginemode: 'CLASSIC and MODERN',
                 upcomp: 'UPWARD', character: 'CHARACTER', clipshape: 'CLIP SHAPE', dash: 'DASH' };

  function readFile(file) {
    return new Promise(function (ok, fail) {
      var fr = new FileReader();
      fr.onprogress = function (e) { if (e.lengthComputable) progress(e.loaded / e.total, T('Reading {name}…', { name: file.name })); };
      fr.onload = function () { ok(fr.result); };
      fr.onerror = function () { fail(fr.error); };
      fr.readAsArrayBuffer(file);
    });
  }

  pick.addEventListener('change', async function () {
    var file = pick.files && pick.files[0];
    if (!file) return;
    if (!/\.(mp3|wav)$/i.test(file.name)) { say(T('Please choose an MP3 or WAV file')); pick.value = ''; return; }
    if (file.size > MAX_MB * 1024 * 1024) { say(T('That file is over {mb} MB', { mb: MAX_MB })); pick.value = ''; return; }
    load(file.name, function () { return readFile(file); });
  });

  // The demo plays at most the first 75 s of a track (fading out over 50 ms where it is cut), then 1.2 s of silence before
  // it loops. The engine (.wasm, web/houzy_web.cpp) enforces the same 75 s on its own: after 75 s of music without a 1 s
  // pause it goes silent, so a longer track fed to it from the console does not get past the limit either.
  var DEMO_S = 75, GAP_S = 1.2;
  function demoCut(buf) {
    var sr = buf.sampleRate, n = Math.min(buf.length, Math.round(DEMO_S * sr)), fade = Math.round(0.05 * sr);
    var out = ctx.createBuffer(buf.numberOfChannels, n + Math.round(GAP_S * sr), sr);
    for (var c = 0; c < buf.numberOfChannels; c++) {
      var d = out.getChannelData(c);
      d.set(buf.getChannelData(c).subarray(0, n));
      if (buf.length > n) for (var i = 0; i < fade; i++) d[n - 1 - i] *= i / fade;   // cut mid-track: fade, no click
    }
    return out;
  }

  // one path for an uploaded file: engine, bytes, decode, play
  async function load(name, getBytes) {
    clearTrack(true);
    try {
      await startEngine();
      var bytes = await getBytes();
      progress(null, T('Decoding {name}…', { name: name }));
      var full = await ctx.decodeAudioData(bytes);
      bytes = null;
      track = demoCut(full);
      nameEl.textContent = name + ' — ' + (full.duration > DEMO_S ? T('demo: first 1:15 of {len}', { len: fmt(full.duration) }) : T('demo'));
      full = null;                 // only the 75 s demo stays in memory
      prog.hidden = true;
      ui.classList.add('has-track'); row.hidden = false;
      offset = 0; showTime();
      play();
    } catch (e) {
      prog.hidden = true;
      say(/decode|Decod/i.test(String(e)) ? T('This file could not be decoded') : T('Could not start: {msg}', { msg: e.message }), 4000);
      clearTrack(true);
    }
  }

  // phones: a big AMOUNT slider, in step with the knob on the panel
  var amt = $('.live-amount input'), amtOut = $('.live-amount output');
  if (amt) {
    amt.addEventListener('input', function () { amtOut.textContent = (+amt.value).toFixed(0); if (window.houzySetKnob) window.houzySetKnob('amount', +amt.value); });
    document.addEventListener('houzy-amount', function (e) { if (document.activeElement !== amt) { amt.value = e.detail; amtOut.textContent = (+e.detail).toFixed(0); } });
  }

  function now() { return playing ? (offset + ctx.currentTime - startedAt) % track.duration : offset; }
  function play() {
    if (!track || playing) return;
    ctx.resume();
    src = ctx.createBufferSource(); src.buffer = track; src.loop = true;
    src.connect(node); src.start(0, offset);
    startedAt = ctx.currentTime; playing = window.houzyPlaying = true; playBtn.textContent = T('Pause');   // site.js: UI sounds stay quiet while a track plays
    tick();
  }
  function pause() {
    if (!playing) return;
    offset = now(); src.stop(); src.disconnect(); src = null; playing = window.houzyPlaying = false; playBtn.textContent = T('Play');
  }
  function seek(t) { var was = playing; pause(); offset = Math.max(0, Math.min(t, track.duration - 0.01)); showTime(); if (was) play(); }
  function fmt(s) { s = Math.max(0, s | 0); return (s / 60 | 0) + ':' + ('0' + s % 60).slice(-2); }
  function showTime() {
    if (!track) return;
    var t = now(), len = track.duration - GAP_S;   // the 1.2 s gap before the loop is not part of the shown length
    barFill.style.width = (t / track.duration * 100) + '%'; timeEl.textContent = fmt(Math.min(t, len)) + ' / ' + fmt(len);
  }
  function tick() { if (!playing) return; showTime(); requestAnimationFrame(tick); }

  // Clear: stop, drop every reference to the audio so the browser frees it; nothing was ever stored
  function clearTrack(silent) {
    if (src) { try { src.stop(); } catch (e) {} src.disconnect(); src = null; }
    playing = window.houzyPlaying = false; track = null; offset = 0; pick.value = '';
    ui.classList.remove('has-track'); row.hidden = true; nameEl.textContent = '';
    playBtn.textContent = T('Play'); barFill.style.width = '0%';
    if (!silent) say(T('Track removed from this page. It was never uploaded anywhere.'), 3200);
  }

  playBtn.addEventListener('click', function () { playing ? pause() : play(); });
  abBtn.addEventListener('click', function () { setBypass(!bypass); });
  clearBtn.addEventListener('click', function () { clearTrack(false); });
  bar.addEventListener('pointerdown', function (e) {
    if (!track) return;
    var r = bar.getBoundingClientRect(); seek((e.clientX - r.left) / r.width * track.duration);
  });
  document.addEventListener('keydown', function (e) {
    if (!track || /INPUT|TEXTAREA|BUTTON/.test(document.activeElement.tagName)) return;
    if (e.code === 'Space') { e.preventDefault(); playing ? pause() : play(); }
  });
})();
