# HTTP Logs Access Cheatsheet

1. Test connectivity: `/raw_log/schema.md`
   1. Also try `.json`, `.fs`, `.cpp`.
1. Number of entries in the log: `/raw_log?sizeonly`
   1. Or `HEAD` instead of `GET`, to get the response in the header. 
1. When don't need an infinite stream (i.e., when `-f` is not required from this `tail`, ex. from the browser):
   1. TL;DR: Add `&nowait`.
   1. Alternate means of capping the output: `&n=`, `&period=`, and `&stop_after_bytes=`. 
1. When in the browser, add `&array`. This makes the whole output "page" a valid JSON. 
1. A milder version of `&array` is `&entries_only`.
   1. Without `&entries_only` or `&array`, it's going to be one log entry per line as two tab-separated JSONs.
   1. The first tab-column of the full output is the way to get the true timestamp of the log event.
   1. Timestamps and periods are generally epoch microseconds (use `date -d "yesterday" +"%s000000"`, with six zeroes at the end, to create one. Unless you're on OS X, in which case sorry.)
1. Reference access patterns:
   1. `/raw_log?tail=1&nowait`
   1. `/raw_log?tail=100&array&nowait`
   1. `/raw_log?i=1000000&n=100`
   1. `/raw_log?i=1000000&nowait`
   1. `/raw_log?i=100&period=100000000&array`
   1. `/raw_log?since=1473380843835579&stop_after_bytes=10000&array`
1. JSON formats:
   1. `&json=js` for JavaScript-friendly JSONs (no numerical type ID), and
   1. `&json=fs` for F#-friendly JSONs (see the `/raw_log/schema.fs` above).
1. Recap for `/raw_log`:
   1. Left boundary: `&i=<index>` | `&since=<microseconds timestamp>` | `&tail=<index starting from the end>` | `&recent=<same as tail but in microseconds instead of indexes>`.
   1. Right boundary: `&n=<count>` | `&period=<microseconds range>` | `&stop_after_bytes=<bytes>` | `&nowait`.
   1. JSON layout: `&json=js` hides type IDs, and `&json=fs` is F#-friendly.
   1. Default format is one event per line, as two `'\t'`-separated JSONs: `{index,timestamp}` and event body.
      1 `&entries_only` surpasses the 1st col, and `&array`, makes the output one large JSON array of the 2nd col.
   1. Special endpoints: `?sizeonly` for the todal number of entries, and `/raw_low/schema.{json,cpp,fs,h}` for the schema.
