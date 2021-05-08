#!/usr/bin/env node

const cpp = require('bindings')('current-nodejs-external-server') ;

cpp.spawnServer(
  (port, registerer) => { 
    console.log(`[ node.js   ] The first server is being spawned on port ${port}.`);
    registerer('/one', (body, callback) => { 
      console.log(`[ node.js   ] JS callback on 'http://localhost:${port}/one'.`);
      callback(`[ http      ] One: responding to ${JSON.stringify(body)}.\n`);
    });
    console.log(`[ node.js   ] The first server is configured.`);
}).then((port) => {
  console.log(`[ node.js   ] The first server, on port ${port}, is 'DELETE'-d.`);
});

cpp.spawnServer(
  (port, registerer) => { 
    console.log(`[ node.js   ] The second server is being spawned on port ${port}.`);
    registerer('/two', (body, callback) => { 
      console.log(`[ node.js   ] JS callback on 'http://localhost:${port}/two'.`);
      callback(`[ http      ] Two: responding to ${JSON.stringify(body)}.\n`);
    });
    console.log(`[ node.js   ] The second server is configured.`);
}).then((port) => {
  console.log(`[ node.js   ] The second server, on port ${port}, is 'DELETE'-d.`);
});

console.log(`[ node.js   ] The two { JavaScript + C++ } servers are initialized, waiting for them to be 'DELETE'-d.`);
