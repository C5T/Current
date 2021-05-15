const lib = require('bindings')('current-nodejs-integration-example') 

test('cppValues', () => {
  expect(lib.valueInt).toEqual(42);
  expect(lib.valueDouble).toEqual(3.14);
  expect(lib.valueString).toEqual("The Answer");
  expect(lib.valueTrue).toEqual(true);
  expect(lib.valueFalse).toEqual(false);
  expect(lib.valueNull).toEqual(null);
  expect(lib.valueNull).not.toEqual(undefined);
  expect(lib.valueUndefined).toEqual(undefined);
  expect(lib.valueUndefined).not.toEqual(null);
  expect(lib.valueObjectOne).toEqual({foo: 'Foo', bar: 42});
  expect(lib.valueObjectTwo).toEqual({foo: 'Two', bar: true});
  expect(lib.valueObjectNested).toEqual({a: { aa: 0, ab: 1 }, b: { ba: 2, bb: 3 }});
});

test('cppSyncSum', () => {
  expect(lib.cppSyncSum(1, 2)).toEqual(3);
});

test('cppSyncCallbackSum', (done) => {
  lib.cppSyncCallbackSum(3, 4, (result) => { expect(result).toEqual(7); done(); });
});

test('cppAsyncCallbackSum', (done) => {
  lib.cppAsyncCallbackSum(5, 6, (result) => { expect(result).toEqual(11); done(); });
});

test('cppFutureSum, then', (done) => {
  lib.cppFutureSum(1, 2).then((x) => { expect(x).toEqual(3); done(); });
});

test('cppFutureSum, expect.resolves.toEqual', () => {
  return expect(lib.cppFutureSum(1, 2)).resolves.toEqual(3);
});

test('cppFutureSum, async/await', async () => {
  expect(await lib.cppFutureSum(3, 4)).toEqual(7);
});

test('cppReturnsNull', () => {
  expect(lib.cppReturnsNull()).toEqual(null);
  expect(lib.cppReturnsNull()).not.toEqual(undefined);
});

test('cppReturnsUndefined', () => {
  expect(lib.cppReturnsUndefined()).toEqual(undefined);
  expect(lib.cppReturnsUndefined()).not.toEqual(null);
});

test('cppReturnsUndefinedII', () => {
  expect(lib.cppReturnsUndefinedII()).toEqual(undefined);
  expect(lib.cppReturnsUndefinedII()).not.toEqual(null);
});

test('cppSyncCallbacksABA', (done) => {
  var r = [];
  lib.cppSyncCallbacksABA(
    (x) => { r.push('a' + x); },
    (x) => { r.push('b' + x); }
  ).then(
    () => { expect(r.join('|')).toEqual('a1|b2|a:three'); done(); }
  );
});

test('cppAsyncCallbacksABA', (done) => {
  var r = [];
  lib.cppAsyncCallbacksABA(
    (x) => { r.push('a' + x); },
    (x, y, z, p, q) => { r.push('b' + x + y + z + p + q); }
  ).then(
    () => { expect(r.join('|')).toEqual('a-test|b:here:null:3.14:|a-passed'); done(); }
  );
});

test('cppReturnsObject', () => {
  expect(lib.cppReturnsObject()).toEqual({supports: "also", dot_notation: true, _null: null, _undefined: undefined });
});

test('cppModifiesObject', () => {
  var obj = { a: 1, b: 2};
  lib.cppModifiesObject(obj);
  expect(obj).toEqual({a: 1, b: 2, sum: 3, sum_as_string: '3'});
});

test('cppWrapsFunction', () => {
  expect(lib.cppWrapsFunction(1, (f) => { return f(2); })).toEqual('Outer 1, inner 2.');
  expect(lib.cppWrapsFunction(100, (f) => { return f(42); })).toEqual('Outer 100, inner 42.');
});

test('cppGetsResultsOfJsFunctions', () => {
  expect(lib.cppGetsResultsOfJsFunctions(() => { return 'A'; }, () => { return 'B'; })).toEqual('AB');
});

test('cppGetsResultsOfJsFunctionsAsync', async () => {
  expect(await lib.cppGetsResultsOfJsFunctionsAsync(() => { return 'C'; }, () => { return 'D'; })).toEqual('CD');
});

test('cppMultipleAsyncCallsAtArbitraryTimes', async () => {
  var checks = [];
  var primes = [];
  const promise = lib.cppMultipleAsyncCallsAtArbitraryTimes(
    (x) => { checks.push(x); return x < 17; },
    (x) => { primes.push(x); });
  await promise;
  expect(checks).toStrictEqual([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17]);
  expect(primes).toStrictEqual([2,3,5,7,11,13]);
});
