import * as fs from 'fs';
import * as path from 'path';

const GOLDEN_SMOKE_TEST_STRUCT_TS_PATH = path.resolve(__dirname, '../../golden/smoke_test_struct.ts');
const TEST_GOLDEN_SMOKE_TEST_STRUCT_TS_PATH = path.resolve(__dirname, './smoke_test_struct_interface.ts');

const SERIALIZED_SMOKE_TEST_STRUCT_JSON_PATH = path.resolve(__dirname, '../../serialized/smoke_test_struct.json');
const TEST_SERIALIZED_SMOKE_TEST_STRUCT_JSON_PATH = path.resolve(__dirname, './smoke_test_struct_serialized.json');

const goldenSmokeTestStructContent = fs.readFileSync(GOLDEN_SMOKE_TEST_STRUCT_TS_PATH);
const goldenSmokeTestStructContentUpdated = String(goldenSmokeTestStructContent).replace(
  "import * as C5TCurrent from 'c5t-current-schema-ts';",
  "import * as C5TCurrent from '../src/index';"
);
fs.writeFileSync(TEST_GOLDEN_SMOKE_TEST_STRUCT_TS_PATH, goldenSmokeTestStructContentUpdated);

fs.writeFileSync(TEST_SERIALIZED_SMOKE_TEST_STRUCT_JSON_PATH, fs.readFileSync(SERIALIZED_SMOKE_TEST_STRUCT_JSON_PATH));
