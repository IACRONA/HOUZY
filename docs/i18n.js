// HOUZY site: the language switch. One glass button with the current code (EN / RU / ES / PT); a click opens a menu with the four
// languages (fades and grows in from the button, silent - site.js leaves .lang out of the click sound).
// A click on a language swaps the text of the page IN PLACE (no reload), moves the address to /ru/ /es/ /pt/ (the
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

  // ---- the control: one button with the current code; a click opens a glass menu with the four languages
  var box = document.createElement('div'); box.className = 'lang';
  var btn = document.createElement('button'); btn.type = 'button'; btn.className = 'lang-btn';
  btn.setAttribute('aria-haspopup', 'true'); btn.setAttribute('aria-expanded', 'false');
  var code = document.createElement('span'); btn.appendChild(code);
  var chev = document.createElement('i'); chev.className = 'lang-chev'; chev.setAttribute('aria-hidden', 'true'); btn.appendChild(chev);
  var menu = document.createElement('div'); menu.className = 'lang-menu'; menu.setAttribute('role', 'group');
  box.appendChild(btn); box.appendChild(menu);
  var links = LANGS.map(function (l) {
    var a = document.createElement('a'); a.href = urlOf(l[0]); a.lang = l[0]; a.hreflang = l[0];
    a.innerHTML = '<b>' + l[1] + '</b>' + l[2];
    menu.appendChild(a); return a;
  });
  nav.insertBefore(box, nav.querySelector('.buy') || null);
  // an old stylesheet from the browser cache (a page cached before this switch existed) has no rules for the menu, and the four
  // languages would spill into the header as a row of text: hide the switch and fetch the current stylesheet.
  if (getComputedStyle(menu).flexDirection !== 'column') {
    box.style.visibility = 'hidden';
    var css = document.createElement('link'); css.rel = 'stylesheet'; css.href = '/site.css?v=' + Date.now();
    css.onload = function () { box.style.visibility = ''; }; document.head.appendChild(css);
  }
  function mark(l) {
    code.textContent = LANGS[indexOf(l)][1];
    btn.setAttribute('aria-label', T('Language') + ': ' + LANGS[indexOf(l)][2]); menu.setAttribute('aria-label', T('Language'));
    links.forEach(function (a) { a.setAttribute('aria-current', a.lang === l ? 'true' : 'false'); });
  }
  function open(on) { box.classList.toggle('open', on); btn.setAttribute('aria-expanded', on ? 'true' : 'false'); }
  btn.addEventListener('click', function () { open(!box.classList.contains('open')); });
  document.addEventListener('click', function (e) { if (!box.contains(e.target)) open(false); });
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape' && box.classList.contains('open')) { open(false); btn.focus(); } });
  mark(cur);

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
    mark(l);
    // the words changed length: everything that measures them (the header's drop, the wordmark, the layout) re-measures
    window.dispatchEvent(new Event('resize'));
  }
  var busy = 0;
  function setLang(l, push) {
    var ticket = ++busy;
    mark(l);                                              // the button and the tick change at once, the text follows when the file is there
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
    open(false);
    if (a.lang !== cur) setLang(a.lang, true);
  });
  window.addEventListener('popstate', function () {
    var m = /^\/(ru|es|pt)(?=\/|$)/.exec(location.pathname), l = m ? m[1] : 'en';
    if (l !== cur) setLang(l, false);
  });
})();
