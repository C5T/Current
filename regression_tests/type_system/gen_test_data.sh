#!/bin/bash

INCLUDE_DIR="./include"
GOLDEN_DIR="./golden"

rm -rf $INCLUDE_DIR
rm -rf $GOLDEN_DIR
mkdir -p $INCLUDE_DIR
mkdir -p $GOLDEN_DIR

STRUCT_COUNT=100
if [[ $# -gt 0 ]]; then
	STRUCT_COUNT=$1
fi
echo -e "\033[1m\033[33mGenerating data for $STRUCT_COUNT structures.\033[0m\033[39m"

# 'current_struct.cc' test.
CURRENT_STRUCT_HEADER="$INCLUDE_DIR/current_struct.h"
CURRENT_STRUCT_TEST="$INCLUDE_DIR/current_struct.cc"
CURRENT_STRUCT_GOLDEN="$GOLDEN_DIR/current_struct.cc"

echo "namespace current_userspace {" >> $CURRENT_STRUCT_GOLDEN
for i in `seq 1 $STRUCT_COUNT`; do
	echo "CURRENT_STRUCT(Struct$i) {" >> $CURRENT_STRUCT_HEADER
	echo "  CURRENT_FIELD(x$i, uint32_t);" >> $CURRENT_STRUCT_HEADER
	echo "};" >> $CURRENT_STRUCT_HEADER

	echo "schema.AddType<Struct$i>();" >> $CURRENT_STRUCT_TEST

	echo "struct Struct$i {" >> $CURRENT_STRUCT_GOLDEN
	echo "  uint32_t x$i;" >> $CURRENT_STRUCT_GOLDEN
	echo "};" >> $CURRENT_STRUCT_GOLDEN
done
echo "}  // namespace current_userspace" >> $CURRENT_STRUCT_GOLDEN

# 'struct_fields.cc' test.
STRUCT_FIELDS_HEADER="$INCLUDE_DIR/struct_fields.h"
STRUCT_FIELDS_GOLDEN="$GOLDEN_DIR/struct_fields.cc"

echo "CURRENT_STRUCT(StructWithManyFields) {" >> $STRUCT_FIELDS_HEADER
echo "namespace current_userspace {" >> $STRUCT_FIELDS_GOLDEN
echo "struct StructWithManyFields {" >> $STRUCT_FIELDS_GOLDEN
for i in `seq 1 $STRUCT_COUNT`; do
	echo "  CURRENT_FIELD(z$i, uint32_t);" >> $STRUCT_FIELDS_HEADER
	echo "  uint32_t z$i;" >> $STRUCT_FIELDS_GOLDEN
done
echo "};" >> $STRUCT_FIELDS_HEADER
echo "};" >> $STRUCT_FIELDS_GOLDEN
echo "}  // namespace current_userspace" >> $STRUCT_FIELDS_GOLDEN

# 'typelist.cc' test.
TYPELIST_HEADER="$INCLUDE_DIR/typelist.h"
TYPELIST_TEST="$INCLUDE_DIR/typelist.cc"
echo "typedef TypeList<" >> $TYPELIST_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "struct Struct$i { enum { x = $i }; };" >> $TYPELIST_HEADER
	echo -n "Struct$i" >> $TYPELIST_TEST
	if [[ $i -ne $STRUCT_COUNT ]]; then
		echo "," >> $TYPELIST_TEST
	fi
done
echo "> TYPELIST;" >> $TYPELIST_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "EXPECT_EQ($i, (TypeListElement<$(($i - 1)), TYPELIST>::x));" >> $TYPELIST_TEST
done

# 'typelist_impl.cc' test.
TYPELIST_IMPL_HEADER="$INCLUDE_DIR/typelist_impl.h"
TYPELIST_IMPL_TEST="$INCLUDE_DIR/typelist_impl.cc"
echo "typedef TypeListImpl<" >> $TYPELIST_IMPL_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "struct Struct$i { enum { x = $i }; };" >> $TYPELIST_IMPL_HEADER
	echo -n "Struct$i" >> $TYPELIST_IMPL_TEST
	if [[ $i -ne $STRUCT_COUNT ]]; then
		echo "," >> $TYPELIST_IMPL_TEST
	fi
done
echo "> TYPELIST_IMPL;" >> $TYPELIST_IMPL_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "EXPECT_EQ($i, (TypeListElement<$(($i - 1)), TYPELIST_IMPL>::x));" >> $TYPELIST_IMPL_TEST
done

# 'rtti_dynamic_call.cc' test.
RTTI_DYNAMIC_CALL_HEADER="$INCLUDE_DIR/rtti_dynamic_call.h"
RTTI_DYNAMIC_CALL_TEST="$INCLUDE_DIR/rtti_dynamic_call.cc"
RTTI_DYNAMIC_CALL_GOLDEN="$GOLDEN_DIR/rtti_dynamic_call.output"
echo "typedef TypeListImpl<" >> $RTTI_DYNAMIC_CALL_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "CURRENT_STRUCT(Struct$i) {" >> $RTTI_DYNAMIC_CALL_HEADER
	echo "  CURRENT_FIELD(x$i, uint32_t, $i);" >> $RTTI_DYNAMIC_CALL_HEADER
	echo "  void print_x$i(std::ostream& os) const { os << \"x$i=\" << x$i; };" >> $RTTI_DYNAMIC_CALL_HEADER
	echo "};" >> $RTTI_DYNAMIC_CALL_HEADER

	echo -n "Struct$i" >> $RTTI_DYNAMIC_CALL_TEST
	if [[ $i -ne $STRUCT_COUNT ]]; then
		echo "," >> $RTTI_DYNAMIC_CALL_TEST
	fi

	echo -n "x$i=$i" >> $RTTI_DYNAMIC_CALL_GOLDEN
done
echo "> TYPELIST;" >> $RTTI_DYNAMIC_CALL_TEST
echo "struct CallStruct {" >> $RTTI_DYNAMIC_CALL_HEADER
echo "  std::ostringstream oss;" >> $RTTI_DYNAMIC_CALL_HEADER
for i in `seq 1 $STRUCT_COUNT`; do
	echo "  void operator()(const Struct$i& s) { s.print_x$i(oss); }" >> $RTTI_DYNAMIC_CALL_HEADER
	echo "Struct$i s$i;" >> $RTTI_DYNAMIC_CALL_TEST
	echo "const current::CurrentSuper& p$i = s$i;" >> $RTTI_DYNAMIC_CALL_TEST
done
echo "};" >> $RTTI_DYNAMIC_CALL_HEADER

echo "CallStruct call_struct;" >> $RTTI_DYNAMIC_CALL_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "RTTIDynamicCall<TYPELIST>(p$i, call_struct);" >> $RTTI_DYNAMIC_CALL_TEST
done

# 'typelist_dynamic.cc' test.
TYPELIST_DYNAMIC_HEADER="$INCLUDE_DIR/typelist_dynamic.h"
TYPELIST_DYNAMIC_TEST="$INCLUDE_DIR/typelist_dynamic.cc"
echo "typedef TypeListImpl<" >> $TYPELIST_DYNAMIC_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "CURRENT_STRUCT(A$i) {};" >> $TYPELIST_DYNAMIC_HEADER
	echo "CURRENT_STRUCT(D$i) {};" >> $TYPELIST_DYNAMIC_HEADER
	echo "CURRENT_STRUCT(Struct$i) { using Add = A$i; using Delete = D$i; };" >> $TYPELIST_DYNAMIC_HEADER
	echo -n "Struct$i" >> $TYPELIST_DYNAMIC_TEST
	if [[ $i -ne $STRUCT_COUNT ]]; then
		echo "," >> $TYPELIST_DYNAMIC_TEST
	fi
done
echo "> DATA_TYPES;" >> $TYPELIST_DYNAMIC_TEST

#`variant.cc` test
VARIANT_HEADER="$INCLUDE_DIR/variant.h"
VARIANT_TEST="$INCLUDE_DIR/variant.cc"
VARIANT_GOLDEN="$GOLDEN_DIR/variant.output"
echo "typedef Variant<" >> $VARIANT_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "CURRENT_STRUCT(Struct$i) {" >> $VARIANT_HEADER
	echo "  CURRENT_FIELD(x$i, uint32_t, $i);" >> $VARIANT_HEADER
	echo "  void print_x$i(std::ostream& os) const { os << \"x$i=\" << x$i; };" >> $VARIANT_HEADER
	echo "};" >> $VARIANT_HEADER

	echo -n "Struct$i" >> $VARIANT_TEST
	if [[ $i -ne $STRUCT_COUNT ]]; then
		echo "," >> $VARIANT_TEST
	fi

	echo -n "x$i=$i" >> $VARIANT_GOLDEN
done
echo "> VARIANT_TYPE;" >> $VARIANT_TEST
echo "struct CallStruct {" >> $VARIANT_HEADER
echo "  std::ostringstream oss;" >> $VARIANT_HEADER
for i in `seq 1 $STRUCT_COUNT`; do
	echo "  void operator()(const Struct$i& s) { s.print_x$i(oss); }" >> $VARIANT_HEADER
done
echo "};" >> $VARIANT_HEADER

echo "CallStruct call_struct;" >> $VARIANT_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "VARIANT_TYPE v$i((Struct$i()));" >> $VARIANT_TEST
	echo "v$i.Call(call_struct);" >> $VARIANT_TEST
done

# 'storage.cc' test.
STORAGE_HEADER="$INCLUDE_DIR/storage.h"
STORAGE_TEST="$INCLUDE_DIR/storage.cc"

echo "storage.ReadWriteTransaction([](MutableFields<TestStorage> fields) {" >> $STORAGE_TEST
for i in `seq 1 $STRUCT_COUNT`; do
	echo "CURRENT_STRUCT(Struct$i) {" >> $STORAGE_HEADER
	echo "  CURRENT_FIELD(x$i, uint32_t);" >> $STORAGE_HEADER
	echo "  CURRENT_USE_FIELD_AS_KEY(x$i);" >> $STORAGE_HEADER
	echo "  CURRENT_CONSTRUCTOR(Struct$i)(uint32_t i = 0u) : x$i(i) {}" >> $STORAGE_HEADER
	echo "};" >> $STORAGE_HEADER
	echo "CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, Struct$i, StorageStruct$i);" >> $STORAGE_HEADER

	echo "  fields.v$i.Add(Struct$i(${i}u));" >> $STORAGE_TEST
done
echo "}).Wait();" >> $STORAGE_TEST

echo "storage.ReadOnlyTransaction([](ImmutableFields<TestStorage> fields) {" >> $STORAGE_TEST
echo "CURRENT_STORAGE(Storage) {" >> $STORAGE_HEADER
for i in `seq 1 $STRUCT_COUNT`; do
	echo "  CURRENT_STORAGE_FIELD(v$i, StorageStruct$i);" >> $STORAGE_HEADER

	echo "  EXPECT_EQ(${i}u, Value(fields.v$i[$i]).x$i);" >> $STORAGE_TEST
done
echo "};" >> $STORAGE_HEADER
echo "}).Wait();" >> $STORAGE_TEST


touch dummy.h
