#ifndef VTABLE_BUILDER_H
#define VTABLE_BUILDER_H

#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "table/vtable_format.h"
#include "util/coding.h"

namespace leveldb {

class VTableBuilder {
  public:
    VTableBuilder(const Options& options, WritableFile* file);

    // Add a record to the vTable
    void Add(const VTableRecord& record, VTableHandle* handle);

    // Builder status, return non-ok iff some error occurs
    Status status() const { return status_; }

    // Finish building the vTable
    Status Finish();

    // Abandon building the vTable
    void Abandon();
  private:
    bool ok() const { return status().ok(); }

    WritableFile* file_;
    uint64_t file_size_{0};

    Status status_;

    RecordEncoder encoder_;
};

} // namespace leveldb

#endif //VTABLE_BUILDER_H
