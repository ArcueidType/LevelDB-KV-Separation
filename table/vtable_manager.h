#ifndef VTABLE_MANAGER_H
#define VTABLE_MANAGER_H

#include <map>

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

struct VTableMeta {
  uint64_t number;

  uint64_t records_num;

  uint64_t invalid_num;

  uint64_t table_size;

  void Encode(std::string* target) const;
  Status Decode(Slice* input);

  VTableMeta() : number(0), records_num(0), invalid_num(0), table_size(0) {}
};

class VTableManager {
  public:
    explicit VTableManager(const std::string& dbname, Env* env, size_t gc_threshold) :
  dbname_(dbname),
  env_(env),
  gc_threshold_(gc_threshold) {}

    ~VTableManager() = default;

    void AddVTable(const VTableMeta& vtable_meta);

    void RemoveVTable(uint64_t file_num);

    Status AddInvalid(uint64_t file_num);

    Status SaveVTableMeta() const;

    Status LoadVTableMeta();

    void MaybeScheduleGarbageCollect();

    static void BackgroudGC(void* gc_info);

  private:
    std::string dbname_;
    Env* env_;
    std::map<uint64_t, VTableMeta> vtables_;
    std::vector<uint64_t> invalid_;
    size_t gc_threshold_;
};

} // namespace leveldb
#endif //VTABLE_MANAGER_H
