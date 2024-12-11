#include "table/vtable_builder.h"

namespace leveldb {

VTableBuilder::VTableBuilder(const Options& options, WritableFile* file)
  : file_(file) {}

void VTableBuilder::Add(const VTableRecord& record, VTableHandle* handle) {
  if (!ok()) return;


}


} // namespace leveldb