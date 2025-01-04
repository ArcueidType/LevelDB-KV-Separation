#include "table/vtable_manager.h"

#include "db/dbformat.h"
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
  const auto it = vtables_.find(file_num);
  if (it == vtables_.end()) { return; }
  vtables_.erase(it);
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
  auto delete_list = std::vector<uint64_t>();
  if (!invalid_.empty()) {
    auto invalid = std::set<uint64_t>(invalid_.begin(), invalid_.end());
    invalid_ = std::vector<uint64_t>(invalid.begin(), invalid.end());
    for (auto & file_num : invalid) {
      if (vtables_.find(file_num) != vtables_.end() && vtables_[file_num].ref <= 0) {
        size += vtables_[file_num].table_size;
        delete_list.emplace_back(file_num);
      }
    }
    if (size >= gc_threshold_) {
      for (auto & file_num : delete_list) {
        auto it = std::remove(invalid_.begin(), invalid_.end(), file_num);
        RemoveVTable(file_num);
      }
      auto* gc_info = new GCInfo;
      gc_info->dbname = dbname_;
      gc_info->file_list = delete_list;
      gc_info->env = env_;
      env_->StartThread(&VTableManager::BackgroudGC, gc_info);
      // for (auto & file_num : gc_info->file_list) {
      //   RemoveVTable(file_num);
      //   auto it = std::remove(invalid_.begin(), invalid_.end(), file_num);
      // }
    }
  }
}

void VTableManager::BackgroudGC(void* gc_info) {
  auto info = reinterpret_cast<GCInfo*>(gc_info);
  std::cout << "gc starts..." << std::endl;
  auto start_time = std::chrono::steady_clock::now();
  for (auto & file_num : info->file_list) {
    // if (file_num <= 0) {continue;}
    auto fname = VTableFileName(info->dbname, file_num);
    info->env->RemoveFile(fname);
  }
  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  std::cout << "gc lasts: " << duration << "micros" << std::endl;
}

void VTableManager::RefVTable(uint64_t file_num) {
  vtables_[file_num].ref += 1;
}

void VTableManager::UnrefVTable(uint64_t file_num) {
  vtables_[file_num].ref -= 1;
}

} // namespace leveldb
