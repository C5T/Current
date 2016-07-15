#include "../../../Storage/storage.h"
#include "../../../Storage/persister/sherlock.h"

CURRENT_ENUM(EntryID, uint64_t);

CURRENT_STRUCT(Entry) {
  CURRENT_FIELD(key, EntryID);
  CURRENT_FIELD(value, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(Entry){};
  CURRENT_CONSTRUCTOR(Entry)(EntryID id, const std::string& value) : key(id), value(value) {}
};

CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, Entry, EntryDict);

CURRENT_STORAGE(TestStorage) { CURRENT_STORAGE_FIELD(entries, EntryDict); };

using storage_t = TestStorage<SherlockStreamPersister>;
