#include "gtest/gtest.h"
#include "leveldb/env.h"
#include "leveldb/db.h"
using namespace leveldb;

constexpr int value_size = 2000;
constexpr int data_size = 128 << 20;

std::map<std::string, std::string> data;

Status OpenDB(std::string dbName, DB **db) {
  Options options;
  options.create_if_missing = true;
  return DB::Open(options, dbName, db);
}

void InsertData(DB *db) {
  WriteOptions writeOptions;
  int key_num = data_size / value_size;
  srand(0);

  for (int i = 0; i < key_num; i++) {
    int key_ = rand() % key_num+1;
    std::string key = std::to_string(key_);
    std::string value(value_size, 'a');
    db->Put(writeOptions, key, value);
    data[key] = value;
  }
}

TEST(TestBasicIO, PointGet) {
  DB *db;
  if(OpenDB("testdb", &db).ok() == false) {
    std::cerr << "open db failed" << std::endl;
    abort();
  }

  data.clear();
  InsertData(db);

  ReadOptions readOptions;

  for (auto iter = data.begin(); iter != data.end(); ++iter) {
    std::string value;
    db->Get(readOptions, iter->first, &value);
    ASSERT_TRUE(value == iter->second);
  }

  delete db;
}

TEST(TestBasicIO, RangeQuery) {
  DB *db;
  if(OpenDB("testdb", &db).ok() == false) {
    std::cerr << "open db failed" << std::endl;
    abort();
  }

  data.clear();
  InsertData(db);

  ReadOptions readOptions;

  auto iter = db->NewIterator(readOptions);
  iter->SeekToFirst();
  while (iter->Valid()) {
    ASSERT_TRUE(data[iter->key().ToString()] == iter->fields()["1"]);
    iter->Next();
  }
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
