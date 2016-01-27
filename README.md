# NodeOS-init

NodeOS-init is a simple init that only reap zombie processes and shutdown the
system when there's no more running ones. It was initially based on
[sinit](http://core.suckless.org/sinit), at the same time based on Rich Felker's
[minimal init](https://gist.github.com/rofl0r/6168719).

## Why?

NodeOS is an operating system build entirely on Node.js, also it's `PID 1` init
[century](https://github.com/NodeOS/node-century). Problem is, since v0.11.15
the version of v8 was changed from v3.26.33 to v3.28.73, and a bug was inserted
that forbid to use it as `PID 1` anymore. Due to this, we need to use a more
"traditional" C-based init executable.

## How?

There are 3 signals that sinit will act on.

* *SIGCHLD*: reap children
* *SIGINT*:  reboots the machine (or alternatively via ctrl-alt-del)
* *SIGTERM*: send the `SIGTERM` to all its child processes and later shutdown

It also shutdown the machine when it detect there are no more child processes.
