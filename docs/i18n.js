// HOUZY site: the language switch. One glass button with the current code (EN / RU / ES / PT); a click opens a menu with the seven
// languages (fades and grows in from the button, silent - site.js leaves .lang out of the click sound).
// A click on a language swaps the text of the page IN PLACE (no reload), moves the address to /ru/ /es/ /pt/ /fr/ /it/ /ja/ /de/ /pl/ /zh/ /th/ (the
// real, indexed pages) and back; every code is also a plain link to its page, so the switch works without scripts.
// Text blocks carry data-i18n="N" (index into the language file's "t"), attributes data-i18n-a="aria-label:N,title:M".
(function () {
  var LANGS = [['en', 'EN', 'English'], ['ru', 'RU', 'Русский'], ['es', 'ES', 'Español'], ['pt', 'PT', 'Português'], ['fr', 'FR', 'Français'], ['it', 'IT', 'Italiano'], ['ja', 'JA', '日本語'], ['de', 'DE', 'Deutsch'], ['pl', 'PL', 'Polski'], ['zh', 'ZH', '简体中文'], ['th', 'TH', 'ไทย']];
  var nav = document.querySelector('.navpanel');
  if (!nav || !document.querySelector('[data-i18n]')) return;
  var T = function (s) { return window.houzyT ? window.houzyT(s) : s; };
  var cur = (document.documentElement.lang || 'en').slice(0, 2);
  if (!LANGS.some(function (l) { return l[0] === cur; })) cur = 'en';

  // /ru/plugin.html -> /plugin.html ; /ru/ -> /
  var base = location.pathname.replace(/^\/(ru|es|pt|fr|it|ja|de|pl|zh|th)(?=\/|$)/, '') || '/';
  // 18.10.2026: Download and Manual have their own address per language (/ru/download.html ...), like the home page, and
  // each file holds ONE language (they used to hold four, picked by #ru / #es / #pt-BR - not indexable).
  var ownPage = /(download|manual|why-houzy|affiliates|plugin|upgrade|best-mastering-compressor|houzy-5-2).html$/.test(base);
  var pageKey = /plugin\.html$/.test(base) ? 'plugin' : ownPage ? 'own' : 'index';   // no "own" head in the files: the page keeps its title
  function urlOf(l) {
    if (/houzy-5-2\.html$/.test(base) && ['en', 'ru', 'es', 'pt'].indexOf(l) < 0) return base;   // the release page is in four languages, the rest read English
    return l === 'en' ? base : '/' + l + (base === '/' ? '/' : base);
  }
  var indexOf = function (l) { return LANGS.findIndex(function (x) { return x[0] === l; }); };
  // the old addresses keep working: download.html#ru (the UPDATE badge of HOUZY 5.0.0 opens these) -> /ru/download.html
  var oldHash = ownPage && cur === 'en' && /^#(ru|es|pt|fr|it|ja|de|pl|zh|th)(-BR)?$/.exec(location.hash);
  if (oldHash) { location.replace(urlOf(oldHash[1])); return; }

  // ---- 18.10.2026 (owner): no automatic language at all - no redirect, no offer card. The visitor picks a language with the
  // button in the header; that pick is remembered (houzy-lang) only for the visitor's own use of the menu.
  var KEY = 'houzy-lang';
  function remember(l) { try { localStorage.setItem(KEY, l); } catch (e) {} }

  // ---- the control: one button with the current code; a click opens a glass menu with the four languages
  var box = document.createElement('div'); box.className = 'lang';
  var btn = document.createElement('button'); btn.type = 'button'; btn.className = 'lang-btn';
  btn.setAttribute('aria-haspopup', 'true'); btn.setAttribute('aria-expanded', 'false');
  btn.innerHTML = '<svg class="lang-globe" viewBox="0 0 16 16" aria-hidden="true"><circle cx="8" cy="8" r="6.3"/><path d="M1.7 8h12.6M8 1.7c1.9 1.9 2.8 4 2.8 6.3S9.9 12.4 8 14.3C6.1 12.4 5.2 10.3 5.2 8S6.1 3.6 8 1.7z"/></svg>';
  var code = document.createElement('span'); code.className = 'lang-code'; btn.appendChild(code);
  var name = document.createElement('span'); name.className = 'lang-name'; btn.appendChild(name);   // phones: the full name
  var chev = document.createElement('i'); chev.className = 'lang-chev'; chev.setAttribute('aria-hidden', 'true'); btn.appendChild(chev);
  var menu = document.createElement('div'); menu.className = 'lang-menu'; menu.setAttribute('role', 'group');
  box.appendChild(btn); box.appendChild(menu);
  // small flags right of the names (12.11.2026, user): inline SVG - emoji flags show as letters on Windows
  var r = function (x, y, w, h, c) { return '<rect x="' + x + '" y="' + y + '" width="' + w + '" height="' + h + '" fill="' + c + '"/>'; };
  var FLAGS = {
    en: '<rect width="60" height="30" fill="#012169"/><path d="M0 0l60 30M60 0L0 30" stroke="#fff" stroke-width="6"/><path d="M0 0l60 30M60 0L0 30" stroke="#C8102E" stroke-width="2"/><path d="M30 0v30M0 15h60" stroke="#fff" stroke-width="10"/><path d="M30 0v30M0 15h60" stroke="#C8102E" stroke-width="6"/>',
    ru: r(0, 0, 3, 2, '#fff') + r(0, .667, 3, .667, '#0039A6') + r(0, 1.333, 3, .667, '#D52B1E'),
    es: r(0, 0, 3, 2, '#AA151B') + r(0, .5, 3, 1, '#F1BF00'),
    pt: r(0, 0, 3, 2, '#FF0000') + r(0, 0, 1.2, 2, '#006600') + '<circle cx="1.2" cy="1" r=".4" fill="#FFCC00"/>',
    fr: r(0, 0, 1, 2, '#0055A4') + r(1, 0, 1, 2, '#fff') + r(2, 0, 1, 2, '#EF4135'),
    it: r(0, 0, 1, 2, '#009246') + r(1, 0, 1, 2, '#fff') + r(2, 0, 1, 2, '#CE2B37'),
    ja: r(0, 0, 3, 2, '#fff') + '<circle cx="1.5" cy="1" r=".6" fill="#BC002D"/>',
    de: r(0, 0, 3, .667, '#000') + r(0, .667, 3, .667, '#DD0000') + r(0, 1.333, 3, .667, '#FFCE00'),
    pl: r(0, 0, 3, 1, '#fff') + r(0, 1, 3, 1, '#DC143C'),
    zh: r(0, 0, 3, 2, '#EE1C25') + '<polygon fill="#FFDE00" points=".5,.2 .568,.407 .785,.407 .609,.536 .676,.743 .5,.615 .324,.743 .391,.536 .215,.407 .432,.407"/>',
    th: r(0, 0, 3, 2, '#A51931') + r(0, .333, 3, 1.333, '#F4F5F8') + r(0, .667, 3, .667, '#2D2A4A')
  };
  var links = LANGS.map(function (l) {
    var a = document.createElement('a'); a.href = urlOf(l[0]); a.lang = l[0]; a.hreflang = l[0];
    a.innerHTML = '<b>' + l[1] + '</b>' + l[2] + '<svg class="lang-flag" viewBox="' + (l[0] === 'en' ? '0 0 60 30' : '0 0 3 2') +
      '" preserveAspectRatio="none" aria-hidden="true">' + FLAGS[l[0]] + '</svg>';
    menu.appendChild(a); return a;
  });
  nav.appendChild(box);   // the last column of the bar: the right corner (18.10.2026, Buy / Download left the header)
  // an old stylesheet from the browser cache (a page cached before this switch existed) has no rules for the menu, and the seven
  // languages would spill into the header as a row of text: hide the switch and fetch the current stylesheet.
  if (getComputedStyle(menu).display !== 'grid') {
    box.style.visibility = 'hidden';
    var css = document.createElement('link'); css.rel = 'stylesheet'; css.href = '/site.css?v=' + Date.now();
    css.onload = function () { box.style.visibility = ''; }; document.head.appendChild(css);
  }
  function mark(l) {
    code.textContent = LANGS[indexOf(l)][1];
    name.textContent = LANGS[indexOf(l)][2];
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
  function load(l) { return files[l] || (files[l] = fetch('/i18n/' + l + '.json?v=20261112a').then(function (r) { return r.json(); })); }
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
      a.setAttribute('href', (l === 'en' ? '' : '/' + l) + '/manual.html');
    });
    [].forEach.call(document.querySelectorAll('a[href$="download.html"]'), function (a) {   // its own address per language too
      a.setAttribute('href', (l === 'en' ? '' : '/' + l) + '/download.html');
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
    window.dispatchEvent(new CustomEvent('houzy:lang', { detail: l }));   // download.js: its own words
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
    remember(a.lang);                                   // the visitor's own choice
    if (ownPage) return;                                // one language per file: the link opens that file
    e.preventDefault();
    open(false);
    if (a.lang !== cur) setLang(a.lang, true);
  });
  window.addEventListener('popstate', function () {
    var m = /^\/(ru|es|pt|fr|it|ja|de|pl|zh|th)(?=\/|$)/.exec(location.pathname), l = m ? m[1] : 'en';
    if (l !== cur) setLang(l, false);
  });
})();
