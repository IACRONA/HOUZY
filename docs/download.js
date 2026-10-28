// Download page (17.10.2026): everything about releases comes from /version.json - the same file the plugin's
// UPDATE badge reads. Releasing = upload the files, put the new release FIRST in "releases", keep the last three.
// Five files per release (win .exe, win_vst3 .zip, mac .pkg, mac_vst3 .zip, mac_au .zip); each key takes the newest
// release that HAS that file (macOS can ship later than Windows).
// ONE language per file since 18.10.2026 (/download.html, /ru/ /es/ /pt/download.html): the page language is <html lang>.
(function () {
  var TXT = {
    en: ['What\'s new', 'Latest version', 'Download', 'Installer', 'Download HOUZY', 'Coming soon'],
    ru: ['Что нового', 'Последняя версия', 'Скачать', 'Установщик', 'Скачать HOUZY', 'Скоро'],
    es: ['Novedades', 'Última versión', 'Descargar', 'Instalador', 'Descargar HOUZY', 'Próximamente'],
    pt: ['Novidades', 'Última versão', 'Baixar', 'Instalador', 'Baixar HOUZY', 'Em breve'],
    fr: ['Nouveautés', 'Dernière version', 'Télécharger', 'Installateur', 'Télécharger HOUZY', 'Bientôt'],
    it: ['Novità', 'Ultima versione', 'Scarica', 'Installer', 'Scarica HOUZY', 'Prossimamente'],
    ja: ['新機能', '最新バージョン', 'ダウンロード', 'インストーラー', 'HOUZYをダウンロード', '近日公開']
  };
  var L = (document.documentElement.lang || 'en').slice(0, 2), data = null;   // 18.10.2026: one language per file (/ru/download.html)
  if (!TXT[L]) L = 'en';

  function el(tag, cls, text) { var e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; }
  function date(d) { var p = String(d || '').split('-'); return p.length === 3 ? p[2] + '.' + p[1] + '.' + p[0] : ''; }

  function render() {
    var T = TXT[L];
    var title = document.getElementById('dl-title'); if (title) title.textContent = T[4];
    // macOS is not out yet: its key says so in the page language (no file in version.json = no link at all)
    [].forEach.call(document.querySelectorAll('[data-soon]'), function (e) { e.textContent = T[5]; });
    if (!data) return;
    var rel = (data.releases || []).slice(0, 3);
    if (!rel.length) return;
    // [field in a release, short name for older releases, key label]
    var FILES = [['win', 'Windows .exe', T[3] + ' · .exe'], ['win_vst3', 'Windows VST3', 'VST3 · .zip'],
                 ['mac', 'macOS .pkg', T[3] + ' · .pkg'], ['mac_vst3', 'macOS VST3', 'VST3 · .zip'], ['mac_au', 'macOS AU', 'AU · .zip']];
    FILES.forEach(function (f) {
      var key = document.querySelector('[data-file="' + f[0] + '"]'), r = rel.filter(function (x) { return x[f[0]]; })[0];
      if (!key) return;
      key.textContent = f[2] + (r && r !== rel[0] ? ' · v' + r.version : '');   // the version only when this file lags
      if (r) { key.href = r[f[0]]; key.removeAttribute('aria-disabled'); }
    });
    var ver = document.getElementById('dl-ver');
    if (ver) ver.textContent = T[1] + ' ' + rel[0].version + ' · ' + date(rel[0].date);

    var box = document.getElementById('whatsnew');
    if (!box) return;
    box.textContent = '';
    box.appendChild(el('h2', '', T[0]));
    rel.forEach(function (r, i) {
      var n = (r.notes && (r.notes[L] || r.notes.en)) || '';
      box.appendChild(el('h3', '', 'v' + r.version + (r.date ? ' · ' + date(r.date) : '')));
      var ul = el('ul');
      n.split('\n').forEach(function (line) { if (line.trim()) ul.appendChild(el('li', '', line)); });
      box.appendChild(ul);
      if (i > 0) {   // older releases: their own files, in case the newest one does not suit
        var p = el('p', 'meta');
        FILES.forEach(function (f) {
          if (!r[f[0]]) return;
          var a = el('a', '', f[1]); a.href = r[f[0]];
          if (p.childNodes.length) p.appendChild(document.createTextNode('  ·  '));
          p.appendChild(a);
        });
        if (p.childNodes.length) { p.insertBefore(document.createTextNode(T[2] + ' v' + r.version + ':  '), p.firstChild); box.appendChild(p); }
      }
    });
    box.hidden = false;
  }

  window.addEventListener('houzy:lang', function (e) { L = TXT[e.detail] ? e.detail : 'en'; render(); });
  render();
  fetch('/version.json?t=' + Date.now()).then(function (r) { return r.json(); })
    .then(function (v) { data = v; render(); })
    .catch(function () {});   // no file: the page keeps its placeholders
})();
