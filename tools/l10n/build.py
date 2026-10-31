"""py build.py fr|it|ja [...]  - build docs/<lang>/*.html from the English pages + <lang>.tsv (index = line of units.json),
write docs/i18n/<lang>.json (the in-place switch of the home page), then re-issue hreflang, footer language links and the
sitemap for ALL languages (existing ones included). Re-run any time: output is a pure function of the inputs."""
import re, json, os, sys
from lib import *

LANGS = {  # code: (html lang, og:locale, hreflang, native name)
    'fr': ('fr', 'fr_FR', 'fr', 'Français'), 'it': ('it', 'it_IT', 'it', 'Italiano'), 'ja': ('ja', 'ja_JP', 'ja', '日本語')}
ALL = [('en', 'en', ''), ('ru', 'ru', '/ru'), ('es', 'es', '/es'), ('pt-BR', 'pt', '/pt'), ('fr', 'fr', '/fr'), ('it', 'it', '/it'), ('ja', 'ja', '/ja')]
LOCAL = re.compile(r'^/(?:(?:plugin|download|manual|upgrade|why-houzy|affiliates|best-mastering-compressor)\.html)?(#.*)?$')
rd = lambda p: open(p, encoding='utf-8').read()
def wr(p, s):
    os.makedirs(os.path.dirname(p), exist_ok=True); open(p, 'w', encoding='utf-8', newline='').write(s)

def loc_hash(path, lang):          # '/' -> '/fr/' ; '/#faq' -> '/fr/#faq' ; '/manual.html' -> '/fr/manual.html' ; other paths unchanged
    return ('/' + lang + path) if LOCAL.match(path) else path

def tr_table(code):
    units = json.load(open('units.json', encoding='utf-8'))
    tr = load_tr(code + '.tsv'); T = {}
    for i, k in enumerate(units):
        T[k] = tr.get(i, k)
    miss = [i for i in range(len(units)) if i not in tr]
    return T, units, miss

def build_page(page, code, T, stats):
    hl, ogl, _, _ = LANGS[code]
    html = rd(os.path.join(DOCS, page + '.html'))
    us, _ = find_units(html)
    for s, e, k in sorted(us, key=lambda x: -x[0]):
        html = html[:s] + T.get(k, k) + html[e:]
        stats['n'] += 1
    # attributes
    def attr_sub(m):
        tag = m.group(0)
        for a in ATTRS:
            tag = re.sub(r'(\s%s=")([^"]*)(?=")' % a, lambda x: x.group(1) + (T.get(norm(x.group(2)), norm(x.group(2))).replace('"', '&quot;') if words(x.group(2)) else x.group(2)), tag)
        if tag.lower().startswith('<meta'):
            nm = re.search(r'(?:name|property)="([^"]+)"', tag)
            if nm and nm.group(1) in META:
                tag = re.sub(r'(content=")([^"]*)(?=")', lambda x: x.group(1) + T.get(norm(x.group(2)), norm(x.group(2))).replace('"', '&quot;'), tag)
        return tag
    html = re.sub(r'<(?!/|!--)[a-zA-Z][^>]*>', attr_sub, html)
    # JSON-LD
    def ld_sub(m):
        try: d = json.loads(m.group(2))
        except Exception: return m.group(0)
        walk_ld(d, lambda v: T.get(norm(v), v))
        def fix(o):
            if isinstance(o, dict):
                for k, v in list(o.items()):
                    if k == 'inLanguage': o[k] = hl
                    elif isinstance(v, str) and v.startswith(SITE + '/') and k in ('url', 'item', '@id'):
                        o[k] = SITE + loc_hash(v[len(SITE):], code)
                    else: fix(v)
            elif isinstance(o, list):
                for v in o: fix(v)
        fix(d)
        return m.group(1) + '\n' + json.dumps(d, ensure_ascii=False, indent=1) + '\n' + m.group(3)
    html = LD.sub(ld_sub, html)
    # head
    html = re.sub(r'<html lang="[^"]*"', '<html lang="%s"' % hl, html, 1)
    html = re.sub(r'(<meta property="og:locale" content=")[^"]*"', r'\g<1>%s"' % ogl, html, 1)
    for pat in (r'(<link rel="canonical" href=")([^"]*)"', r'(<meta property="og:url" content=")([^"]*)"'):
        html = re.sub(pat, lambda m: m.group(1) + SITE + loc_hash(m.group(2)[len(SITE):], code) + '"', html, 1)
    # internal links (anchors that are language switches keep their hreflang target)
    def href_sub(m):
        tag = m.group(0)
        if ' hreflang=' in tag: return tag
        return re.sub(r'(href=")(/[^"]*)"', lambda x: x.group(1) + loc_hash(x.group(2), code) + '"', tag)
    html = re.sub(r'<a\s[^>]*>', href_sub, html)
    wr(os.path.join(DOCS, code, page + '.html'), html)

def build_json(code, T):
    en = json.load(open(os.path.join(DOCS, 'i18n', 'en.json'), encoding='utf-8'))
    def tx(o):
        if isinstance(o, dict): return {k: tx(v) for k, v in o.items()}
        if isinstance(o, list): return [tx(v) for v in o]
        if isinstance(o, str) and words(o): return T.get(norm(o), o)
        return o
    out = tx(en); out['lang'] = code
    wr(os.path.join(DOCS, 'i18n', code + '.json'), json.dumps(out, ensure_ascii=False, separators=(',', ':')))

# ---- everything that lists the languages: hreflang, footer links, sitemap ----------------------------------------------
def path_of(f):
    rel = os.path.relpath(f, DOCS).replace('\\', '/')
    m = re.match(r'^(ru|es|pt|fr|it|ja)/(.*)$', rel)
    name = m.group(2) if m else rel
    return '/' if name == 'index.html' else '/' + name
def url_for(path, prefix):
    return SITE + (prefix + path if prefix else path)

def fix_lang_lists():
    files = [os.path.join(DOCS, p + '.html') for p in PAGES] + [os.path.join(DOCS, l, p + '.html') for _, l, pr in ALL if pr for p in PAGES if os.path.exists(os.path.join(DOCS, l, p + '.html'))]
    for f in files:
        path = path_of(f)
        s = rd(f)
        block = ''.join('<link rel="alternate" hreflang="%s" href="%s">\n' % (h, url_for(path, pr)) for h, _, pr in ALL)
        block += '<link rel="alternate" hreflang="x-default" href="%s">\n' % url_for(path, '')
        s2 = re.sub(r'(<link rel="alternate" hreflang="[^"]*" href="[^"]*">\n)+', lambda m: block, s, 1)
        add = ''.join(' · <a href="%s" hreflang="%s" lang="%s">%s</a>' % (pr + path, h, h, LANGS[l][3]) for h, l, pr in ALL if l in LANGS)
        s2 = re.sub(r'(<a href="[^"]*" hreflang="pt-BR" lang="pt-BR">Português</a>)(?: · <a href="[^"]*" hreflang="(?:fr|it|ja)" lang="(?:fr|it|ja)">[^<]*</a>)*', lambda m: m.group(1) + add, s2)
        if s2 != s: wr(f, s2)

def sitemap():
    p = os.path.join(DOCS, 'sitemap.xml'); s = rd(p)
    ents = re.findall(r'<url><loc>(.*?)</loc>.*?<lastmod>(.*?)</lastmod><priority>(.*?)</priority></url>', s, re.S)
    keep = []; seen = set()
    for u, lm, pr in ents:
        path = u[len(SITE):]; m = re.match(r'^/(ru|es|pt|fr|it|ja)(/.*)$', path); base = m.group(2) if m else path
        if LOCAL.match(base):
            if base not in seen: seen.add(base); keep.append((base, lm, pr))
        else: keep.append((None, u, (lm, pr)))
    out = ['<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" xmlns:xhtml="http://www.w3.org/1999/xhtml">\n']
    for base, lm, pr in keep:
        if base is None:
            out.append('  <url><loc>%s</loc>\n    <lastmod>%s</lastmod><priority>%s</priority></url>\n' % (lm, pr[0], pr[1])); continue
        alts = ''.join('    <xhtml:link rel="alternate" hreflang="%s" href="%s"/>\n' % (h, url_for(base, pr_)) for h, _, pr_ in ALL)
        alts += '    <xhtml:link rel="alternate" hreflang="x-default" href="%s"/>\n' % url_for(base, '')
        for h, _, pr_ in ALL:
            out.append('  <url><loc>%s</loc>\n%s    <lastmod>%s</lastmod><priority>%s</priority></url>\n' % (url_for(base, pr_), alts, lm, pr))
    out.append('</urlset>\n'); wr(p, ''.join(out))

if __name__ == '__main__':
    for code in [a for a in sys.argv[1:] if a in LANGS]:
        T, units, miss = tr_table(code); stats = {'n': 0}
        for pg in PAGES: build_page(pg, code, T, stats)
        build_json(code, T)
        print(code, 'pages built, %d units placed, %d untranslated (kept as English):' % (stats['n'], len(miss)), miss[:80])
    fix_lang_lists(); sitemap(); print('hreflang, footer links, sitemap refreshed')
