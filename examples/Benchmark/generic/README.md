## NOTE

On Ubuntu/Linux, run `sudo sysctl net.ipv4.tcp_tw_reuse=1` to avoid issues with `TIME_WAIT` sockets for high-load local performance tests.

## `Benchmark/Primes`

The "is a random number between one and one million prime" benchmark, comparing:

* local, no-HTTP, run
* simple HTTP server,
* a `Storage`-based solution, and
* a `Storage`-based solution with "authentication".

TODO(dkorolev): Run instructions.
