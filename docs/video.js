// HOUZY site: the video player - Plyr 3.7.8 (MIT, /vendor/plyr). Its icons and blank video default to cdn.plyr.io, which the
// site's CSP does not allow: both point to our own files. The video plays at its own resolution (no quality switching).
(function () {
  var v = document.getElementById('houzy-video');
  if (!v || !window.Plyr) return;   // no script: the browser's own <video controls> still works
  new window.Plyr(v, {
    iconUrl: '/vendor/plyr/plyr.svg',
    blankVideo: '',
    muted: false,
    volume: 0.5,   // 50 % by default (owner)
    storage: { enabled: false },   // sound on every time: no remembered mute / volume
    ratio: '16:9',
    controls: ['play-large', 'play', 'progress', 'current-time', 'mute', 'volume', 'fullscreen'],
    tooltips: { controls: false, seek: true }
  });
})();
