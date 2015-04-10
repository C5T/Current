# KVStorage Design Doc
**KVStorage** is a layer of abstraction above **Sherlock** stream, which implements a key-value storage functionality. It is responsible for keeping in-memory state and data consistency. KVStorage provides API to Create, Read, Update and Delete records (CRUD).

## Data Structure
Each data structure which is supposed to be used in KVStorage should be serializable itself and have serializable **Key** and **Value** members.

### Key
Key structure should implement: 
* `HashFunction` to provide single unique identifier (type?)
* Comparison operator `==`

## API Structure
There are three groups of access methods:
* **Get** methods
* **Set** methods
* **Delete** methods

For each method there are three types of implementation:
* Synchronous
* Asynchronous via `std::future`
* Asynchronous with callbacks

## Get
### Synchronous version
```cpp
ValueType Get[TypeName](const KeyType& key);
```

If key does not exist, throws an exception.

### Asynchronous version
```cpp
std::future<ValueType> AsyncGet[TypeName](const KeyType& key);
```

If key does not exist, throws an exception.

### Callback version
```cpp
void CallbackGet[TypeName](const KeyType& key,
                           std::function<void(ValueType& value)> on_found,
                           std::function<void(KeyType& key)> on_not_found);
```


## Set
All Set methods create new record if key does not exist and modify existing record otherwise.
```cpp
void Set[TypeName](const KeyType& key, const ValueType& value);

std::future<void> AsyncSet[TypeName](const KeyType& key, const ValueType& value);

void CallbackSet[TypeName](const KeyType& key, const ValueType& value,
                           std::function<void(KeyType& key)> on_write);
```


## Delete

## Multiple types support

## Data consistency
