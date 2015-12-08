#!/usr/bin/env node

var fs = require('fs');
fs.writeFileSync(
  'golden/smoke_test_struct.json',
  JSON.stringify(JSON.parse(fs.readFileSync('golden/smoke_test_struct.json').toString()), null, 2))
