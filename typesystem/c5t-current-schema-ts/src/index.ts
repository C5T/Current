import * as iots from 'io-ts';

const numberWithName = (name: string): iots.Type<number> => (
  {
    _A: iots._A,
    name: 'C5TCurrent.' + name,
    validate: iots.number.validate,
  }
);

export const Bool_IO = iots.boolean;
export type Bool = iots.TypeOf<typeof Bool_IO>;

export const UInt8_IO = numberWithName('UInt8');
export type UInt8 = iots.TypeOf<typeof UInt8_IO>;

export const UInt16_IO = numberWithName('UInt16');
export type UInt16 = iots.TypeOf<typeof UInt16_IO>;

export const UInt32_IO = numberWithName('UInt32');
export type UInt32 = iots.TypeOf<typeof UInt32_IO>;

export const UInt64_IO = numberWithName('UInt64');
export type UInt64 = iots.TypeOf<typeof UInt64_IO>;

export const Int8_IO = numberWithName('Int8');
export type Int8 = iots.TypeOf<typeof Int8_IO>;

export const Int16_IO = numberWithName('Int16');
export type Int16 = iots.TypeOf<typeof Int16_IO>;

export const Int32_IO = numberWithName('Int32');
export type Int32 = iots.TypeOf<typeof Int32_IO>;

export const Int64_IO = numberWithName('Int64');
export type Int64 = iots.TypeOf<typeof Int64_IO>;

export const Char_IO = numberWithName('Char');
export type Char = iots.TypeOf<typeof Char_IO>;

export const String_IO: iots.Type<string> = (
  {
    _A: iots._A,
    name: 'C5TCurrent.String',
    validate: iots.string.validate,
  }
);
export type String = iots.TypeOf<typeof String_IO>;

export const Float_IO = numberWithName('Float');
export type Float = iots.TypeOf<typeof Float_IO>;

export const Double_IO = numberWithName('Double');
export type Double = iots.TypeOf<typeof Double_IO>;

export const Microseconds_IO = numberWithName('Microseconds');
export type Microseconds = iots.TypeOf<typeof Microseconds_IO>;

export const Milliseconds_IO = numberWithName('Milliseconds');
export type Milliseconds = iots.TypeOf<typeof Milliseconds_IO>;

export const Optional_IO = <RT extends iots.Any>(Type_IO: RT, name?: string) => (
  iots.union([ Type_IO, iots.null ], name || ('C5TCurrent.Optional<' + Type_IO.name + '>'))
);
export type Optional<T> = T | null;

export const Pair_IO = <RT_FIRST extends iots.Any, RT_SECOND extends iots.Any>(FirstType_IO: RT_FIRST, SecondType_IO: RT_SECOND, name?: string) => (
  iots.tuple([ FirstType_IO, SecondType_IO ], name || ('C5TCurrent.Pair<' + FirstType_IO.name + ', ' + SecondType_IO.name + '>'))
);
export type Pair<T_FIRST, T_SECOND> = [ T_FIRST, T_SECOND ];

export const Enum_IO = (name?: string): iots.Type<number> => (
  {
    _A: iots._A,
    name: name || ('C5TCurrent.Enum'),
    validate: iots.number.validate,
  }
);
export type Enum = number;

export const Vector_IO = <RT_ITEM extends iots.Any>(Type_IO: RT_ITEM, name?: string) => (
  iots.array(Type_IO, name || ('C5TCurrent.Vector<' + Type_IO.name + '>'))
);
export type Vector<T_ITEM> = T_ITEM[];

export const Set_IO = <RT_ITEM extends iots.Any>(Type_IO: RT_ITEM, name?: string) => (
  iots.array(Type_IO, name || ('C5TCurrent.Set<' + Type_IO.name + '>'))
);
export type Set<T_ITEM> = T_ITEM[];

export const UnorderedSet_IO = <RT_ITEM extends iots.Any>(Type_IO: RT_ITEM, name?: string) => (
  iots.array(Type_IO, name || ('C5TCurrent.UnorderedSet<' + Type_IO.name + '>'))
);
export type UnorderedSet<T_ITEM> = T_ITEM[];

export const NonPrimitiveMap_IO = <RT_KEY extends iots.Any, RT_VALUE extends iots.Any>(KeyType_IO: RT_KEY, ValueType_IO: RT_VALUE, name?: string) => (
  iots.array(Pair_IO(KeyType_IO, ValueType_IO), name || ('C5TCurrent.NonPrimitiveMap<' + KeyType_IO.name + ', ' + ValueType_IO.name + '>'))
);
export type NonPrimitiveMap<T_KEY, T_VALUE> = Pair<T_KEY, T_VALUE>[];

export const NonPrimitiveUnorderedMap_IO = <RT_KEY extends iots.Any, RT_VALUE extends iots.Any>(KeyType_IO: RT_KEY, ValueType_IO: RT_VALUE, name?: string) => (
  iots.array(Pair_IO(KeyType_IO, ValueType_IO), name || ('C5TCurrent.NonPrimitiveUnorderedMap<' + KeyType_IO.name + ', ' + ValueType_IO.name + '>'))
);
export type NonPrimitiveUnorderedMap<T_KEY, T_VALUE> = Pair<T_KEY, T_VALUE>[];

export const PrimitiveMap_IO = <RT_VALUE extends iots.Any>(ValueType_IO: RT_VALUE, name?: string) => (
  iots.dictionary(iots.string, ValueType_IO, name || ('C5TCurrent.PrimitiveMap<' + ValueType_IO.name + '>'))
);
export interface PrimitiveMap<T_VALUE> { [key: string]: T_VALUE; }

export const PrimitiveUnorderedMap_IO = <RT_VALUE extends iots.Any>(ValueType_IO: RT_VALUE, name?: string) => (
  iots.dictionary(iots.string, ValueType_IO, name || ('C5TCurrent.PrimitiveUnorderedMap<' + ValueType_IO.name + '>'))
);
export interface PrimitiveUnorderedMap<T_VALUE> { [key: string]: T_VALUE; }
