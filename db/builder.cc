// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/builder.h"

#include "db/dbformat.h"
#include "db/filename.h"
#include "db/table_cache.h"
#include "db/version_edit.h"

#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"

#include "table/vtable_builder.h"
#include "table/vtable_manager.h"

namespace leveldb {

Status BuildTable(const std::string& dbname, Env* env, const Options& options,
                  TableCache* table_cache, Iterator* iter, FileMetaData* meta,
                  VTableMeta* vtable_meta) {
  Status s;
  meta->file_size = 0;
  iter->SeekToFirst();

  std::string fname = TableFileName(dbname, meta->number);
  std::string vtb_name = VTableFileName(dbname, meta->number);
  if (iter->Valid()) {
    WritableFile* file;
    s = env->NewWritableFile(fname, &file);
    if (!s.ok()) {
      return s;
    }

    WritableFile* vtb_file;
    s = env->NewWritableFile(vtb_name, &vtb_file);
    if (!s.ok()) {
      return s;
    }

    TableBuilder* builder = new TableBuilder(options, file);
    VTableBuilder* vtb_builder = new VTableBuilder(options, vtb_file);
    meta->smallest.DecodeFrom(iter->key());
    Slice key;
    for (; iter->Valid(); iter->Next()) {
      key = iter->key();
      Slice value = iter->value();
      if (value.size() < options.kv_sep_size) {
        // No need to separate key and value
        builder->Add(key, value);
      }
      else {
        // Separate key value
        ParsedInternalKey parsed;
        if (!ParseInternalKey(key, &parsed)) {
          s = Status::Corruption("Fatal. Memtable Key Error");
          builder->Abandon();
          vtb_builder->Abandon();
          return s;
        }
        value.remove_prefix(1);
        VTableRecord record {parsed.user_key, value};
        VTableHandle handle;
        VTableIndex index;
        std::string value_index;
        vtb_builder->Add(record, &handle);

        index.file_number = meta->number;
        index.vtable_handle = handle;
        index.Encode(&value_index);
        builder->Add(key, Slice(value_index));
      }
    }
    if (!key.empty()) {
      meta->largest.DecodeFrom(key);
    }

    // Finish and check for builder errors
    s = builder->Finish();
    if (s.ok()) {
      meta->file_size = builder->FileSize();
      assert(meta->file_size > 0);
    }
    delete builder;

    // Finish and check for file errors
    if (s.ok()) {
      s = file->Sync();
    }
    if (s.ok()) {
      s = file->Close();
    }
    delete file;
    file = nullptr;

    if (s.ok()) {
      s = vtb_builder->Finish();
    }
    if (s.ok()) {
      vtable_meta->number = meta->number;
      vtable_meta->table_size = vtb_builder->FileSize();
      vtable_meta->records_num = vtb_builder->RecordNumber();
    }
    delete vtb_builder;

    if (s.ok()) {
      s = vtb_file->Sync();
    }
    if (s.ok()) {
      s = vtb_file->Close();
    }
    delete vtb_file;
    vtb_file = nullptr;

    if (s.ok()) {
      // Verify that the table is usable
      Iterator* it = table_cache->NewIterator(ReadOptions(), meta->number,
                                              meta->file_size);
      s = it->status();
      delete it;
    }
  }

  // Check for input iterator errors
  if (!iter->status().ok()) {
    s = iter->status();
  }

  if (s.ok() && meta->file_size > 0) {
    // Keep it
  } else {
    env->RemoveFile(fname);
  }
  if (s.ok() && vtable_meta->table_size > 0) {
    // Keep it
  } else {
    env->RemoveFile(vtb_name);
  }
  return s;
}

}  // namespace leveldb
