import json, os, sys
from lib import *
seen = {}; order = []
def add(s):
    s = norm(s)
    if s and s not in seen: seen[s] = len(order); order.append(s)
for p in PAGES:
    html = open(os.path.join(DOCS, p + '.html'), encoding='utf-8').read()
    us, lost = page_units(html)
    for u in us: add(u)
    for l in lost: print('LOST in', p, l)
en = json.load(open(os.path.join(DOCS, 'i18n', 'en.json'), encoding='utf-8'))
def walk(o):
    if isinstance(o, dict):
        for v in o.values(): walk(v)
    elif isinstance(o, list):
        for v in o: walk(v)
    elif isinstance(o, str) and words(o): add(o)
walk(en.get('t', [])); walk(en.get('js', {})); walk(en.get('head', {}))
json.dump(order, open('units.json', 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
print(len(order), 'units,', sum(len(s) for s in order), 'chars')
