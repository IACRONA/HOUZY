"""py best_page.py : write docs/best-mastering-compressor.html for the hand-maintained languages (en, ru, es, pt) from the
Why HOUZY page shell + best_strings.py, link it from the footers of the home and Why HOUZY pages (en, ru, es, pt), and add it to
the sitemap. fr/it/ja are built from the EN page by build.py (add the page to lib.PAGES). Idempotent."""
import re, json, os
from lib import DOCS, SITE
from best_strings import STR, NAMES
from geo_apply import org, imgobj, IMGNAME
SHOT = ('    <div class="video-stage why-shot" data-reveal><img src="/img/houzy-card.jpg" width="1640" height="912" '
        'alt="%s" loading="lazy" decoding="async"></div>\n')

SLUG = 'best-mastering-compressor.html'
LANGS = {'en': '', 'ru': '/ru', 'es': '/es', 'pt': '/pt'}
HLANG = {'en': 'en', 'ru': 'ru', 'es': 'es', 'pt': 'pt-BR'}
rd = lambda p: open(p, encoding='utf-8').read()
def wr(p, s): open(p, 'w', encoding='utf-8', newline='').write(s)
def ld(o): return '<script type="application/ld+json">\n' + json.dumps(o, ensure_ascii=False, indent=1) + '\n</script>\n'

def main_html(S, cta):
    rows = ''.join('<tr><th scope="row">%s</th><td>%s</td><td>%s</td></tr>\n' % (NAMES[i], S['R%da' % (i + 1)], S['R%db' % (i + 1)]) for i in range(6))
    return ('<main id="main" class="legal dl man why">\n  <div class="wrap">\n    <div class="glass dl-head" data-reveal>\n      <p class="kicker">HOUZY</p>\n'
            '      <h1>%s</h1>\n      <p class="man-lead">%s</p>\n      <p class="small why-upd">%s</p>\n    </div>\n'
            '    <section class="glass prose" data-reveal>\n      <h2>%s</h2>\n      <ul>\n%s      </ul>\n'
            '      <h2>%s</h2>\n      <table>\n<thead><tr><th>%s</th><th>%s</th><th>%s</th></tr></thead>\n<tbody>\n%s</tbody>\n</table>\n      <p class="small">%s</p>\n'
            '      <h2>%s</h2>\n      <p>%s</p>\n      <h2>%s</h2>\n      <p>%s</p>\n      <h2>%s</h2>\n%s      %s\n    </section>\n  </div>\n</main>') % (
        S['H1'], S['LEAD'], S['UPD'], S['H2_CRIT'], ''.join('        <li>%s</li>\n' % S['C%d' % i] for i in range(1, 6)),
        S['H2_CMP'], S['TH1'], S['TH2'], S['TH3'], rows, S['NOTE'], S['H2_WHEN'], S['P_WHEN'], S['H2_OTHER'], S['P_OTHER'], S['H2_FAQ'],
        ''.join('      <h3>%s</h3>\n      <p>%s</p>\n' % (S['Q%d' % i], S['A%d' % i]) for i in range(1, 4)), cta)

def lds(lang, S, prefix):
    url = SITE + prefix + '/' + SLUG
    faq = {"@context": "https://schema.org", "@type": "FAQPage", "inLanguage": HLANG[lang],
           "mainEntity": [{"@type": "Question", "name": S['Q%d' % i], "acceptedAnswer": {"@type": "Answer", "text": S['A%d' % i]}} for i in range(1, 4)]}
    art = {"@context": "https://schema.org", "@type": "Article", "headline": S['H1'], "description": S['DESC'], "inLanguage": HLANG[lang],
           "url": url, "datePublished": "2026-10-28", "dateModified": "2026-10-28", "image": SITE + "/img/houzy-card.jpg",
           "author": {"@type": "Organization", "name": "ACRONA AUDIO", "url": SITE + "/"}, "publisher": {"@type": "Organization", "name": "ACRONA AUDIO", "url": SITE + "/"}}
    bc = {"@context": "https://schema.org", "@type": "BreadcrumbList", "itemListElement": [
        {"@type": "ListItem", "position": 1, "name": S['HOME'], "item": SITE + prefix + "/"},
        {"@type": "ListItem", "position": 2, "name": S['H1'], "item": url}]}
    strip = lambda t: re.sub(r'<[^>]+>', '', t)
    howto = {"@context": "https://schema.org", "@type": "HowTo", "name": S['H2_CRIT'], "inLanguage": HLANG[lang],
             "step": [{"@type": "HowToStep", "position": i, "text": strip(S['C%d' % i])} for i in range(1, 6)]}
    items = {"@context": "https://schema.org", "@type": "ItemList", "name": S['H2_CMP'],
             "itemListElement": [{"@type": "ListItem", "position": i + 1, "name": n} for i, n in enumerate(NAMES)]}
    return ld(art) + ld(bc) + ld(faq) + ld(howto) + ld(items) + ld(org(prefix)) + ld(imgobj(lang, prefix))

def page(lang):
    S, prefix = STR[lang], LANGS[lang]
    shell = rd(os.path.join(DOCS, prefix.strip('/'), 'why-houzy.html'))
    cta = re.search(r'<p class="why-cta">.*?</p>', shell, re.S).group(0)
    url = SITE + prefix + '/' + SLUG
    s = shell
    s = re.sub(r'<title>.*?</title>', lambda m: '<title>%s</title>' % S['TITLE'], s, 1, re.S)
    s = re.sub(r'(<meta name="description" content=")[^"]*"', lambda m: m.group(1) + S['DESC'] + '"', s, 1)
    s = re.sub(r'(<meta property="og:title" content=")[^"]*"', lambda m: m.group(1) + S['TITLE'] + '"', s, 1)
    s = re.sub(r'(<meta property="og:description" content=")[^"]*"', lambda m: m.group(1) + S['DESC'] + '"', s, 1)
    s = re.sub(r'(<link rel="canonical" href=")[^"]*"', lambda m: m.group(1) + url + '"', s, 1)
    s = re.sub(r'(<meta property="og:url" content=")[^"]*"', lambda m: m.group(1) + url + '"', s, 1)
    s = re.sub(r'<script type="application/ld\+json">.*?</script>\n?', '', s, flags=re.S)
    s = re.sub(r'<link rel="stylesheet" href="/vendor/plyr/plyr\.css">\n?|<script src="/vendor/plyr/plyr\.min\.js" defer></script>\n?|<script src="/video\.js[^"]*" defer></script>\n?', '', s)
    s = s.replace('</head>', lds(lang, S, prefix) + '</head>', 1)
    body = main_html(S, cta).replace('    <section class="glass prose"', SHOT % IMGNAME[lang] + '    <section class="glass prose"', 1)
    s = re.sub(r'<main.*?</main>', lambda m: body, s, 1, re.S)
    wr(os.path.join(DOCS, prefix.strip('/'), SLUG), s)

def footer_link(lang, name):
    S, prefix = STR[lang], LANGS[lang]
    p = os.path.join(DOCS, prefix.strip('/'), name); s = rd(p)
    if SLUG in s: return
    i = s.find('<footer'); j = s.find('open.spotify.com', i)
    k = s.rfind('<li>', i, j); assert k > 0, (lang, name)
    wr(p, s[:k] + '<li><a href="%s/%s">%s</a></li>\n        ' % (prefix, SLUG, S['LINK']) + s[k:])

def sitemap():
    p = os.path.join(DOCS, 'sitemap.xml'); s = rd(p)
    if SLUG in s: return
    e = '  <url><loc>%s/%s</loc>\n    <lastmod>2026-10-28</lastmod><priority>0.8</priority></url>\n' % (SITE, SLUG)
    wr(p, s.replace('</urlset>', e + '</urlset>'))

if __name__ == '__main__':
    for lang in LANGS:
        page(lang)
        for name in ('index.html', 'why-houzy.html'): footer_link(lang, name)
    sitemap(); print('best page written for', ', '.join(LANGS))
