# Current Symbols

Symbols listed without the namespace are exposed globally.

`TODO(dkorolev) + TODO(mzhurorich): Perhaps move them into `namespace current::global` and add `using namespace current::global` to the top-level `current.h`, guarded by an `#ifndef NO_CURRENT_GLOBAL` ?

Symbols prefixed with a namespace are not exposed globally.

### Macros

```
CURRENT_STRUCT, CURRENT_FIELD                  // Type system.

CURRENT_THROW                                  // Throws and logs line number.

CURRENT_STORAGE, CURRENT_STORAGE_FIELD         // Transactional storage.
CURRENT_STORAGE_STRUCT_TAG                     // To tag `CURRENT_STRUCT`-s as different types.
CURRENT_STORAGE_THROW_ROLLBACK                 // To indicate the transaction should be rolled back.

CURRENT_{LHS,VIA,RHS}, REGISTER_{LHS,VIA,RHS}  // RipCurrent types.
CURRENT_USER_TYPE                              // RipCurrent underlying user type extractor.

{DEFINE/DECLARE}_type                          // dflags
TEST()                                         // gtest
```

### Types

```
// Top-level Current exception. Can be `CURRENT_THROW`-n, includes file/line number,
// and results in transactions being rolled back if thrown from within one.
current::Exception

ParseJSONException, JSONSchemaException

TypeList<TS...>
Optional<T>
Variant<TS...> 
NoValue           // == const current::NoValueException&
NoValueOfType<T>  // == const current::NoValueOfTypeException<T>&

Request, Response
GET, HEAD, POST, PUT, DELETE
URL
current::HTTPHeaders
current::HTTPRoutesScope

FileSystem

current::StreamImpl<T>

current::MMQ<MESSAGE, CONSUMER>

current::Chunk, current::ChunkDB.

current::OptionalResult
current::TransactionResult

current::ImmutableOptional

current::Future

current::WaitableAtomic

current::Owned<T>
current::Borrowed<T>
current::BorrowedWithCallback<T>
current::BorrowedOfGuaranteedLifetime<T>
current::MakeOwned<T>
current::WeakBorrowed<T>
```

## Functions.

```
Value<>(x), Exists<>(x)
Clone(x)
current::CheckIntegrity(x)

MicroTimestampOf(x), SetMicroTimestamp(x, ts)

JSON(x), ParseJSON<T>(s)

HTTP(...)

RTTIDynamicCall(...)

current::Now()

current::Stream<T>(...)  // Returns current::StreamImpl<T>.

current::CRC32(x)
current::SHA256(x)
current::ROL64(x, n)

current::ToString    // Analogous to `std::to_string`, but supports enum classes too.
current::FromString  // Supports enum classes too.
current::Printf
current::Split, current::Join, current::SplitIntoKeyValuePairs
current::ToLower, current::ToUpper
current::Trim

MakeScopeGuard
MakePointerScopeGuard

current::Singleton, current::ThreadLocalSingleton

current::DelayedInstantiateFromTuple
current::DelayedInstantiateWithExtraParameterFromTuple

std::make_unique<T>(...)  // Until c++14 is mainstream, we injected our own.
```

### Enum classes

```
JSONFormat  // Current, Minimalistic, NewtonsoftFSharp.

current::Language (CPP, Current, Markdown, FSharp, InternalFormat)

current::ByLines, current::ByWhitespaces (for current::Split), current::EmptyFields, current::KeyValueParsing

current::ReRegisterRoute (HTTP)

current::MkDirParameters
current::ScanDirParameters
current::RmFileParameters
current::RmDirParameters
current::RmDirRecursive

current::HTTPResponseCodeValue

current::LazyInstantiationStrategy

current::StrictFuture

SherlockStreamPersister
```

### Metaprogramming

```
ENABLE_IF<PREDICATE, TYPE>  // using = typename std::enable_if<...>::type

current::decay<T>

current::copy_free<T>  // == T for POD and const T& for non-POD.

current::{map,filter,reduce}<...>
current::combine<...>

current::call_with_type<...>
```
