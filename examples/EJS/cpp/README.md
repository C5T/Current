# Current + EJS Demo

Just `make` in this directory would build and start a simple server on port 3000:

```
$ curl "localhost:3000?a=10&b=20"
{"a":10,"b":20,"x":[11,13,17,19]}
```

Adding `npm run start` from `../nodejs` would start another, node.js, rendering server on port 3001:

```
$ curl -H "Content-Type: application/json" -d '{"a":99,"b":101,"x":[999]}' localhost:3001
<h1>Primes from 99 to 101</h1>
<ul>
   <li>999</li> 
</ul>
```

Once both servers are running, "HTML" "beautification" would work just like this:

```
$ curl -H "Accept: text/html" "localhost:3000?a=10&b=20"
<h1>Primes from 10 to 20</h1>
<ul>
   <li>11</li>  <li>13</li>  <li>17</li>  <li>19</li> 
</ul>
```

And if that's too nerdy, just open [http://localhost:3000/?a=10&b=20] from the browser.
