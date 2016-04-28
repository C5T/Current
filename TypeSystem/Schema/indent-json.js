#!/usr/bin/env node

// JavaScript is the cleanest way to format JSON w/o reordering the fields.
// Note: It would mess up int64 values that don't fit 53 bits. Oh well. There are none in our schema. -- D.K.
var fs = require('fs');

fs.writeFileSync(
  'golden/smoke_test_struct.json',
  JSON.stringify(JSON.parse(fs.readFileSync('golden/smoke_test_struct.json').toString()), null, 2))

fs.writeFileSync(
  'golden/smoke_test_struct.internal_json',
  JSON.stringify(JSON.parse(fs.readFileSync('golden/smoke_test_struct.internal_json').toString()), null, 2))
