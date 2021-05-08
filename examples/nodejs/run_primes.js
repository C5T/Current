#!/usr/bin/env node

const lib = require('bindings')('current-nodejs-integration-example') ;

const k = 19;
var z = [];
console.log(`Generating the primes up to and not including ${k}.`);
lib.cppMultipleAsyncCallsAtArbitraryTimes(
  (i) => { z.push(i); return i < k; },
  (p) => { console.log(`Prime (via the 2nd callback): ${p}.`); }
).then((c) => {
  console.log(`Done, total primes output (the resolved promise): ${c}.`);
  console.log(`All comparison checks run (via the 1st callback): ${z}.`);
});
