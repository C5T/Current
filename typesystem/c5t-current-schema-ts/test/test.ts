import * as fs from 'fs';
import * as path from 'path';

import * as iots from 'io-ts';
import { isRight } from 'fp-ts/lib/Either';
import { PathReporter } from 'io-ts/lib/PathReporter';

import { assert } from 'chai';

import * as smoke_test_struct_interface from './smoke_test_struct_interface';


describe('c5t-current-schema-ts', () => {
  it('validates serialized JSON via io-ts', () => {
    const full_test: smoke_test_struct_interface.FullTest = JSON.parse(String(fs.readFileSync(path.resolve(__dirname, './smoke_test_struct_serialized.json'))));
    const success_validation = iots.validate(full_test, smoke_test_struct_interface.FullTest_IO);
    assert.isTrue(isRight(success_validation));
    const success_report = PathReporter.report(success_validation);
    assert.deepEqual(success_report, [
      'No errors!',
    ]);
  });

  it('reports error on malformed JSON via io-ts', () => {
    const error_validation = iots.validate({
      primitives: {
        a: 'foo',
      },
    }, smoke_test_struct_interface.FullTest_IO);
    assert.isFalse(isRight(error_validation));
    const error_report = PathReporter.report(error_validation);
    assert.deepEqual(error_report, [
      'Invalid value "foo" supplied to : FullTest/primitives: Primitives/a: C5TCurrent.UInt8',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/b: C5TCurrent.UInt16',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/c: C5TCurrent.UInt32',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/d: C5TCurrent.UInt64',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/e: C5TCurrent.Int8',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/f: C5TCurrent.Int16',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/g: C5TCurrent.Int32',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/h: C5TCurrent.Int64',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/i: C5TCurrent.Char',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/j: C5TCurrent.String',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/k: C5TCurrent.Float',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/l: C5TCurrent.Double',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/m: boolean',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/n: C5TCurrent.Microseconds',
      'Invalid value undefined supplied to : FullTest/primitives: Primitives/o: C5TCurrent.Milliseconds',
      'Invalid value undefined supplied to : FullTest/v1: C5TCurrent.Vector<C5TCurrent.String>',
      'Invalid value undefined supplied to : FullTest/v2: C5TCurrent.Vector<Primitives>',
      'Invalid value undefined supplied to : FullTest/p: C5TCurrent.Pair<C5TCurrent.String, Primitives>',
      'Invalid value undefined supplied to : FullTest/o: C5TCurrent.Optional<Primitives>',
      'Invalid value undefined supplied to : FullTest/q: Variant_B_A_B_B2_C_Empty_E',
      'Invalid value undefined supplied to : FullTest/w1: Templated_T9209980946934124423',
      'Invalid value undefined supplied to : FullTest/w2: Templated_T9227782344077896555',
      'Invalid value undefined supplied to : FullTest/w3: Templated_T9209626390174323094',
      'Invalid value undefined supplied to : FullTest/w4: TemplatedInheriting_T9209980946934124423',
      'Invalid value undefined supplied to : FullTest/w4: TemplatedInheriting_T9209980946934124423',
      'Invalid value undefined supplied to : FullTest/w5: TemplatedInheriting_T9227782344077896555',
      'Invalid value undefined supplied to : FullTest/w5: TemplatedInheriting_T9227782344077896555',
      'Invalid value undefined supplied to : FullTest/w6: TemplatedInheriting_T9201673071807149456',
      'Invalid value undefined supplied to : FullTest/w6: TemplatedInheriting_T9201673071807149456',
      'Invalid value undefined supplied to : FullTest/tsc: TrickyEvolutionCases',
    ]);
  });
});
