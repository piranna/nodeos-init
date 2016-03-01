# NodeOS-init

NodeOS-init is a simple init that only reap zombie processes and shutdown the
system when there's no more running ones. It was initially based on
[sinit](http://core.suckless.org/sinit), at the same time based on Rich Felker's
[minimal init](https://gist.github.com/rofl0r/6168719).

## Why?

NodeOS is an operating system build entirely on Node.js, and also it was its
`PID 1` init process ([century](https://github.com/NodeOS/node-century)).
Problem is, on v0.11.15 Node.js upgraded the version of v8 from v3.26.33 to
v3.28.73, and a regression was introducced that don't allow to use it as `PID 1`
anymore, seems related to the need of having a `devtmpfs` filesystem mounted on
`/dev` and not only defined a `/dev/console` device file. Due to this, we need
to mount it previously to any instance of Node.js can be executed, but also we
can this way control better the processes termination and cleanly shutdown the
system instead of get a *Kernel panic*.

## How?

There are 3 signals that NodeOS-init will act on.

* *SIGCHLD*: reap children
* *SIGINT*:  reboots the machine (or alternatively via `ctrl-alt-del`)
* *SIGTERM*: send the `SIGTERM` to all its child processes and later shutdown

It also shutdown the machine when it detect there are no more child processes.
