#ifndef VTABLE_READER_H
#define VTABLE_READER_H

#include <memory>

#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

#include "util/coding.h"

#include "vtable_format.h"
#include "vtable_manager.h"

namespace leveldb {

class VTableReader {
  public:
    VTableReader() = default;

    VTableReader(uint64_t fnum, VTableManager *manager) :
      fnum_(fnum),
      manager_(manager) {};

    Status Open(const Options& options, std::string fname);

    Status Get(const VTableHandle& handle,
               VTableRecord* record) const ;

    void Close();
  private:
    Options options_;
    uint64_t fnum_;
    RandomAccessFile* file_{nullptr};
    VTableManager* manager_{nullptr};
};

} // namespace leveldb

#endif //VTABLE_READER_H
