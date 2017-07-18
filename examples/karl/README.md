# Karl + Claire Demo

After `make -j all`, run the binaries in separate terminals in the following order:

* `./.current/service_karl`
* `./.current/service_generator`
* `./.current/service_is_prime`
* `./.current/service_annotator`
* `./.current/service_filter`

Alternatively, if you're not afraid to `pkill "service_*"`, copy-paste the following command:

```
(./.current/service_karl &) ; \
(./.current/service_generator &) ; \
(./.current/service_is_prime &) ; \
(./.current/service_annotator &) ; \
(./.current/service_filter &) ; \
sleep 3 && echo "Enjoy."
```

As all the binaries are running:

* Each binary prints a URL to `curl` to standard out.
* [TODO] Karl's URL can be open in a browser for a fucking awesome SVG.
* [TODO] On each binary's port, `http://localhost:${port}/.current` is its status page.
