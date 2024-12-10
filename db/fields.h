#ifndef STORAGE_LEVELDB_FIELDS_H_
#define STORAGE_LEVELDB_FIELDS_H_

#include <map>
#include <string>
#include <vector>

#include "leveldb/slice.h"

namespace leveldb {

  using Field = std::pair<std::string, std::string>;
  using FieldArray = std::vector<std::pair<std::string, std::string>>;

  class Fields {
  public:
    // 从FieldArray构建Fields
    explicit Fields(const FieldArray& field_array);

    // 从LevelDB存储的Value中解码出Fields
    explicit Fields(const Slice& fields_str);

    Fields() = default;

    ~Fields();

    // 重载[]运算符简便对字段的修改和访问操作
    std::string& operator[](const std::string& field_name);

    // 获取当前Fields对应的FieldArray
    FieldArray GetFieldArray() const;

    // 将Fields编码为存入LevelDB的Value
    std::string Serialize() const;

  private:
    std::map<std::string, std::string> _fields;
  };
}  // namespace leveldb
#endif //STORAGE_LEVELDB_FIELDS_H_
