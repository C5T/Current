const lib = require('bindings')('current-nodejs-integration-example') 

test('cppValues', () => {
  expect(lib.valueInt).toBe(42);
  expect(lib.valueDouble).toBe(3.14);
  expect(lib.valueString).toBe("The Answer");
  expect(lib.valueTrue).toBe(true);
  expect(lib.valueFalse).toBe(false);
  expect(lib.valueNull).toBe(null);
});

test('cppSyncSum', () => {
  expect(lib.cppSyncSum(1, 2)).toBe(3);
});

test('cppSyncCallbackSum', (done) => {
  lib.cppSyncCallbackSum(3, 4, (result) => { expect(result).toBe(7); done(); });
});

test('cppAsyncCallbackSum', (done) => {
  lib.cppAsyncCallbackSum(5, 6, (result) => { expect(result).toBe(11); done(); });
});

test('cppFutureSum, expect.resolves.toBe', () => {
  return expect(lib.cppFutureSum(1, 2)).resolves.toBe(3);
});

test('cppFutureSum, async/await', async () => {
  expect(await lib.cppFutureSum(3, 4)).toBe(7);
});

test('cppSyncCallbacksABA', (done) => {
  var r = [];
  lib.cppSyncCallbacksABA(
    (x) => { r.push('a' + x); },
    (x) => { r.push('b' + x); }
  ).then(
    () => { expect(r.join('-')).toBe('a1-b2-a:three'); done(); }
  );
});

test('cppAsyncCallbacksABA', (done) => {
  var r = [];
  lib.cppAsyncCallbacksABA(
    (x) => { r.push('a' + x); },
    (x, y, z, p, q) => { r.push('b' + x + y + z + p + q); }
  ).then(
    () => { expect(r.join('-')).toBe('a-test-b:here:null:3.14:-a-passed'); done(); }
  );
});

test('cppWrapsFunction', () => {
  expect(lib.cppWrapsFunction(1, (f) => { return f(2); })).toBe('Outer 1, inner 2.');
  expect(lib.cppWrapsFunction(100, (f) => { return f(42); })).toBe('Outer 100, inner 42.');
});

test('cppGetsResultsOfJsFunctions', () => {
  expect(lib.cppGetsResultsOfJsFunctions(() => { return 'A'; }, () => { return 'B'; })).toBe('AB');
});

test('cppGetsResultsOfJsFunctionsAsync', async () => {
  expect(await lib.cppGetsResultsOfJsFunctionsAsync(() => { return 'C'; }, () => { return 'D'; })).toBe('CD');
});

