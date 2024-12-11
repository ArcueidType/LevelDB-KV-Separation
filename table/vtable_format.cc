#include "table/vtable_format.h"

#include <locale>

#include "util/coding.h"

namespace leveldb {

namespace {

bool GetChar(Slice* input, unsigned char* value) {
  if (input->empty()) {
    return false;
  }
  *value = *input->data();
  input->remove_prefix(1);
  return true;
}

}

void VTableRecord::Encode(std::string* target) const {
  PutLengthPrefixedSlice(target, key);
  PutLengthPrefixedSlice(target, value);
}

Status VTableRecord::Decode(Slice* input) {
  if (!GetLengthPrefixedSlice(input, &key) ||
      !GetLengthPrefixedSlice(input, &value)) {
    return Status::Corruption("Error decode VTableRecord");
  }
  return Status::OK();
}

void RecordEncoder::Encode(const VTableRecord& record) {
  record_buff_.clear();

  record.Encode(&record_buff_);
  record_ = Slice(record_buff_.data(), record_buff_.size());

  assert(record.size() < std::numeric_limits<uint32_t>::max());

  EncodeFixed32(header_, static_cast<uint32_t>(record_.size()));
}

Status RecordDecoder::DecodeHeader(Slice* input) {
  if (!GetFixed32(input, &record_size_)) {
    return Status::Corruption("Error decode record header");
  }
  return Status::OK();
}

Status RecordDecoder::DecodeRecord(Slice* input, VTableRecord* record) const {
  Slice record_input(input->data(), record_size_);
  input->remove_prefix(record_size_);

  return DecodeSrcIntoObj(record_input, record);
}

void VTableHandle::Encode(std::string* target) const {
  PutVarint64(target, offset);
  PutVarint64(target, size);
}

Status VTableHandle::Decode(Slice* input) {
  if (!GetVarint64(input, &offset) || !GetVarint64(input, &size)) {
    return Status::Corruption("Error decode VTableHandle");
  }
  return Status::OK();
}

void VTableIndex::Encode(std::string* target) const {
  target->push_back(kVTableIndex);
  PutVarint64(target, file_number);
  vtable_handle.Encode(target);
}

Status VTableIndex::Decode(Slice* input) {
  unsigned char type;
  if (!GetChar(input, &type) || type != kVTableIndex ||
      !GetVarint64(input, &file_number)) {
    return Status::Corruption("Error decode VTableIndex");
  }

  Status s = vtable_handle.Decode(input);
  if (!s.ok()) {
    return Status::Corruption("Error decode VTableHandle", s.ToString());
  }
  return s;
}

} // namespace leveldb
