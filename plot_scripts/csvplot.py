#!/usr/bin/env python3

# Functions in this module are called from multiplot.py. Each function is
# responsible for plotting ONE csvfile on ONE Axes instance, using configs.

# Calling signature: (ax, csvfile, configs)
# You can pass arbitrary arguments from your YAML file via configs.

# About Axes: matplotlib.org/api/axes_api.html

# Function names are identified by the "plotter" value in YAML config files.

# Try to make each function generic for plotting one kind of data representation
# in CSV format. Custom modifiers such as titles, labels, and styling are
# handled by multiplot.py according to your YAML config files.

# Pandas DataFrame is an optional data structure to represent CSV data in
# Python. It also has some nice plot functions built on top of matplotlib.

# About pandas.DataFrame:
# pandas.pydata.org/pandas-docs/stable/generated/pandas.DataFrame.html
# About pandas.DataFrame.plot:
# pandas.pydata.org/pandas-docs/stable/generated/pandas.DataFrame.plot.html

import pandas as pd
import matplotlib.ticker as ticker

pd.set_option("precision", 2) # decimal positions
pd.set_option("display.width", 1000) # characters

# dataframe to bars (helper function, NOT callable from multiplot.py)
def dfbars(ax, df, configs):
    df.plot.bar(ax=ax, legend=False, subplots=False,
        width=configs["multiplot"]["barwidth"]
    )

    barsets, datasets = len(df.index), len(df.columns) # csv rows, cols
    # need to set hatch and edgecolor for each bar due to bugs in pandas
    # ax.patches contain (barsets * datasets) bars, iterating datasets first,
    # idx = 0 .. barsets * datasets - 1
    for idx, bar in enumerate(ax.patches):
        bar.set_edgecolor("black")
        # bars of one dataset use the same hatch
        bar.set_hatch(configs["multiplot"]["barhatches"][idx // barsets])

    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="y", which="major", color="lightgray")


# generic bar plot
def bars(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    dfbars(ax, df, configs)


# bars using transposed dataframe
def bars_transposed(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    dfbars(ax, df.T, configs)


# generic line plot
def lines(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    df.plot.line(ax=ax, legend=False, subplots=False)

    ax.set_xticks(df.index)
    ax.set_xticklabels(df.index)

    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="both", which="major", color="lightgray")

def lines_ratio(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    color_mm = ["#fc8900","#00c6d7","#006a96"]
    color_ratio = ["#006a96","#7fb4ca",
                         "#fc8900","#c69214","#ffe545",
                         "#6e963b","#98df8a"]

    # I'm not sure if there is a better way to do this
    if df.iloc[0, 0] < 10:
        df.plot.line(ax=ax, legend=False, subplots=False, color=color_ratio)
    else:
        df.plot.line(ax=ax, legend=False, subplots=False, color=color_mm)

    ax.set_xticks(df.index)
    ax.set_xticklabels(df.index)

    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="both", which="major", color="lightgray")

def lines_ratio_err(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    datalen = len(df.columns) // 3
    dfavg = df.iloc[:, 0:datalen]
#    mindelta = dfavg*0 + 10000
    mindelta = df.iloc[:, datalen:2*datalen]
#    maxdelta = dfavg*0 + 15000
    maxdelta = df.iloc[:, 2*datalen:3*datalen]

    #fmts = ['-^', '-v', '-o', '-x']
    fmts = ['-o', '-o', '-o', '-o']
    color_mm = ["#fc8900","#00c6d7","#006a96"]
    color_ratio = ["#006a96","#7fb4ca",
                         "#fc8900","#c69214","#ffe545",
                         "#6e963b","#98df8a"]

    if df.iloc[0, 0] < 10:
        for col in range(0, datalen):
            ax.errorbar(df.index, dfavg.iloc[:, col], fmt=fmts[col], capsize=3,
                    color=color_ratio[col], ecolor=color_ratio[col],
                    yerr=[mindelta.iloc[:, col], maxdelta.iloc[:, col]])
    else:
        for col in range(0, datalen):
            ax.errorbar(df.index, dfavg.iloc[:, col], fmt=fmts[col], capsize=3,
                    color=color_mm[col], ecolor=color_mm[col],
                    yerr=[mindelta.iloc[:, col], maxdelta.iloc[:, col]])

    ax.set_xticks(dfavg.index)
    ax.set_xticklabels(dfavg.index)

    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="both", which="major", color="lightgray")


def lines_rwmix(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    #dfi = df.interpolate() # fill NaNs but don't plot marker for these points
    #dfi.plot.line(ax=ax, legend=False, subplots=False, marker='')

    df.plot.line(ax=ax, legend=False, subplots=False)

    dflen = len(df.index)

    ax.set_xticks([0, dflen//4, dflen//2, dflen*3//4, dflen-1])
    xticklabels = ["All Wr.", "Wr. Dominant", "", "Rd.  Dominant", "All Rd."]
    ax.set_xticklabels(xticklabels)

    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="both", which="major", color="lightgray")

# 1 DIMM
def lines_1DIMM(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    df.plot.line(ax=ax, legend=False, subplots=False)

    dflen = len(df.index)

    ax.set_xticks([0, 2, 5, 8, 11])
    xticklabels = ["64 B", "256 B", "2 KB", "16 KB", "128 KB"]
    ax.set_xticklabels(xticklabels)

    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="both", which="major", color="lightgray")

# generic line plot
def lines_5x(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    df.plot.line(ax=ax, legend=False, subplots=False)

    ax.set_xticks(range(1, len(df.index), 5))
    ax.set_xticklabels(range(1, len(df.index), 5))

    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="both", which="major", color="lightgray")


def lines_wide(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    df.plot.line(ax=ax, legend=False, subplots=False)
    tick = df.index[len(df.index) - 1] / 4
    skip = len(df.index) >> 2
    major = ticker.MultipleLocator(tick)
    minor = ticker.MultipleLocator(tick)

    array = [0]
    if df.index[0] == 0:
        array.append(df.index[0])
        array.append(df.index[skip])
        array.append(df.index[skip*2])
        array.append(df.index[skip*3])
    else:
        array.append(0)
        array.append(df.index[skip - 1])
        array.append(df.index[skip*2 - 1])
        array.append(df.index[skip*3 - 1])
    array.append(df.index[len(df.index)-1])
    
    print(array)

    ax.set_xticks(df.index)
    ax.xaxis.set_major_locator(major)
    ax.xaxis.set_minor_locator(minor)
    ax.set_xticklabels(array)


    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="both", which="major", color="lightgray")

# a customized bar plot
def bars_normalized(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    normalize_to_column = configs["multiplot"]["normalize_to_column"]
    for app in df.index: # normalize
        df.loc[app] = df.loc[app] / df.loc[app, normalize_to_column]
    #df.loc["average"] = df.mean(axis=0)

    #print(df.loc["average"])

    dfbars(ax, df, configs)

    ax.axhline(y=1.0, xmin=0, xmax=1, color="blue", ls="--", lw=1.0)


# bars using transposed dataframe
def bars_transposed_normalized(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    normalize_to = df["XFS"]["PM-LDRAM"]
    for app in df.columns: # normalize
        df[app] = df[app] / normalize_to
  # df.loc["average"] = df.mean(axis=0)

    dfbars(ax, df.T, configs)

  # ax.axhline(y=1.0, xmin=0, xmax=1, color="blue", ls="--", lw=1.0)


# line plot with X-Y data saved in alternating csv columns
def xyline(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)

    print(df)

    for i in range(0, len(df.columns), 2):
        x = df.iloc[:, i].tolist()
        y = df.iloc[:, i+1].tolist()
        ax.plot(y, x, label=df.columns[i])

    ax.grid(b=True, axis="both", which="major", color="lightgray")



def lines_text(ax, csvfile, configs):
    df = pd.read_csv(csvfile, comment="#", index_col=0, parse_dates=False)
    df = df.astype(float)
    print(df)
    df.plot.line(ax=ax, legend=False, subplots=False)

#    dflen = len(df.index)
    #print(dflen)
    ax.set_xticks([64, 512, 4096, 32768, 262144, 2097152])
    ax.set_xticklabels(['64B', '512B', '4KB', '32KB', '256KB', '2MB'])
#    ax.set_xticklabels([df.index[0], df.index[0], df.index[5], df.index[10], df.index[15]])


    # this could use rcparams but axes.grid.axis always set both (bug)
    ax.grid(b=True, axis="both", which="major", color="lightgray")
