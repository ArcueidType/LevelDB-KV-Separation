#include "table/vtable_builder.h"

#include "leveldb/env.h"

namespace leveldb {

VTableBuilder::VTableBuilder(const Options& options, WritableFile* file)
  : file_(file),
    encoder_() {}

void VTableBuilder::Add(const VTableRecord& record, VTableHandle* handle) {
  if (!ok()) return;

  encoder_.Encode(record);
  handle->offset = file_size_;
  handle->size = encoder_.GetEncodedSize();
  file_size_ += encoder_.GetEncodedSize();

  status_ = file_->Append(encoder_.GetHeader().ToString() +
                          encoder_.GetRecord().ToString());

  assert(ok());
  //TODO: meta info support in the future
}

Status VTableBuilder::Finish() {
  if (!ok()) return status();

  status_ = file_->Flush();

  return status();
}

void VTableBuilder::Abandon() { }

} // namespace leveldb