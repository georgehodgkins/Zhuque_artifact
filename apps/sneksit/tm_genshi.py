"""
Render a template using Genshi module.
"""

import functools
import argparse
from sneksit import do_bench

from genshi.template import MarkupTemplate, NewTextTemplate


BIGTABLE_XML = """\
<table xmlns:py="http://genshi.edgewall.org/">
<tr py:for="row in table">
<td py:for="c in row.values()" py:content="c"/>
</tr>
</table>
"""

BIGTABLE_TEXT = """\
<table>
{% for row in table %}<tr>
{% for c in row.values() %}<td>$c</td>{% end %}
</tr>{% end %}
</table>
"""


def bench_genshi(loops, tmpl_cls, tmpl_str):
    tmpl = tmpl_cls(tmpl_str)
    table = [dict(a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8, i=9, j=10)
             for _ in range(1000)]
    range_it = range(loops)

    for _ in range_it:
        stream = tmpl.generate(table=table)
        stream.render()


BENCHMARKS = {
    'xml': (MarkupTemplate, BIGTABLE_XML),
    'text': (NewTextTemplate, BIGTABLE_TEXT),
}


if __name__ == "__main__":
    benchmarks = sorted(BENCHMARKS)

    for bench in benchmarks:
        tmpl_cls, tmpl_str = BENCHMARKS[bench]
        func = functools.partial(bench_genshi, 100, tmpl_cls, tmpl_str)
        do_bench('genshi', func, 5)

