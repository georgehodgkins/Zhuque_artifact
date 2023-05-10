set startup-with-shell off
set environment LD_RELOAD=goob;0.00005
!rm -rf goob && mkdir goob
handle SIGPWR nostop
handle SIGUSR2 nostop
