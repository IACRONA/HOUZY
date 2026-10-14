// HOUZY site: the language switch. EN | RU | ES | PT - a drop of liquid glass flows between the codes (the same motion as the
// header's selection and the plugin's switches: the leading edge runs ahead, the trailing one follows, the drop thins across
// while it is pulled out). A click swaps the text of the page IN PLACE (no reload), moves the address to /ru/ /es/ /pt/ (the
// real, indexed pages) and back; every code is also a plain link to its page, so the switch works without scripts.
// Text blocks carry data-i18n="N" (index into the language file's "t"), attributes data-i18n-a="aria-label:N,title:M".
(function () {
  var LANGS = [['en', 'EN', 'English'], ['ru', 'RU', 'Русский'], ['es', 'ES', 'Español'], ['pt', 'PT', 'Português']];
  var nav = document.querySelector('.navpanel');
  if (!nav || !document.querySelector('[data-i18n]')) return;
  var T = function (s) { return window.houzyT ? window.houzyT(s) : s; };
  var cur = (document.documentElement.lang || 'en').slice(0, 2);
  if (!LANGS.some(function (l) { return l[0] === cur; })) cur = 'en';

  // /ru/plugin.html -> /plugin.html ; /ru/ -> /
  var base = location.pathname.replace(/^\/(ru|es|pt)(?=\/|$)/, '') || '/';
  var pageKey = /plugin\.html$/.test(base) ? 'plugin' : 'index';
  function urlOf(l) { return l === 'en' ? base : '/' + l + (base === '/' ? '/' : base); }
  var indexOf = function (l) { return LANGS.findIndex(function (x) { return x[0] === l; }); };

  // ---- the control
  var box = document.createElement('div'); box.className = 'lang'; box.setAttribute('role', 'group'); box.setAttribute('aria-label', T('Language'));
  var drop = document.createElement('i'); drop.className = 'lang-drop'; drop.setAttribute('aria-hidden', 'true'); box.appendChild(drop);
  var links = LANGS.map(function (l) {
    var a = document.createElement('a'); a.href = urlOf(l[0]); a.textContent = l[1]; a.lang = l[0]; a.hreflang = l[0]; a.title = l[2];
    a.setAttribute('aria-current', l[0] === cur ? 'true' : 'false');
    box.appendChild(a); return a;
  });
  nav.insertBefore(box, nav.querySelector('.buy') || null);

  // ---- the drop
  var L = 0, R = 0, tL = 0, tR = 0, tTop = 0, tH = 0, started = false, running = false, last = 0;
  function place(i) {
    var b = box.getBoundingClientRect(), r = links[i].getBoundingClientRect();
    tL = r.left - b.left; tR = r.right - b.left; tTop = r.top - b.top; tH = r.height;
  }
  function draw() {
    var w = R - L, tw = Math.max(1, tR - tL), stretch = Math.min(1, Math.max(0, (w - tw) / (tw * 1.2))), h = tH * (1 - 0.28 * stretch);
    drop.style.width = w.toFixed(1) + 'px'; drop.style.height = h.toFixed(1) + 'px';
    drop.style.transform = 'translate(' + L.toFixed(1) + 'px,' + (tTop + (tH - h) / 2).toFixed(1) + 'px)';
  }
  function frame(now) {
    var dt = last ? Math.min(0.05, (now - last) / 1000) : 0.016; last = now;
    var kLead = 1 - Math.exp(-dt / 0.045), kTrail = 1 - Math.exp(-dt / 0.13), right = (tL + tR) >= (L + R);
    R += (tR - R) * (right ? kLead : kTrail); L += (tL - L) * (right ? kTrail : kLead);
    draw();
    if (Math.abs(R - tR) < 0.15 && Math.abs(L - tL) < 0.15) { R = tR; L = tL; draw(); running = false; last = 0; return; }
    requestAnimationFrame(frame);
  }
  function go(i, instant) {
    place(i);
    if (instant || !started) { L = tL; R = tR; draw(); started = true; drop.classList.add('on'); }
    else if (!running) { running = true; last = 0; requestAnimationFrame(frame); }
  }
  function snap() { place(indexOf(cur)); L = tL; R = tR; draw(); }

  // ---- swapping the page
  var files = {};
  function load(l) { return files[l] || (files[l] = fetch('/i18n/' + l + '.json').then(function (r) { return r.json(); })); }
  function setMeta(sel, attr, v) { var e = document.querySelector(sel); if (e && v != null) e.setAttribute(attr, v); }
  function apply(l, data) {
    var t = data.t || [];
    [].forEach.call(document.querySelectorAll('[data-i18n]'), function (el) {
      var tr = t[+el.getAttribute('data-i18n')];
      if (!tr) return;
      if (el.hasAttribute('data-i18n-lead')) {            // a block with an icon after the words: only the leading text changes
        var n = el.firstChild;
        if (n && n.nodeType === 3) { var ws = /\s*$/.exec(n.nodeValue)[0]; n.nodeValue = tr + ws; }
      } else el.innerHTML = tr;
    });
    [].forEach.call(document.querySelectorAll('[data-i18n-a]'), function (el) {
      el.getAttribute('data-i18n-a').split(',').forEach(function (p) {
        var kv = p.split(':'), v = t[+kv[1]]; if (v) el.setAttribute(kv[0], v);
      });
    });
    var home = l === 'en' ? '/' : '/' + l + '/';
    [].forEach.call(document.querySelectorAll('[data-home]'), function (a) { a.setAttribute('href', home + a.getAttribute('data-home')); });
    [].forEach.call(document.querySelectorAll('[data-manual]'), function (a) {
      a.setAttribute('href', '/manual.html' + (l === 'en' ? '' : '#' + (l === 'pt' ? 'pt-BR' : l)));
    });
    var h = (data.head || {})[pageKey];
    if (h) {
      document.title = h.title;
      setMeta('meta[name="description"]', 'content', h.description);
      setMeta('meta[property="og:title"]', 'content', h.og_title);
      setMeta('meta[property="og:description"]', 'content', h.og_description);
    }
    var abs = location.origin + urlOf(l);
    setMeta('link[rel="canonical"]', 'href', abs); setMeta('meta[property="og:url"]', 'content', abs);
    document.documentElement.lang = l;
    if (window.houzyI18nSet) window.houzyI18nSet(data.js || {});
    box.setAttribute('aria-label', T('Language'));
    links.forEach(function (a, i) { a.setAttribute('aria-current', LANGS[i][0] === l ? 'true' : 'false'); });
    // the words changed length: everything that measures them (the header's drop, the wordmark, the layout) re-measures
    window.dispatchEvent(new Event('resize'));
  }
  var busy = 0;
  function setLang(l, push) {
    var ticket = ++busy;
    go(indexOf(l));                                       // the drop starts at once, the text follows when the file is there
    load(l).then(function (data) {
      if (ticket !== busy) return;                        // a newer click won
      cur = l; apply(l, data);
    }).catch(function () { location.href = urlOf(l); });  // no file (offline?): the real page
    if (push) history.pushState({ lang: l }, '', urlOf(l));
  }
  box.addEventListener('click', function (e) {
    var a = e.target.closest && e.target.closest('a');
    if (!a || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey || e.button) return;
    e.preventDefault();
    if (a.lang !== cur) setLang(a.lang, true);
  });
  window.addEventListener('popstate', function () {
    var m = /^\/(ru|es|pt)(?=\/|$)/.exec(location.pathname), l = m ? m[1] : 'en';
    if (l !== cur) setLang(l, false);
  });
  window.addEventListener('resize', function () { if (started) snap(); });
  if (document.fonts) document.fonts.ready.then(function () { go(indexOf(cur), true); });
  go(indexOf(cur), true);
})();
