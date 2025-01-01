#include "table/vtable_manager.h"

#include "db/filename.h"
#include <iostream>
#include <ostream>

#include "leveldb/env.h"
#include "leveldb/status.h"

#include "util/coding.h"

namespace leveldb {

struct GCInfo {
  std::string dbname;
  std::vector<uint64_t> file_list;
  Env* env = nullptr;
};

void VTableMeta::Encode(std::string* target) const {
  PutVarint64(target, number);
  PutVarint64(target, records_num);
  PutVarint64(target, invalid_num);
  PutVarint64(target, table_size);
}

Status VTableMeta::Decode(Slice* input) {
  if (!GetVarint64(input, &number) || !GetVarint64(input, &records_num) ||
      !GetVarint64(input, &invalid_num) || !GetVarint64(input, &table_size)) {
    return Status::Corruption("Error Decode VTable meta");
  }
  return Status::OK();
}



void VTableManager::AddVTable(const VTableMeta& vtable_meta) {
  vtables_[vtable_meta.number] = vtable_meta;
}

void VTableManager::RemoveVTable(uint64_t file_num) {
  vtables_.erase(file_num);
}

Status VTableManager::AddInvalid(uint64_t file_num) {
  const auto it = vtables_.find(file_num);
  if (it == vtables_.end()) {
    return Status::Corruption("Invalid VTable number");
  }

  vtables_[file_num].invalid_num += 1;
  if (vtables_[file_num].invalid_num >= vtables_[file_num].records_num) {
    invalid_.emplace_back(file_num);
  }

  MaybeScheduleGarbageCollect();

  return Status::OK();
}

Status VTableManager::SaveVTableMeta() const {
  auto fname = VTableManagerFileName(dbname_);
  WritableFile* file;
  Status s = env_->NewWritableFile(fname, &file);
  if (!s.ok()) {
    return Status::Corruption("Failed to open vTable manager file");
  }

  const auto vtable_num = vtables_.size();
  std::string target;
  PutVarint64(&target, vtable_num);
  for (auto & vtable : vtables_) {
    vtable.second.Encode(&target);
  }
  s = file->Append(target);
  if (!s.ok()) {
    return Status::Corruption("Failed to append vTable manager file");
  }
  s = file->Flush();
  if (s.ok()) {
    s = file->Sync();
  }
  if (s.ok()) {
    s = file->Close();
  }
  delete file;
  file = nullptr;
  if (!s.ok()) {
    return Status::Corruption("Failed to write vTable meta file");
  }
  return Status::OK();
}

Status VTableManager::LoadVTableMeta() {
  auto fname = VTableManagerFileName(dbname_);
  if (!env_->FileExists(fname)) {
    return Status::OK();
  }
  SequentialFile* file;
  Status s = env_->NewSequentialFile(fname, &file);
  if (!s.ok()) {
    return Status::Corruption("Failed to open vTable manager file");
  }
  uint64_t file_size;
  s = env_->GetFileSize(fname, &file_size);
  if (!s.ok()) {
    return Status::Corruption("Failed to get vTable manager file size");
  }
  auto buf = new char[file_size];
  Slice input;
  s = file->Read(file_size, &input, buf);
  if (!s.ok()) {
    return Status::Corruption("Failed to read vTable manager file");
  }

  uint64_t vtable_num;
  if(!GetVarint64(&input, &vtable_num)) {
    return Status::Corruption("Failed to get vTable num");
  }

  for (int i = 0; i < vtable_num; i++) {
    VTableMeta vtable_meta;
    s = vtable_meta.Decode(&input);
    if (s.ok()) {
      if (vtable_meta.number == 0) {
        continue;
      }
      AddVTable(vtable_meta);
      if (vtable_meta.invalid_num >= vtable_meta.records_num) {
        invalid_.emplace_back(vtable_meta.number);
      }
    } else {
      return s;
    }
  }

  return s;
}

void VTableManager::MaybeScheduleGarbageCollect() {
  size_t size = 0;
  for (auto & file_num : invalid_) {
    size += vtables_[file_num].table_size;
  }
  if (size >= gc_threshold_) {
    auto* gc_info = new GCInfo;
    gc_info->dbname = dbname_;
    gc_info->file_list = invalid_;
    gc_info->env = env_;
    env_->StartThread(&VTableManager::BackgroudGC, gc_info);
    for (auto & file_num : gc_info->file_list) {
      RemoveVTable(file_num);
      auto it = std::remove(invalid_.begin(), invalid_.end(), file_num);
    }
  }
}

void VTableManager::BackgroudGC(void* gc_info) {
  auto info = reinterpret_cast<GCInfo*>(gc_info);
  for (auto & file_num : info->file_list) {
    auto fname = VTableFileName(info->dbname, file_num);
    info->env->RemoveFile(fname);
  }
  delete info;
}



} // namespace leveldb
