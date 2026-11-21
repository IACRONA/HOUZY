// PRICES (21.11.2026, user): every Buy key opens this window over the page instead of Gumroad - three Liquid Glass
// cards: ACR LIMITER, the bundle (in the middle, the best value), HOUZY. Each card's key goes to its own Gumroad page.
// Black Friday until 27 Nov 2026 (inclusive): bundle $55, HOUZY $38 with BLACKFRIDAY45; from 28 Nov the regular prices.
(function () {
  // only the plugins' own Buy keys: not Become a partner (/affiliates) nor the 4 -> 5 upgrade (/houzy/HOUZY4FRIENDS)
  var keys = [].slice.call(document.querySelectorAll('.cta.buy[href$="/l/houzy"], .cta.buy[href$="/l/acr-limiter"]'));
  if (!keys.length) return;
  var ru = (document.documentElement.lang || '').slice(0, 2) === 'ru';
  var BF = Date.now() < Date.UTC(2026, 10, 28, 8);   // until 27 Nov, end of day in the Americas
  var T = ru ? {
    title: 'Выберите плагин', buy: 'Купить', best: 'Выгоднее всего', bf: 'Black Friday · до 27 ноября',
    acr: ['ACR LIMITER', 'Лимитер для каждой дорожки и шины: громкость без смены тона.', ['Клиппер ACR — импульс вместо среза', 'Лимитер T8 / T9 — бас и бочка без хруста', 'TRUE PEAK по умолчанию', 'Лёгкий: на любое число дорожек']],
    both: ['HOUZY + ACR LIMITER', 'HOUZY на мастер, ACR LIMITER на каждую дорожку и шину.', ['Оба плагина, две лицензии', 'Ровная компрессия мастера', 'Громкость и потолок на любой дорожке', 'Экономия против покупки по отдельности']],
    hz: ['HOUZY', 'Мастеринг-компрессор: ровное сжатие по всем частотам.', ['Сжатие по каждому тону, без пампинга', 'Клиппер ACR и лимитер T8 внутри', 'CYCLES / NOTE — атака в периодах волны', 'VST3, AU — Windows и macOS']],
    code: 'с промокодом ', fresh: 'Новый плагин · версия 1.0',
    trust: 'Лицензия навсегда на 2 компьютера · 14 дней бесплатно · возврат 14 дней · оплата через Gumroad', close: 'Закрыть'
  } : {
    title: 'Choose your plugin', buy: 'Buy', best: 'Best value', bf: 'Black Friday · until 27 November',
    acr: ['ACR LIMITER', 'A limiter for every track and bus: loudness that keeps the tone.', ['ACR clipper: a pulse instead of a slice', 'T8 / T9 limiter: bass and kick without grit', 'TRUE PEAK on by default', 'Light on CPU: use it on any number of tracks']],
    both: ['HOUZY + ACR LIMITER', 'HOUZY for the master, ACR LIMITER for every track and bus.', ['Both plugins, two licences', 'Even compression on the master', 'Loudness and ceiling on any track', 'Saves against buying them separately']],
    hz: ['HOUZY', 'Mastering compressor: even compression across every frequency.', ['Compression per tone, no pumping', 'ACR clipper and T8 limiter inside', 'CYCLES / NOTE: attack in wave cycles', 'VST3, AU — Windows and macOS']],
    code: 'with code ', fresh: 'New plugin · version 1.0',
    trust: 'Lifetime licence for 2 computers · 14 days free · 14-day refund · secure payment on Gumroad', close: 'Close'
  };
  var G = 'https://acronawar.gumroad.com/l/';
  var cards = [
    { t: T.acr, price: BF ? '$29' : '$39', was: BF ? '$39' : '', note: BF ? T.bf + ' · ' + T.code + 'BlackFridayL' : T.fresh, href: G + 'acr-limiter' + (BF ? '/BlackFridayL' : '') },   // user 21.11.2026: $39, Black Friday $29
    { t: T.both, price: BF ? '$55' : '$79', was: '$108', note: BF ? T.bf : '', tag: BF ? '−49%' : '−27%', best: true, href: G + 'houzy-acrlimiter' },   // $108 = $39 + $69
    { t: T.hz, price: BF ? '$38' : '$69', was: BF ? '$69' : '', note: BF ? T.bf + ' · ' + T.code + 'BLACKFRIDAY45' : '', href: G + 'houzy' + (BF ? '/BLACKFRIDAY45' : '') }
  ];
  var esc = function (s) { return String(s).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); };
  var box = document.createElement('div');
  box.className = 'pm'; box.hidden = true; box.setAttribute('role', 'dialog'); box.setAttribute('aria-modal', 'true'); box.setAttribute('aria-label', T.title);
  box.innerHTML = '<div class="pm-back"></div><div class="pm-wrap"><button class="pm-x" type="button" aria-label="' + esc(T.close) + '">×</button>' +
    '<p class="pm-title">' + esc(T.title) + '</p><div class="pm-grid">' + cards.map(function (c) {
      return '<article class="glass pm-card' + (c.best ? ' pm-best' : '') + '">' + (c.best ? '<span class="pm-badge">' + esc(T.best) + '</span>' : '') +
        '<h3>' + esc(c.t[0]) + '</h3><p class="pm-lead">' + esc(c.t[1]) + '</p>' +
        '<p class="pm-price">' + (c.was ? '<s>' + c.was + '</s>' : '') + '<b>' + c.price + '</b>' + (c.tag ? '<span class="pm-off">' + c.tag + '</span>' : '') + '</p>' +
        '<p class="pm-note">' + esc(c.note) + '</p>' +   // always there: the cards' rows line up (CSS subgrid)
        '<ul>' + c.t[2].map(function (x) { return '<li>' + esc(x) + '</li>'; }).join('') + '</ul>' +
        '<a class="cta pm-buy" href="' + c.href + '" target="_blank" rel="noopener">' + esc(T.buy) + '</a></article>';
    }).join('') + '</div><p class="pm-trust">' + esc(T.trust) + '</p></div>';
  document.body.appendChild(box);
  var last = null;
  function open(e) { e.preventDefault(); e.stopPropagation(); last = document.activeElement; box.hidden = false; requestAnimationFrame(function () { box.classList.add('on'); }); box.querySelector('.pm-x').focus(); document.documentElement.classList.add('pm-lock'); }
  function close() { box.classList.remove('on'); document.documentElement.classList.remove('pm-lock'); setTimeout(function () { if (!box.classList.contains('on')) box.hidden = true; }, 420); if (last && last.focus) last.focus(); }
  keys.forEach(function (k) { k.addEventListener('click', open, true); });
  box.querySelector('.pm-back').addEventListener('click', close);
  box.querySelector('.pm-x').addEventListener('click', close);
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape' && !box.hidden) close(); });
})();
