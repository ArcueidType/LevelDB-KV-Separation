#ifndef VTABLE_FORMAT_H
#define VTABLE_FORMAT_H

#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "table/format.h"

namespace leveldb {

const uint64_t kRecordHeaderSize = 4;

// VTable最基本的存储单位，表示存储的一个key和一个value
struct VTableRecord {
  Slice key;
  Slice value;

  // 将record编码为str
  void Encode(std::string* target) const;
  // 将Slice解码为record
  Status Decode(Slice* input);

  // 该record的size
  size_t size() const { return key.size() + value.size(); }

  friend bool operator==(const VTableRecord& a, const VTableRecord& b) {
    return a.key == b.key && a.value == b.value;
  }
};

class RecordEncoder {
  public:
    // TODO: Support compression while encoding a record
    RecordEncoder() = default;

    // 编码一条vTable record
    void Encode(const VTableRecord& record);

    // 获得编码后的records的size
    size_t GetEncodedSize() const { return sizeof(header_) + record_.size(); }

    // 获取编码后的header
    Slice GetHeader() const { return {header_, sizeof(header_)}; }

    // 获得编码后的record
    Slice GetRecord() const { return record_; }
  private:
    char header_[kRecordHeaderSize];
    Slice record_;

    std::string record_buff_;
};

class RecordDecoder {
  public:

    // 解码出record的header
    Status DecodeHeader(Slice* input);

    // 解码出record
    Status DecodeRecord(Slice* input, VTableRecord* record) const;

    // 获得解码后的record size
    size_t GetDecodedSize() const { return record_size_; }

  private:
    uint32_t record_size_{0};
};

struct VTableHandle {
  // 表示某个record在VTable中的位置
  uint64_t offset{0};
  uint64_t size{0};

  void Encode(std::string* target) const;
  Status Decode(Slice* input);

  friend bool operator==(const VTableHandle& a, const VTableHandle& b) {
    return a.offset == b.offset && a.size == b.size;
  }
};

struct VTableIndex {
  // 存入sstable中的index
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

// 便利的调用解码方法的函数
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
