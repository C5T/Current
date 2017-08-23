import * as fs from 'fs';
import * as path from 'path';

import * as iots from 'io-ts';
import { PathReporter } from 'io-ts/lib/PathReporter';
import { ThrowReporter } from 'io-ts/lib/ThrowReporter';

import { assert } from 'chai';

import * as smoke_test_struct_interface from './smoke_test_struct_interface';


describe('c5t-current-schema-ts', () => {
  it('validates the serialized JSON via io-ts', () => {
    const full_test: smoke_test_struct_interface.FullTest = JSON.parse(String(fs.readFileSync(path.resolve(__dirname, './smoke_test_struct_serialized.json'))));
    const success_validation = iots.validate(full_test, smoke_test_struct_interface.FullTest_IO);
    const success_report = PathReporter.report(success_validation);
    assert.deepEqual(success_report, [
      'No errors!',
    ]);
    ThrowReporter.report(success_validation);

    // Pay attention to the Variant types first. -- @sompylasar
    // TODO(@sompylasar): Test more field values.

    assert.isTrue(iots.is(full_test.q, smoke_test_struct_interface.C5TCurrentVariant_T9228482442669086788_IO));

    function handleVariantQCC(variantQCC: smoke_test_struct_interface.MyFreakingVariant) {
      assert.isTrue(iots.is(variantQCC, smoke_test_struct_interface.MyFreakingVariant_IO));

      assert.isTrue(iots.is(variantQCC, smoke_test_struct_interface.MyFreakingVariant_VariantCase_A_IO));
      const variantQCCA = (variantQCC as smoke_test_struct_interface.MyFreakingVariant_VariantCase_A).A;
      if (variantQCCA) {
        assert.strictEqual(variantQCCA.a, (1 << 30), 'full_test.q.c.a is not (1 << 30)');
      }
      else {
        assert.isOk(false, 'full_test.q.c is not Variant<A,B,B2,C,Empty> case A');
      }
    }

    function handleVariantQCD(variantQCD: smoke_test_struct_interface.MyFreakingVariant) {
      assert.isTrue(iots.is(variantQCD, smoke_test_struct_interface.MyFreakingVariant_IO));

      assert.isTrue(iots.is(variantQCD, smoke_test_struct_interface.MyFreakingVariant_VariantCase_Y_IO));
      const variantQCDY = (variantQCD as smoke_test_struct_interface.MyFreakingVariant_VariantCase_Y).Y;
      if (variantQCDY) {
        // The enum E value SOME (0).
        assert.strictEqual(variantQCDY.e, 0, 'full_test.q.d.e is not enum E value SOME (0)');
      }
      else {
        assert.isOk(false, 'full_test.q.d is not Variant<A,X,Y> case Y');
      }
    }

    function handleVariantQ(variantQ: smoke_test_struct_interface.C5TCurrentVariant_T9228482442669086788) {
      assert.isTrue(iots.is(variantQ, smoke_test_struct_interface.C5TCurrentVariant_T9228482442669086788_VariantCase_C_IO));
      const variantQC = (variantQ as smoke_test_struct_interface.C5TCurrentVariant_T9228482442669086788_VariantCase_C).C;
      if (variantQC) {
        handleVariantQCC(variantQC.c);
        handleVariantQCD(variantQC.d);
      }
      else {
        assert.isOk(false, 'full_test.q is not Variant<A,B,B2,C,Empty> case C');
      }
    }

    handleVariantQ(full_test.q);
  });

  it('reports error on malformed JSON via io-ts', () => {
    const error_validation = iots.validate({
      primitives: {
        a: 'foo',
      },
    }, smoke_test_struct_interface.FullTest_IO);

    assert.throws(() => {
      ThrowReporter.report(error_validation);
    });

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
      'Invalid value undefined supplied to : FullTest/q: C5TCurrentVariant_T9228482442669086788',
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
