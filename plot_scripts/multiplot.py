#!/usr/bin/env python3

import os, re, io, sys, math
import yaml, pprint, argparse, importlib
from cycler import cycler
from itertools import cycle
from functools import reduce
import matplotlib as mpl
mpl.use("PDF")
import matplotlib.pyplot as plt

def noting(*args):
    print("\033[1m\033[92mNoting:\033[0m", " ".join(args))


def warning(*args):
    print("\033[1m\033[93mWarning:\033[0m", " ".join(args))


def errexit(*args):
    print("\033[1m\033[91mError:\033[0m", " ".join(args))
    sys.exit(1)


# least common multiple of integers
def lcms(*nums):
    lcm = lambda a, b: a * b // math.gcd(a, b)
    return reduce(lcm, nums)


def figparams(datafiles, configs):
    subrows, subcols = configs["multiplot"]["subplots"]

    if subrows * subcols < len(datafiles):
        warning("subplots {}x{} less than datafiles {}, ignoring trailing ones."
                .format(subrows, subcols, len(datafiles)))
        del datafiles[(subrows * subcols - len(datafiles)):]
    if subrows * subcols > len(datafiles):
        warning("subplots {}x{} more than datafiles {}, repeating the last one."
                .format(subrows, subcols, len(datafiles)))
        datafiles += [datafiles[-1]] * (subrows * subcols - len(datafiles))

    figsize = configs["multiplot"]["figsize"]
    noting("drawing {}x{} subplots with figsize {}, using datafiles {}"
           .format(subrows, subcols, figsize, datafiles))

    return subrows, subcols, figsize


def legendparams(configs):
    legend_on = configs["multiplot"]["legend_on"]
    legend_loc = configs["multiplot"]["legend_loc"]
    legend_ncol = configs["multiplot"]["legend_ncol"]
    legend_bbox = configs["multiplot"]["legend_bbox"]

    return legend_on, legend_loc, legend_ncol, legend_bbox


def get_plotter(configs):
    plotter_name = configs["multiplot"]["plotter"]

    match = re.search(r"^(.*)\.(.*)$", plotter_name)
    if not match:
        errexit("Cannot parse '{}' as a plotter name! Format: <module>.<func>"
                .format(plotter_name))

    module_name, func_name = match.group(1), match.group(2)

    try:
        module = importlib.import_module(module_name)
    except ModuleNotFoundError:
        errexit("Cannot find module '{}'!".format(module_name))

    try:
        plotter = getattr(module, func_name)
    except AttributeError:
        errexit("Cannot find '{}()' in module {}!"
                .format(func_name, module_name))

    return plotter


def place_legend(axes, legend_on, loc, ncol, bbox):
    if legend_on == None or legend_on == "all":
        return
    ax = axes[legend_on]
    handles, labels = ax.get_legend_handles_labels()
    ax.legend(handles, labels, ncol=ncol, loc=loc, bbox_to_anchor=bbox)


# axes properties that should exist before plotting
def setup_and_append_ax(axes, ax, configs):
    xscales = configs["multiplot"]["xscales"]
    yscales = configs["multiplot"]["yscales"]

    axid = len(axes)

    ax.set_xscale(xscales[axid % len(xscales)])
    ax.set_yscale(yscales[axid % len(yscales)])

    axes.append(ax)


def format_axis_ticks(axis, fmt, scaler):
    if fmt == None and scaler == None:
        return
    if fmt == None:
        fmt = "{0:g}"
    if scaler == None:
        ticks = mpl.ticker.FuncFormatter(lambda val, pos: fmt.format(val))
    else:
        ticks = mpl.ticker.FuncFormatter(lambda val, pos: fmt.format(val * scaler))

    axis.set_major_formatter(ticks)


# axes properties that are better adjusted after plotting
def update_axes(axes, configs):
    xlims = cycle(configs["multiplot"]["xlims"])
    ylims = cycle(configs["multiplot"]["ylims"])
    titles = cycle(configs["multiplot"]["titles"])
    xlabels = cycle(configs["multiplot"]["xlabels"])
    ylabels = cycle(configs["multiplot"]["ylabels"])
    xticks_rots = cycle(configs["multiplot"]["xticks_rots"])
    yticks_rots = cycle(configs["multiplot"]["yticks_rots"])
    xmajor_scalers = cycle(configs["multiplot"]["xmajor_scalers"])
    ymajor_scalers = cycle(configs["multiplot"]["ymajor_scalers"])
    xmajor_formats = cycle(configs["multiplot"]["xmajor_formats"])
    ymajor_formats = cycle(configs["multiplot"]["ymajor_formats"])
    xticks_bottom_labels = cycle(configs["multiplot"]["xticks_bottom_labels"])
    yticks_left_labels = cycle(configs["multiplot"]["yticks_left_labels"])

    for ax in axes:
        ax.set_xlim(next(xlims))
        ax.set_ylim(0, next(ylims))
        ax.set_title(next(titles))
        ax.set_xlabel(next(xlabels))
        ax.set_ylabel(next(ylabels))
        ax.tick_params(axis='x', labelrotation=next(xticks_rots))
        ax.tick_params(axis='y', labelrotation=next(yticks_rots))
        format_axis_ticks(ax.xaxis, next(xmajor_formats), next(xmajor_scalers))
        format_axis_ticks(ax.yaxis, next(ymajor_formats), next(ymajor_scalers))
        ax.tick_params(axis='x', labelbottom=next(xticks_bottom_labels))
        ax.tick_params(axis='y', labelleft=next(yticks_left_labels))


def savefig(fig, configs):
    filename = configs["multiplot"]["saveas"]
    fig.savefig(filename)
    noting("figure saved as", filename)


def subplots(datafiles, configs):
    subrows, subcols, figsize = figparams(datafiles, configs)
    legend_on, legend_loc, legend_ncol, legend_bbox = legendparams(configs)

    with mpl.rc_context(rc=configs["rcparams"]):
        axes = []
        fig, _ = plt.subplots(nrows=subrows, ncols=subcols, figsize=figsize)
        for idx, datafile in enumerate(datafiles):
            # subplot id starts from 1
            ax = plt.subplot(subrows, subcols, idx + 1)
            setup_and_append_ax(axes, ax, configs)

            plotter = get_plotter(configs)
            plotter(ax, datafile, configs)

            if legend_on == "all":
                ax.legend(loc=legend_loc, ncol=legend_ncol,
                          bbox_to_anchor=legend_bbox)

        update_axes(axes, configs)
        place_legend(axes, legend_on, legend_loc, legend_ncol, legend_bbox)

        savefig(fig, configs)


if __name__ == "__main__":
    # parse arguments

    argparser = argparse.ArgumentParser()
    # optional args
    argparser.add_argument("-y", "--yaml", default="multiplot.yml",
        help="YAML config file")
    argparser.add_argument("-c", "--config", default="default",
        help="config name in the YAML config file")
    argparser.add_argument("-v", "--verbose", action="store_true",
        help="print more information")
    args = argparser.parse_args()

    # load config file

    # always load multiplot.yml as default
    if not os.path.isfile("multiplot.yml"):
        errexit("File 'multiplot.yml' does not exist!")
    if not os.path.isfile(args.yaml):
        errexit("File '{}' does not exist!".format(args.yaml))
    with open("multiplot.yml", 'r') as default, open(args.yaml, 'r') as custom:
        configslist = default.readlines() + custom.readlines()
        allconfigs = yaml.safe_load(io.StringIO(''.join(configslist)))

    # load configs

    pp = pprint.PrettyPrinter(indent=2, width=100)

    if args.config not in allconfigs:
        pp.pprint(allconfigs)
        errexit("'{}' does not exist in {}!".format(args.config, args.yaml))

    noting("using config '{}' from '{}'".format(args.config, args.yaml))
    configs = allconfigs[args.config]

    # update rcparams
    # cycler() doesn't support adding property lists of different lengths.
    # If lengths are different, equalize them with their least common multiple.

    # MM-LDRAM, MM-3DXP-Cached, MM-3DXP-Uncached
    colortypes = {"mm": ["#fc8900","#00c6d7","#006a96"],
    # PM-LDRAM, PM-RDRAM, PM-3DXP, PM-PMEP
                # "pm": ["#fc8900", "#fc8900", "#006a96", "#006a96", "#69BDB2"], #memcached
                # "pm": ["#006a96", "#ffe545","#4a3f35"], #ido
                # "pm": ["#fc8900", "#006a96", "#69BDB2"], #memcached-abstract
                # "pm": ["#fc8900", "#6e963b", "#006a96", "#69BDB2"],  # thread
                # "pm": ["#4a3f35","#fc8900", "#006a96", "#69BDB2"], #vacation
                # "pm": ["#fc8900", "#006a96", "#4a3f35", "#69BDB2"], #opt
                # "pm": ["#fc8900", "#006a96", "#fc8900", "#006a96"], #malloc
                # "pm": ["#fc8900", "#4a3f35", "#006a96", "#69BDB2"], #data structures
                # "pm": [ "#00a1ab","#fc8900", "#006a96", "#4a3f35"], #recover
                # "pm": ["#4a3f35","#fc8900", "#006a96"], #yada
                # "pm": ["#DD2C00","#184d47","#fc8900","#006a96","#E3DFC8","#ffe545", "#96BB7C","#B2EBF2", "#ffe545", "#96BB7C","#ffe545", "#00a1ab","#69BDB2"], #vacation-new
                #"pm": ["#DD2C00", "#B2EBF2", "#00BCD4"], #zhuque-persistent, native, zhuque-volatile
                #"pm": ["#B2EBF2", "#DD2C00", "#E3DFC8","#96BB7C", "#184d47", "#fc8900"], #native, zhuque-persistent, Mnemosyne, PMDK, Clobber-NVM
                #"pm": ["#DD2C00","#B2EBF2", "#E3DFC8","#00008B","#96BB7C"],
                #"pm": ["#DD2C00", "#B2EBF2", "#96BB7C", "#184d47", "#ffe545", "#E3DFC8", "#00a1ab"], #zhuque-persistent, native, PMDK, Clobber-NVM, Atlas, Mnemosyne
                "pm": ["#DD2C00", "#B2EBF2", "#000000"],
                "pm": ["#DD2C00", "#B2EBF2", "#000000", "#E3DFC8","#96BB7C", "#fc8900"],
                #"pm": ["#DD2C00", "#B2EBF2", "#fc8900","#E3DFC8","#96BB7C"],
                #"pm": ["#DD2C00", "#184d47", "#E3DFC8","#B2EBF2"],
                #"pm": ["#184d47","#fc8900","#006a96","#4a3f35", "#4a3f35","#ffe545", "#ffe545","#00a1ab","#00a1ab","#69BDB2"], #memcached-1.2.5
                # "pm": ["#4a3f35","#fc8900", "#ffe545", "#00a1ab","#006a96", "#69BDB2"], #log
    # XFS, XFS-DAX, Ext4-DAX, Ext4, Ext4-DJ, NOVA, NOVA-Relaxed
                  "fs": ["#006a96","#7fb4ca","#98df8a",
                         "#fc8900","#c69214","#ffe545",
                         "#6e963b"]
    }
    colortype = configs["multiplot"]["colortype"]

    if colortype == None:
        colors = configs["multiplot"]["colors"]
    else:
        if colortype not in colortypes:
            errexit("colortype '{}' does not exist!".format(colortype))
        colors = colortypes[colortype]

    linemarkers = configs["multiplot"]["linemarkers"]
    linestyles = configs["multiplot"]["linestyles"]

    crlen, lmlen, lslen = len(colors), len(linemarkers), len(linestyles)
    lenlcm = lcms(crlen, lmlen, lslen)

    colors *= lenlcm // crlen
    linemarkers *= lenlcm // lmlen
    linestyles *= lenlcm // lslen

    prop_cycles = cycler(color=colors, marker=linemarkers, linestyle=linestyles)

    configs["rcparams"].update({"axes.prop_cycle": prop_cycles})

    if args.verbose:
        pp.pprint(configs)

    # check datafiles

    datafiles = configs["multiplot"]["datafiles"]

    for datafile in datafiles:
        if datafile == None:
            errexit("datafile is None!")
        if not os.path.isfile(datafile):
            errexit("File", datafile, "does not exist!")

    # make graphs

    plt.style.use(configs["multiplot"]["pltstyle"])

    subplots(datafiles, configs)
