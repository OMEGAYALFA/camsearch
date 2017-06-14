#!/usr/bin/python3

import io
import os
import sys
import time
import requests
import argparse
from functools import partial
from collections import namedtuple
from lxml import etree

# http://www.nirsoft.net/countryip/index.html
SOURCE = "http://www.nirsoft.net/countryip/%s.html"

argp = argparse.ArgumentParser()
argp.add_argument('--country', type=str.lower,
    help='specify country code', required=True)
argp.add_argument('--start-at', metavar='INDEX', default=0, type=int,
    help='start at INDEX in country ip listing')
argp.add_argument('camsearch_args',
    help='speficy arguments for camsearch (something like "-p 80 -p 81")')

perr = partial(print, file=sys.stderr)
pflush = partial(print, flush=True)

IPRange = namedtuple('IPRange', ('from_', 'to', 'total', 'date', 'owner'))

def get_ip_listing(country_code):
    res = requests.get(SOURCE % country_code)
    if res.status_code != 200:
        raise Exception("IP Listing not found for that country")

    html_tree = etree.parse(io.StringIO(res.text), etree.HTMLParser())

    for row in html_tree.xpath('//tr'):
        try:
            tds_text = []
            for td in row.getchildren():
                tds_text.append(td.text.strip())

            yield IPRange(*tds_text)
        except Exception as e:
            continue

args = argp.parse_args()
range_listing = list(get_ip_listing(args.country))
for i, iprange in enumerate(range_listing):
    if i < args.start_at:
        continue

    try:
        cmdline = "./camsearch %s -f %s -t %s" % (args.camsearch_args, iprange.from_, iprange.to)
        pflush("# [{i}] Probing {from_} - {to} (total: {total}) [{owner}]".format(
            i=i, from_=iprange.from_, to=iprange.to, total=iprange.total, owner=iprange.owner
        ))
        pflush("#   running", cmdline)

        if os.system(cmdline):
            time.sleep(5)
    except Exception as e:
        perr("Error calling run():", e)
