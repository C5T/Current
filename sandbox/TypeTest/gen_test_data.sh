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

for i in `seq 1 $STRUCT_COUNT`; do
	echo "CURRENT_STRUCT(Struct$i) {" >> $CURRENT_STRUCT_HEADER
	echo "  CURRENT_FIELD(x$i, uint32_t);" >> $CURRENT_STRUCT_HEADER
	echo "};" >> $CURRENT_STRUCT_HEADER

	echo "schema.AddType<Struct$i>();" >> $CURRENT_STRUCT_TEST

	echo "struct Struct$i {" >> $CURRENT_STRUCT_GOLDEN
	echo "  uint32_t x$i;" >> $CURRENT_STRUCT_GOLDEN
	echo "};" >> $CURRENT_STRUCT_GOLDEN
done

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
#for i in `seq 1 $STRUCT_COUNT`; do
#	echo "EXPECT_EQ($i, (TypeListElement<$(($i - 1)), TYPELIST_DYNAMIC>::x));" >> $TYPELIST_DYNAMIC_TEST
#done

