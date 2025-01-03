#include "fields.h"
#include "util/coding.h"

namespace leveldb {

  Fields::Fields(const FieldArray& field_array) {
     assert(!field_array.empty());
     for (const auto& field : field_array) {
       this->_fields[field.first] = field.second;
       this->size_ += field.first.size() + field.second.size();
     }
   }

  Fields::Fields(const Slice& fields_str) {
    Slice fields = fields_str;
    while (fields.size() > 0) {
      uint64_t field_size;
      uint64_t name_size;

      GetVarint64(&fields, &field_size);

      Slice field = Slice(fields.data(), field_size);
      GetVarint64(&field, &name_size);

      Slice name = Slice(field.data(), name_size);
      Slice value = Slice(field.data() + name_size, field.size() - name_size);

      this->_fields[name.ToString()] = value.ToString();
      this->size_ += name.ToString().size() + value.ToString().size();

      fields = Slice(fields.data() + field_size, fields.size() - field_size);
    }
  }

  Fields::~Fields() {
    this->_fields.clear();
  }

  std::string& Fields::operator[](const std::string& field_name) {
    return this->_fields[field_name];
  }

  std::string Fields::Serialize() const {
    std::string fields_str;

    for (const auto & _field : this->_fields) {
      std::string field_str;
      std::string field_name = _field.first;
      std::string field_value = _field.second;

      uint64_t name_size = field_name.size();

      PutVarint64(&field_str, name_size);
      field_str.append(field_name);
      field_str.append(field_value);

      PutVarint64(&fields_str, field_str.size());
      fields_str.append(field_str);
    }

    // const Slice fields = Slice(fields_str);
    return fields_str;
  }

  FieldArray Fields::GetFieldArray() const {
    FieldArray field_array;

    for (const auto& _field : this->_fields) {
      field_array.emplace_back(_field.first, _field.second);
    }

    return field_array;
  }
}  // namespace leveldb