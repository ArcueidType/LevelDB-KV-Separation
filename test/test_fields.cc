#include <iostream>
#include <gtest/gtest.h>

#include "leveldb/db.h"
#include "leveldb/env.h"

using namespace std;
using namespace leveldb;

Status OpenDB(std::string dbName, DB **db) {
  Options options;
  options.create_if_missing = true;
  return DB::Open(options, dbName, db);
}

template<typename T>
// Return true if v1 and v2 have same elements
bool CompareVector(const std::vector<T>& v1, const std::vector<T>& v2) {
  if (v1.size() != v2.size()) {
    return false;
  } else {
    std::set<T> compare_set(v1.begin(), v1.end());
    for (const auto &ele : v2) {
      if (compare_set.insert(ele).second == true) {
        return false;
      }
    }
  }
  return true;
}

TEST(TestFields, GetPutIterator) {
  DB *db;
  if(OpenDB("testdb", &db).ok() == false) {
    cerr << "Open DB Failed" << endl;
  }

  std::string key_1 = "k_1";
  std::string key_2 = "k_2";

  FieldArray field_array_1 = {
    {"name", "Arcueid01"},
    {"address", "tYpeMuuN"},
    {"phone", "122-233-4455"}
  };
  FieldArray field_array_2 = {
    {"name", "Arcueid02"},
    {"address", "tYpeMuuN"},
    {"phone", "199-999-2004"}
  };

  const auto fields_1 = Fields(field_array_1);
  const auto fields_2 = Fields(field_array_2);
  db->Put(WriteOptions(), key_1, fields_1);
  db->Put(WriteOptions(), key_2, fields_2);

  Fields ret;
  db->Get(ReadOptions(), key_1, &ret);
  const auto fields_ret = ret.GetFieldArray();

  ASSERT_EQ(CompareVector<Field>(fields_ret, field_array_1), true);

  db->Get(ReadOptions(), key_2, &ret);
  ASSERT_EQ(ret["name"], "Arcueid02");
  ASSERT_EQ(ret["address"], "tYpeMuuN");
  ASSERT_EQ(ret["phone"], "199-999-2004");

  auto iter = db->NewIterator(ReadOptions());
  iter->SeekToFirst();
  while (iter->Valid()) {
    auto key = iter->key().ToString();
    auto fields = iter->fields();
    if (key == "k_1") {
      ASSERT_EQ(fields["name"], "Arcueid01");
      ASSERT_EQ(fields["address"], "tYpeMuuN");
      ASSERT_EQ(fields["phone"], "122-233-4455");
    }
    if (key == "k_2") {
      ASSERT_EQ(fields["name"], "Arcueid02");
      ASSERT_EQ(fields["address"], "tYpeMuuN");
      ASSERT_EQ(fields["phone"], "199-999-2004");
    }
    iter->Next();
  }

  delete iter;
  delete db;
}

TEST(TestFields, SearchKey) {
  DB *db;
  if(OpenDB("testdb", &db).ok() == false) {
    cerr << "Open DB Failed" << endl;
  }

  std::vector<std::string> keys_have_field = {"k_1", "k_3"};
  std::vector<std::string> keys_wo_field = {"k_2", "k_4"};
  Field field_test = {"test_name", "Harry"};
  FieldArray field_array_have_field = {
    {"name", "Arcueid"},
    {"address", "tYpeMuuN"},
    {"phone", "122-233-4455"},
    field_test
  };
  FieldArray field_array_wo_field = {
      {"name", "Arcueid"}, {"address", "tYpeMuuN"}, {"phone", "122-233-4455"}};

  const auto fields_have_field = Fields(field_array_have_field);
  const auto fields_wo_field = Fields(field_array_wo_field);
  for(const auto& key : keys_have_field){
    db->Put(WriteOptions(), key, fields_have_field);
  }
  for (const auto& key : keys_wo_field) {
    db->Put(WriteOptions(), key, fields_wo_field);
  }

  const std::vector<std::string> key_ret = db->FindKeysByField(field_test);

  ASSERT_EQ(CompareVector<std::string>(key_ret, keys_have_field), true);

  delete db;
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
