"""py geo_apply.py : the AI-search pass (28.10.2026) on the hand-maintained pages (en, ru, es, pt): definition lead, visible
update date, footer socials, Atom feed link, Organization + ImageObject JSON-LD, 'What is HOUZY?' + FAQ (with FAQPage JSON-LD)
on Why HOUZY, og:image -> houzy-card.jpg. fr/it/ja get all of it from build.py (they are built from the EN pages). Idempotent."""
import re, json, os, glob
from lib import DOCS, SITE
from geo_strings import STR

rd = lambda p: open(p, encoding='utf-8').read()
def wr(p, s): open(p, 'w', encoding='utf-8', newline='').write(s)
LANGS = {'en': '', 'ru': '/ru', 'es': '/es', 'pt': '/pt'}
IMGNAME = {'en': 'HOUZY mastering compressor plugin window', 'ru': 'Окно плагина-компрессора HOUZY',
           'es': 'Ventana del plugin compresor de mastering HOUZY', 'pt': 'Janela do plugin compressor de masterização HOUZY'}
SOCIAL = ('        <li><a href="https://open.spotify.com/artist/7GqbuqTbfmOOr0QVFw1wj7" rel="noopener">Spotify</a></li>\n'
          '        <li><a href="https://music.yandex.ru/artist/5852436" rel="noopener">Yandex Music</a></li>\n      ')
FEED = '<link rel="alternate" type="application/atom+xml" title="HOUZY releases" href="/feed.xml">\n'

def ld(o): return '<script type="application/ld+json">\n' + json.dumps(o, ensure_ascii=False, indent=1) + '\n</script>\n'

def org(prefix):
    return {"@context": "https://schema.org", "@type": "Organization", "@id": SITE + prefix + "/#organization",
            "name": "ACRONA AUDIO", "alternateName": "ACRONA", "url": SITE + prefix + "/",
            "logo": {"@type": "ImageObject", "url": SITE + "/img/acrona-logo.jpg", "width": 512, "height": 512},
            "image": SITE + "/img/houzy-card.jpg",
            "sameAs": ["https://t.me/acronalife", "https://open.spotify.com/artist/7GqbuqTbfmOOr0QVFw1wj7",
                       "https://music.yandex.ru/artist/5852436", "https://acronawar.gumroad.com/"],
            "contactPoint": {"@type": "ContactPoint", "contactType": "customer support", "email": "acronaaudio@proton.me",
                             "availableLanguage": ["English", "Russian", "Spanish", "Portuguese", "French", "Italian", "Japanese"]}}

def imgobj(lang, prefix):
    return {"@context": "https://schema.org", "@type": "ImageObject", "contentUrl": SITE + "/img/houzy-card.jpg",
            "url": SITE + "/img/houzy-card.jpg", "width": 1640, "height": 912, "encodingFormat": "image/jpeg",
            "name": IMGNAME[lang], "creator": {"@id": SITE + prefix + "/#organization"}}

def faqld(S, lang):
    return {"@context": "https://schema.org", "@type": "FAQPage", "inLanguage": {'en': 'en', 'ru': 'ru', 'es': 'es', 'pt': 'pt-BR'}[lang],
            "mainEntity": [{"@type": "Question", "name": S['Q%d' % i], "acceptedAnswer": {"@type": "Answer", "text": S['A%d' % i]}} for i in range(1, 5)]}

def home(lang, s):
    S, prefix = STR[lang], LANGS[lang]
    s, n = re.subn(r'(<p class="lead" data-i18n="8">)(.*?)(</p>)', lambda m: m.group(1) + S['LEAD'] + m.group(3), s, 1, re.S); assert n, 'lead'
    if 'foot-updated' not in s:
        assert '<p class="copy"' in s; s = s.replace('<p class="copy"', '<p class="foot-updated" data-i18n="130">%s</p>\n    <p class="copy"' % S['FOOT'], 1)
    i = s.find('<h2>HOUZY</h2>')
    if i > 0 and 'open.spotify.com' not in s[i:i + 900]:
        j = s.find('</ul>', i); s = s[:j].rstrip(' ') + SOCIAL + s[j:]
    if 'atom+xml' not in s: s = s.replace('<meta name="robots"', FEED + '<meta name="robots"', 1)
    if '#organization' not in s: s = s.replace('</head>', ld(org(prefix)) + ld(imgobj(lang, prefix)) + '</head>', 1)
    return s

def why(lang, s):
    S, prefix = STR[lang], LANGS[lang]
    if 'why-upd' not in s:
        s, n = re.subn(r'(<p class="man-lead">.*?</p>)', lambda m: m.group(1) + '\n      <p class="small why-upd">%s</p>' % S['UPD'], s, 1, re.S); assert n, 'man-lead'
    if S['DEF_H2'] not in s:
        k = '<section class="glass prose" data-reveal>'; assert k in s, 'section'
        s = s.replace(k, k + '\n      <h2>%s</h2>\n      <p>%s</p>' % (S['DEF_H2'], S['DEF_P']), 1)
        faq = '      <h2>%s</h2>\n' % S['FAQ_H2'] + ''.join('      <h3>%s</h3>\n      <p>%s</p>\n' % (S['Q%d' % i], S['A%d' % i]) for i in range(1, 5))
        k = '<p class="why-cta">'; assert k in s, 'cta'
        s = s.replace(k, faq + '      ' + k, 1)
    if 'FAQPage' not in s: s = s.replace('</head>', ld(faqld(S, lang)) + '</head>', 1)
    return s

def jsons(lang):
    p = os.path.join(DOCS, 'i18n', lang + '.json'); d = json.loads(rd(p)); t = d['t']
    t[8] = STR[lang]['LEAD']
    while len(t) < 130: t.append('')
    if len(t) == 130: t.append('')
    t[130] = STR[lang]['FOOT']
    wr(p, json.dumps(d, ensure_ascii=False, separators=(',', ':')))

if __name__ == '__main__':
    for lang, prefix in LANGS.items():
        base = os.path.join(DOCS, prefix.strip('/')) if prefix else DOCS
        for name, fn in (('index.html', home), ('why-houzy.html', why)):
            p = os.path.join(base, name); s = rd(p); s2 = fn(lang, s)
            if s2 != s: wr(p, s2); print('edited', lang, name)
        jsons(lang)
    n = 0
    for p in glob.glob(os.path.join(DOCS, '**', '*.html'), recursive=True) + [os.path.join(DOCS, 'llms.txt')]:
        s = rd(p)
        if 'version5.jpg' in s: wr(p, s.replace('img/version5.jpg', 'img/houzy-card.jpg')); n += 1
    print('og:image swapped in', n, 'files')
