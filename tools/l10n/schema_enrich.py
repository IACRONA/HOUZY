"""py schema_enrich.py : run after every build. Fills the JSON-LD of every page with real data the page already has (its own
title, description, canonical URL, update date) so no block stays a bare stub, adds a Product block for HOUZY next to the
SoftwareApplication one (price and availability as sold on Gumroad), and the GitHub profile to sameAs. Idempotent."""
import re, json, glob, os, html
from lib import DOCS

DATE = '2026-10-28'
PROFILES = ['https://github.com/IACRONA/HOUZY', 'https://www.youtube.com/@acronaaudio', 'https://www.instagram.com/acrona_audio']
LD = re.compile(r'(<script type="application/ld\+json">)(.*?)(</script>)', re.S)
attr = lambda s, pat: (lambda m: html.unescape(m.group(1)) if m else None)(re.search(pat, s))

def enrich(o, page):
    t = o.get('@type')
    def put(k, v):
        if v and k not in o: o[k] = v
    if t in ('FAQPage', 'HowTo', 'ItemList', 'BreadcrumbList', 'WebSite'):
        put('name', page['title']); put('url', page['url']); put('description', page['desc'])
    if t in ('FAQPage', 'HowTo', 'TechArticle', 'Article'):
        put('dateModified', DATE)
    if t == 'ItemList':
        put('numberOfItems', len(o.get('itemListElement', []))); put('itemListOrder', 'https://schema.org/ItemListUnordered')
    if t == 'BreadcrumbList':
        put('numberOfItems', len(o.get('itemListElement', [])))
    if t == 'Organization' and isinstance(o.get('sameAs'), list):
        o['sameAs'] += [u for u in PROFILES if u not in o['sameAs']]
    return o

def product_from(app):
    return {"@context": "https://schema.org", "@type": "Product", "name": app.get('name', 'HOUZY'),
            "description": app.get('description'), "image": app.get('image'), "category": app.get('applicationSubCategory'),
            "brand": {"@type": "Brand", "name": "ACRONA AUDIO"}, "url": app.get('url'),
            "offers": {"@type": "Offer", "price": "69", "priceCurrency": "USD", "availability": "https://schema.org/InStock",
                       "url": "https://acronawar.gumroad.com/l/houzy"}}

def run(path):
    s = open(path, encoding='utf-8').read()
    page = {'title': attr(s, r'<title>(.*?)</title>'), 'desc': attr(s, r'<meta name="description" content="([^"]*)"'),
            'url': attr(s, r'<link rel="canonical" href="([^"]*)"')}
    app = None; has_product = '"Product"' in s
    def sub(m):
        nonlocal app
        try: d = json.loads(m.group(2))
        except Exception: return m.group(0)
        objs = d if isinstance(d, list) else [d]
        for o in objs:
            if isinstance(o, dict):
                enrich(o, page)
                if o.get('@type') == 'SoftwareApplication': app = o
        return m.group(1) + '\n' + json.dumps(d, ensure_ascii=False, indent=1) + '\n' + m.group(3)
    s2 = LD.sub(sub, s)
    if not app and path.endswith('best-mastering-compressor.html'):   # the comparison page offers HOUZY too: same product as on its home page
        home = open(os.path.join(os.path.dirname(path), 'index.html'), encoding='utf-8').read()
        for m in LD.finditer(home):
            d = json.loads(m.group(2))
            for o in (d if isinstance(d, list) else [d]):
                if isinstance(o, dict) and o.get('@type') == 'SoftwareApplication': app = o
    if 'class="qs-steps"' in s2 and '"HowTo"' not in s2:     # the "In short" steps shown on the home page
        box = re.search(r'<ol class="qs-steps">(.*?)</ol>', s2, re.S).group(1)
        steps = [html.unescape(re.sub('<[^>]+>', '', x)).strip() for x in re.findall(r'<li>(.*?)</li>', box, re.S)]
        ht = {"@context": "https://schema.org", "@type": "HowTo", "name": page['title'], "description": page['desc'], "url": page['url'],
              "dateModified": DATE, "tool": [{"@type": "HowToTool", "name": "HOUZY"}],
              "step": [{"@type": "HowToStep", "position": i + 1, "text": t} for i, t in enumerate(steps)]}
        s2 = s2.replace('</head>', '<script type="application/ld+json">\n' + json.dumps(ht, ensure_ascii=False, indent=1) + '\n</script>\n</head>', 1)
    if app and not has_product:
        s2 = s2.replace('</head>', '<script type="application/ld+json">\n' + json.dumps(product_from(app), ensure_ascii=False, indent=1) + '\n</script>\n</head>', 1)
    if s2 != s: open(path, 'w', encoding='utf-8', newline='').write(s2); return 1
    return 0

if __name__ == '__main__':
    n = sum(run(p) for p in glob.glob(os.path.join(DOCS, '**', '*.html'), recursive=True))
    print('enriched', n, 'pages')
