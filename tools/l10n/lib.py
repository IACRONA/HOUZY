"""Site localisation helpers (no third-party packages). An English page is cut into UNITS: the inner HTML of every element that
holds words and only inline children (or carries data-i18n), the translatable attributes, and the JSON-LD strings.
apply_page() puts translations back and rewrites the language-dependent head and links."""
import re, json, os
DOCS = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'docs'))
PAGES = ['index', 'plugin', 'download', 'manual', 'upgrade', 'why-houzy', 'affiliates', 'best-mastering-compressor']
SITE = 'https://acronaaudio.shop'
INLINE = {'a', 'b', 'i', 'em', 'strong', 'code', 'span', 'br', 'sup', 'sub', 'small', 'kbd', 'abbr', 'u', 'mark', 's', 'time'}
VOID = {'br', 'img', 'meta', 'link', 'input', 'hr', 'source', 'wbr', 'area', 'col'}
OPAQUE = re.compile(r'<(script|style|svg|pre|textarea)\b.*?</\1>', re.S | re.I)
TOKEN = re.compile(r'<!--.*?-->|<(?:script|style|svg|pre|textarea)\b.*?</(?:script|style|svg|pre|textarea)>|</?[a-zA-Z][^>]*>', re.S | re.I)
ATTRS = ('alt', 'aria-label', 'title', 'placeholder')
META = ('description', 'og:title', 'og:description', 'twitter:title', 'twitter:description')
LD_KEYS = ('name', 'description', 'text', 'headline', 'alternateName', 'caption', 'abstract', 'articleBody')
norm = lambda s: re.sub(r'\s+', ' ', s).strip()
words = lambda s: bool(re.search(r'[^\W\d_]{2,}', re.sub(r'<[^>]+>|&[a-z#0-9]+;', ' ', s)))

class N:
    def __init__(s, name, attrs, tag_start, inner_start, parent):
        s.name, s.attrs, s.tag_start, s.inner_start, s.inner_end, s.parent = name, attrs, tag_start, inner_start, inner_start, parent
        s.kids, s.texts = [], []

def parse(html):
    root = N('#root', '', 0, 0, None); cur = root; pos = 0
    for m in TOKEN.finditer(html):
        txt = html[pos:m.start()]
        if txt.strip(): cur.texts.append(txt)
        pos = m.end(); t = m.group(0)
        if t.startswith('<!--') or re.match(r'<(script|style|svg|pre|textarea)\b', t, re.I): continue
        name = re.match(r'</?\s*([a-zA-Z0-9]+)', t).group(1).lower()
        if t.startswith('</'):
            n = cur
            while n is not root and n.name != name: n = n.parent   # tolerate sloppy nesting
            if n is not root:
                n.inner_end = m.start(); cur = n.parent
            continue
        node = N(name, t, m.start(), m.end(), cur); cur.kids.append(node)
        if name in VOID or t.endswith('/>'): node.inner_end = node.inner_start
        else: cur = node
    return root

def inline_only(n): return all(c.name in INLINE and inline_only(c) for c in n.kids)
def direct_words(n): return any(words(t) for t in n.texts)

def find_units(html):
    """-> [(inner_start, inner_end, key)] in document order; also warns about words that fall in no unit"""
    root = parse(html); out = []; lost = []
    def walk(n):
        if n is not root and n.name not in ('html', 'head', 'body') and n.name not in VOID:
            i18n = 'data-i18n=' in n.attrs and 'data-i18n-lead' not in n.attrs
            if (i18n and (direct_words(n) or any(words(html[k.inner_start:k.inner_end]) for k in n.kids))) or (inline_only(n) and direct_words(n)):
                key = norm(html[n.inner_start:n.inner_end])
                if words(key): out.append((n.inner_start, n.inner_end, key))
                return
            if direct_words(n): lost.append((n.name, ' | '.join(norm(t)[:60] for t in n.texts if words(t))))
        for k in n.kids: walk(k)
    walk(root); return out, lost

def attr_units(html):
    out = []
    for m in re.finditer(r'<[a-zA-Z][^>]*>', html):
        t = m.group(0)
        for a in ATTRS:
            for x in re.finditer(r'\s%s="([^"]*)"' % a, t):
                v = norm(x.group(1))
                if words(v): out.append(v)
        if t.lower().startswith('<meta'):
            nm = re.search(r'(?:name|property)="([^"]+)"', t); ct = re.search(r'content="([^"]*)"', t)
            if nm and ct and nm.group(1) in META and words(ct.group(1)): out.append(norm(ct.group(1)))
    return out

LD = re.compile(r'(<script type="application/ld\+json">)(.*?)(</script>)', re.S)
def walk_ld(o, f):
    if isinstance(o, dict):
        for k, v in list(o.items()):
            if isinstance(v, str) and k in LD_KEYS and words(v): o[k] = f(v)
            else: walk_ld(v, f)
    elif isinstance(o, list):
        for v in o: walk_ld(v, f)
def ld_units(html):
    out = []
    for m in LD.finditer(html):
        try: walk_ld(json.loads(m.group(2)), lambda v: (out.append(v), v)[1])
        except Exception: pass
    return out

def page_units(html):
    """every translatable string of a page, document order (text units, attributes, JSON-LD)"""
    us, lost = find_units(html)
    return [k for _, _, k in us] + attr_units(html) + ld_units(html), lost

def load_tr(path):
    d = {}
    for ln in open(path, encoding='utf-8'):
        ln = ln.rstrip('\n')
        if not ln.strip() or ln.startswith('#'): continue
        k, _, v = ln.partition('\t'); d[int(k)] = v
    return d
