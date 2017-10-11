#!/usr/bin/env node

// JavaScript is the cleanest way to format JSON w/o reordering the fields.
// Note: It would mess up int64 values that don't fit 53 bits. Oh well. There are none in our schema. -- D.K.
var fs = require('fs');

function indent(filepath) {
  fs.writeFileSync(
    filepath,
    JSON.stringify(JSON.parse(fs.readFileSync(filepath).toString()), null, 2));
}

indent('golden/smoke_test_struct.json');
indent('golden/smoke_test_struct.internal_json');
indent('serialized/smoke_test_struct.json');
