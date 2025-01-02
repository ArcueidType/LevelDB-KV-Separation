#include <string>

#include "leveldb/env.h"

#include "table/vtable_reader.h"

namespace leveldb {
  Status VTableReader::Open(const Options& options, std::string fname) {
    options_ = options;
    return options_.env->NewRandomAccessFile(fname, &file_);
  }

  Status VTableReader::Get(const VTableHandle& handle,
                           VTableRecord* record) const {
    auto buf = new char[handle.size];
    Slice input;
    Status s = file_->Read(handle.offset, handle.size, &input, buf);

    if (!s.ok()) {
      return s;
    }
    if (handle.size != static_cast<uint64_t>(input.size())) {
      return Status::Corruption("Read input size not equal to record size: " +
                                std::to_string(input.size()) + ":" +
                                std::to_string(handle.size));
    }

    RecordDecoder decoder;
    s = decoder.DecodeHeader(&input);
    if (!s.ok()) {
      return s;
    }

    s = decoder.DecodeRecord(&input, record);
    return s;
  }


} // namespace leveldb