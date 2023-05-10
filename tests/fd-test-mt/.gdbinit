set startup-with-shell off
set environment LD_RELOAD=goob
!rm -rf goob && mkdir goob
handle SIGPWR nostop
handle SIGUSR2 nostop
