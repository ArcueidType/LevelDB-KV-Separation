#ifndef VTABLE_READER_H
#define VTABLE_READER_H

#include <memory>

#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

#include "util/coding.h"

#include "vtable_format.h"

namespace leveldb {

class VTableReader {
  public:
    Status Open(const Options& options, std::string fname);

    Status Get(const VTableHandle& handle,
               VTableRecord* record) const ;
  private:
    Options options_;
    RandomAccessFile* file_{nullptr};
};

} // namespace leveldb

#endif //VTABLE_READER_H
