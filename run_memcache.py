#!/usr/bin/python3
# Unified runner for different configurations of memcached+memslap

import os
import subprocess as sp
import copy
import signal
import sys

PRINT_PROGRESS = False

MEMCACHED_BINS = {
    "mnemosyne" : ("/clobber-pmdk/mnemosyne-gcc/usermode/" 
                        "build/bench/memcached/memcached-1.2.4-mtm/memcached"),
    "clobber" : "/clobber-pmdk/apps/memcached-mutex-clobber/memcached",
    "pmdk" : "/clobber-pmdk/apps/memcached-mutex-pmdk/memcached",
    "zhuque_old" : "/apps/memcached/memcached",
    "native_old" : "/apps/memcached/memcached",
    "zhuque_new" : "/apps/memcached-1.6.17/memcached",
    "native_new" : "/apps/memcached-1.6.17/memcached"
}

MEMCACHED_ENVS = {
    "mnemosyne" : {},
    "clobber" : {
        "LD_PRELOAD" : "/clobber-pmdk/taslock/tl-pthread-mutex.so",
        "LIBTXLOCK" : "tas",
        "PMEM_IS_PMEM_FORCE" : "1"
    },
    "pmdk" : {
        "LD_PRELOAD" : "/clobber-pmdk/taslock/tl-pthread-mutex.so",
        "LIBTXLOCK" : "tas",
        "PMEM_IS_PMEM_FORCE" : "1"
    },
    "zhuque_old" : { "LD_RELOAD" : "/mnt/pmem/wsp" },
    "native_old" : {},
    "zhuque_new" : { "LD_RELOAD" : "/mnt/pmem/wsp" },
    "native_new" : {}
}

# if given, change to this directory before running memcached
MEMCACHED_DIRS = {
    "mnemosyne" : "/clobber-pmdk/mnemosyne-gcc/usermode/"
}

# these paths will be deleted before each run
MEMCACHED_RESOURCES = {
    "mnemosyne" : ["/mnt/pmem/psegments"],
    "clobber" : ["/mnt/pmem/*.pop"],
    "pmdk" : ["/mnt/pmem/*.pop"],
    "zhuque_old" : ["/mnt/pmem/wsp/*"],
    "native_old" : [],
    "zhuque_new" : ["/mnt/pmem/wsp/*"],
    "native_new" : []
}

# different versions of memcached return different codes when killed with SIGTERM
MEMCACHED_RETURN = {
    "mnemosyne" : -15,
    "clobber" : 255,
    "pmdk" : 255,
    "zhuque_old" : -15,
    "native_old" : -15,
    "zhuque_new" : 0,
    "native_new" : 0
}
 
MSLAP_BIN = "/apps/libmemcached/clients/memaslap"
MSLAP_THREADS = "4"
MSLAP_CONCURRENCY = "4"
MSLAP_CONFIG_DIR = "/clobber-pmdk/mnemosyne-gcc/usermode/"
MSLAP_OPCOUNT = "100000"
MSLAP_VALSIZE = "64"

SERVER_IP = "127.0.0.1"
SERVER_PORT = "11215"
SERVER_USER = "root"

def run_memslap(workload):
    memslap_cmd = [MSLAP_BIN, "-s", f"{SERVER_IP}:{SERVER_PORT}",
        "-c", MSLAP_CONCURRENCY, "-x", MSLAP_OPCOUNT, "-T", MSLAP_THREADS,
        "-X", MSLAP_VALSIZE, "-F", os.path.join(MSLAP_CONFIG_DIR, f"run_{workload}.cnf")]
    
    memslap_run = sp.run(memslap_cmd, stdout=sp.PIPE, stderr=sp.DEVNULL,
        text=True, check=True, env=os.environ)

    rlines = memslap_run.stdout.split('\n')
    i = 0
    while not 'Net_rate' in rlines[i] and i < len(rlines):
        i += 1
    if i == len(rlines):
        raise RuntimeError("Can't find result in memslap output!")
    return rlines[i].split(' ')[8]

WORKLOADS = [95, 75, 25, 5]
THREADS = [1, 2, 4, 8, 16]
SAMPLE_SIZE = 5
NETSTAT_LINE = f"netstat -nap | grep memcached | grep {SERVER_PORT} | grep LISTEN"

def memcached_up():
    netstat_run = sp.run(NETSTAT_LINE, env=os.environ, stdout=sp.DEVNULL,
        stderr=sp.DEVNULL, shell=True)
    return (netstat_run.returncode == 0)

def run_config (name):
    memcached_bin = MEMCACHED_BINS[name]
    env_append = MEMCACHED_ENVS[name]
    memcached_env = copy.deepcopy(os.environ)
    for k, v in env_append.items():
        memcached_env[k] = v
    
    memcached_cmd = [memcached_bin, "-u", SERVER_USER, "-p", SERVER_PORT,
        "-l", SERVER_IP, "-t", "0"] # thread count must be last

    run_path = None
    owd = os.getcwd()
    if name in MEMCACHED_DIRS:
        run_path = MEMCACHED_DIRS[name]
    
    if PRINT_PROGRESS:
        print(f"RESULTS FOR MEMCACHED-{name}:")
    else:
        print(f"Running memcached-{name}...", end='', flush=True)
        
    with open(f"memcached-{name}.csv", "w") as data_fd:
        for T in THREADS:
            memcached_cmd[-1] = str(T)
            for W in WORKLOADS:
                for I in range(SAMPLE_SIZE):
                    # older memcached does not clean up when killed
                    for path in MEMCACHED_RESOURCES[name]:
                        rm_line = f"rm -rf {path}"
                        sp.run(rm_line, check=True, shell=True, env=os.environ)
                    if run_path is not None:
                        os.chdir(run_path)

                    memcached_proc = sp.Popen(memcached_cmd, stdout=sp.DEVNULL,
                        stderr=sp.DEVNULL, env=memcached_env)

                    if run_path is not None:
                        os.chdir(owd)
                    
                    while memcached_proc.poll() is None and not memcached_up():
                        pass
                    if memcached_proc.poll() is not None:
                        raise RuntimeError(f"Memcached exited early with code: {memcached_proc.returncode}")
                    else:
                        rate = run_memslap(W)
                        
                        memcached_proc.send_signal(signal.SIGTERM)
                        memcached_proc.wait(timeout=2)
                        if memcached_proc.returncode != MEMCACHED_RETURN[name]:
                            raise RuntimeError(f"Unexpected return from memcached: {memcached_proc.returncode}")
                    
                    result_line = f"{name},{T},{W},{I},{rate}"
                    data_fd.write(result_line)
                    if PRINT_PROGRESS:
                        print("\t" + result_line)
    if not PRINT_PROGRESS:
        print("done.")
                    
if __name__ == "__main__":
    config_name = sys.argv[1]
    if not config_name in MEMCACHED_BINS:
        print(f"Error: {config_name} is not a valid configuration!");
        print("Supported configurations:")
        for cn in MEMCACHED_BINS:
            print(f"\t{cn}")
        sys.exit(1)
    
    run_config(config_name)
