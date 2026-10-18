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
  // 18.10.2026: Download and Manual have their own address per language (/ru/download.html ...), like the home page, and
  // each file holds ONE language (they used to hold four, picked by #ru / #es / #pt-BR - not indexable).
  var ownPage = /(download|manual|why-houzy|plugin)\.html$/.test(base);
  var pageKey = /plugin\.html$/.test(base) ? 'plugin' : ownPage ? 'own' : 'index';   // no "own" head in the files: the page keeps its title
  function urlOf(l) { return l === 'en' ? base : '/' + l + (base === '/' ? '/' : base); }
  var indexOf = function (l) { return LANGS.findIndex(function (x) { return x[0] === l; }); };
  // the old addresses keep working: download.html#ru (the UPDATE badge of HOUZY 5.0.0 opens these) -> /ru/download.html
  var oldHash = ownPage && cur === 'en' && /^#(ru|es|pt)(-BR)?$/.exec(location.hash);
  if (oldHash) { location.replace(urlOf(oldHash[1])); return; }

  // ---- 18.10.2026: a visitor whose BROWSER language the site has (ru / es / pt), on an English address, is OFFERED that
  // version in a small card - not redirected (Google's advice: shared links and crawlers keep the address they asked for).
  // "Open" and "x" both remember the answer, as does a pick in the language menu; the card never comes back after that.
  var KEY = 'houzy-lang';
  var saved = null; try { saved = localStorage.getItem(KEY); } catch (e) {}
  var offer = null;
  if (cur === 'en' && !saved && !navigator.webdriver && !/bot|crawl|spider|slurp|lighthouse/i.test(navigator.userAgent)) {
    offer = (navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language || 'en'])
      .map(function (x) { return String(x).slice(0, 2).toLowerCase(); })
      .filter(function (x) { return indexOf(x) >= 0; })[0];
    if (offer === 'en') offer = null;
  }
  function remember(l) { try { localStorage.setItem(KEY, l); } catch (e) {} }
  if (offer) {
    var OFFER = { ru: ['Этот сайт есть на русском.', 'Открыть по-русски', 'Закрыть'],
      es: ['Este sitio está en español.', 'Ver en español', 'Cerrar'],
      pt: ['Este site está em português.', 'Ver em português', 'Fechar'] }[offer];
    var card = document.createElement('div'); card.className = 'lang-offer glass'; card.lang = offer === 'pt' ? 'pt-BR' : offer;
    card.setAttribute('role', 'region'); card.setAttribute('aria-label', OFFER[0]);
    var msg = document.createElement('span'); msg.textContent = OFFER[0];
    var go = document.createElement('a'); go.className = 'cta'; go.href = urlOf(offer) + (ownPage ? '' : location.hash); go.textContent = OFFER[1];
    go.addEventListener('click', function () { remember(offer); });
    var x = document.createElement('button'); x.type = 'button'; x.className = 'lang-offer-x'; x.setAttribute('aria-label', OFFER[2]); x.textContent = '×';
    x.addEventListener('click', function () { remember('en'); card.remove(); });
    card.appendChild(msg); card.appendChild(go); card.appendChild(x);
    document.body.appendChild(card);
  }

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
  var links = LANGS.map(function (l) {
    var a = document.createElement('a'); a.href = urlOf(l[0]); a.lang = l[0]; a.hreflang = l[0];
    a.innerHTML = '<b>' + l[1] + '</b>' + l[2];
    menu.appendChild(a); return a;
  });
  nav.appendChild(box);   // the last column of the bar: the right corner (18.10.2026, Buy / Download left the header)
  // an old stylesheet from the browser cache (a page cached before this switch existed) has no rules for the menu, and the four
  // languages would spill into the header as a row of text: hide the switch and fetch the current stylesheet.
  if (getComputedStyle(menu).flexDirection !== 'column') {
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
  function load(l) { return files[l] || (files[l] = fetch('/i18n/' + l + '.json?v=20261018i').then(function (r) { return r.json(); })); }
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
    remember(a.lang);                                   // the visitor's own choice: the offer card never shows again
    var oc = document.querySelector('.lang-offer'); if (oc) oc.remove();
    if (ownPage) return;                                // one language per file: the link opens that file
    e.preventDefault();
    open(false);
    if (a.lang !== cur) setLang(a.lang, true);
  });
  window.addEventListener('popstate', function () {
    var m = /^\/(ru|es|pt)(?=\/|$)/.exec(location.pathname), l = m ? m[1] : 'en';
    if (l !== cur) setLang(l, false);
  });
})();
