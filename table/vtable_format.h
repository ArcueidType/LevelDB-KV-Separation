#ifndef VTABLE_FORMAT_H
#define VTABLE_FORMAT_H

#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "table/format.h"

namespace leveldb {

const uint64_t kRecordHeaderSize = 4;

struct VTableRecord {
  Slice key;
  Slice value;

  void Encode(std::string* target) const;
  Status Decode(Slice* input);

  size_t size() const { return key.size() + value.size(); }

  friend bool operator==(const VTableRecord& a, const VTableRecord& b) {
    return a.key == b.key && a.value == b.value;
  }
};

class RecordEncoder {
  public:
    // TODO: Support compression while encoding a record
    RecordEncoder() = default;

    // Encode a vTable record
    void Encode(const VTableRecord& record);

    // Get the size of encoded record
    size_t GetEncodedSize() const { return sizeof(header_) + record_.size(); }

    // Get the header
    Slice GetHeader() const { return {header_, sizeof(header_)}; }

    // Get the encoded record
    Slice GetRecord() const { return record_; }
  private:
    char header_[kRecordHeaderSize];
    Slice record_;

    std::string record_buff_;
};

class RecordDecoder {
  public:
    Status DecodeHeader(Slice* input);

    Status DecodeRecord(Slice* input, VTableRecord* record) const;

    size_t GetDecodedSize() const { return record_size_; }

  private:
    uint32_t record_size_{0};
};

struct VTableHandle {
  uint64_t offset{0};
  uint64_t size{0};

  void Encode(std::string* target) const;
  Status Decode(Slice* input);

  friend bool operator==(const VTableHandle& a, const VTableHandle& b) {
    return a.offset == b.offset && a.size == b.size;
  }
};

struct VTableIndex {
  enum Type : unsigned char {
    kVTableIndex = 1,
  };

  uint64_t file_number{0};
  VTableHandle vtable_handle;

  void Encode(std::string* target) const;
  Status Decode(Slice* input);

  friend bool operator==(const VTableIndex& a, const VTableIndex& b) {
    return a.file_number == b.file_number && a.vtable_handle == b.vtable_handle;
  }
};

template <typename T>
Status DecodeSrcIntoObj(const Slice& src, T* target) {
  Slice input = src;
  Status s = target->Decode(&input);
  if (s.ok() && !input.empty()) {
    s = Status::Corruption(Slice());
  }
  return s;
}

} // namespace leveldb
#endif //VTABLE_FORMAT_H
