## Replicating a lot of data over two legs, Follower 2 <- Follower 1 <- Master.

```
[ terminal 1 ] $ ./.current/replications_per_second  -n 3 -m 10000000

[ terminal 2 ] $ while true ; do (curl -s localhost:8502/stream?sizeonly; curl -s localhost:8501/stream?sizeonly ; curl -s localhost:8500/stream?sizeonly) | paste - - - ; sleep 1 ; done
```

Example output:

```
...

293154  293168  10000000
295068  295082  10000000
296998  297012  10000000
```

=> **The lag is effectively zero (`curl` time eats up more), and the replication speed is a but under 2K records per second.**

## Change two legs into 99 legs (`-n 100 -m 10000000`), and change `localhost:8592` into `localhost:8599` in the second command.

Example output:

```
...
107987  108003  10000000
108581  108599  10000000
109182  109193  10000000
```

=> **Same picture, thus adding more legs doesn't make the end-to-end replication slower, thus the lag is indeed negligible.**
