# SimpleServer

A reference implementation of an HTTP server. Stub data collector.

# Example usage

## Binary

```
$ make
$ while true ; do date ; ./.noshit/example 2>&1 | tee output.txt ; done
```

## Port Forwarding

```
$ cat /etc/nginx/sites-enabled/80_to_8686
server {
  location / {
    proxy_pass http://0.0.0.0:8686/;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
  }
}
```
