# Current + EJS Demo

## Implementation

* The binary in the `cpp` directory is a Current binary listening on port 3000. It responds with a JSON to a curl-issued GET request as `curl http://localhost:3000?a=10&b=20`. When the same request comes from the browser, the binary from the `cpp` directory assumes the rendering tool, implemented in node.js and running on the same machine on port 3001, can be used to convert the JSON representation of the output into an HTML-beautified one.

* The tool in the `nodejs` directory listens on port 3001, and responds with an HTML to POST requests containing the JSON of the corresponding format in the HTTP body.

## Details

Just `make` in the `EJS/cpp` directory would build and start a simple server on port 3000:

```
$ curl "http://localhost:3000?a=10&b=20"
{"a":10,"b":20,"x":[11,13,17,19]}
```

Indepenently from the C++ binary, running `npm i && npm run start` from `EJS/nodejs` would start another, node.js, rendering server on port 3001:

```
$ curl -H "Content-Type: application/json" -d '{"a":99,"b":101,"x":[999]}' localhost:3001
<h1>Primes from 99 to 101</h1>
<ul>
   <li>999</li> 
</ul>
```

Once both servers are running, "HTML" "beautification", where the C++ code accepts the request and node.js code renders the HTML, would work just like this:

```
$ curl -H "Accept: text/html" "localhost:3000?a=10&b=20"
<h1>Primes from 10 to 20</h1>
<ul>
   <li>11</li>  <li>13</li>  <li>17</li>  <li>19</li> 
</ul>
```

And if that's too nerdy, just open [http://localhost:3000/?a=10&b=20] from the browser.
