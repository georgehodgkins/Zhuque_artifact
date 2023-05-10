# Zhuque does not support multi-process operation, so we
# modified the benchmarks we ran to use this custom runner framework
# instead of the one provided with pyperformance
from timeit import *
from pathlib import Path
from argparse import ArgumentParser
import subprocess
from subprocess import PIPE, DEVNULL
import sys
import os

def do_bench(name, func, iters):
    if not callable(func):
        raise ValueError("Func must be a callable object!")

    warmups = iters / 20
    for _ in range(int(warmups)):
        func()

    T = Timer(func, setup='gc.enable()')
    dur = T.timeit(iters)
    print(iters, name, dur)


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--psdir", required=True, metavar='PATH',
            help="LD_RELOAD will be set to this value for pmem runs")
    args = parser.parse_args()
    psdir = args.psdir

    ps = Path(psdir)
    if not ps.is_dir():
        raise ValueError("Psdir must be a valid directory path!")

    cwd = Path.cwd()
    benches = cwd.glob('tm_*.py')

    for bench in benches:
        # run without pmem
        print(bench.name, "VOLATILE", end='', flush=True)
        vres = subprocess.run([sys.executable, bench.name], stdout=PIPE, stderr=DEVNULL, text=True)
        if vres.returncode != 0:
            exc = "Volatile run of {0} did not exit cleanly! Code {1}".format(bench.name, vres.returncode)
            raise RuntimeError(exc)

        (iters, name, dur) = [t(s) for t,s in zip((int, str, float), vres.stdout.split())]
        print(" -- runs=", iters, " total=", round(dur,6), " s avg=", round(dur/iters,6), " s", sep='')

        # run with pmem
        print(bench.name, "PERSISTENT", end='', flush=True)
        nenv = os.environ
        nenv['LD_RELOAD'] = psdir

        pres = subprocess.run([sys.executable, bench.name], stdout=PIPE, stderr=DEVNULL, text=True, env=nenv)
        if pres.returncode != 0:
            exc = "Persistent run of {0} did not exit cleanly! Code {1}".format(bench.name, pres.returncode)
            raise RuntimeError(exc)

        (iters, name, dur) = [t(s) for t,s in zip((int, str, float), pres.stdout.split())]
        print(" -- runs=", iters, " total=", round(dur,6), " s avg=", round(dur/iters,6), " s", sep='')



