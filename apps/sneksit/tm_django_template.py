"""Test the performance of the Django template system.

This will have Django generate a 150x150-cell HTML table.
"""

import django.conf
from django.template import Context, Template

import functools
import argparse
from sneksit import do_bench

# 2016-10-10: Python 3.6 takes 380 ms
DEFAULT_SIZE = 1000


def bench_django_template(size):
    template = Template("""<table>
{% for row in table %}
<tr>{% for col in row %}<td>{{ col|escape }}</td>{% endfor %}</tr>
{% endfor %}
</table>
    """)
    table = [range(size) for _ in range(size)]
    context = Context({"table": table})

    func = functools.partial(template.render, context)
    do_bench('django_template', func, 5)


if __name__ == "__main__":
    django.conf.settings.configure(TEMPLATES=[{
        'BACKEND': 'django.template.backends.django.DjangoTemplates',
    }])
    django.setup()

    cmd = argparse.ArgumentParser()
    cmd.add_argument("--table-size",
                     type=int, default=DEFAULT_SIZE,
                     help="Size of the HTML table, height and width "
                          "(default: %s)" % DEFAULT_SIZE)

    args = cmd.parse_args()
    bench_django_template(args.table_size)
