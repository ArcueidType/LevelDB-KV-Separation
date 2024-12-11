#include <iostream>
#include <gtest/gtest.h>

#include "leveldb/db.h"
#include "leveldb/env.h"
#include "table/vtable_builder.h"
#include "table/vtable_reader.h"
#include "table/vtable_format.h"

using namespace std;
using namespace leveldb;

TEST(TestVTable, BuilderReader) {
  VTableHandle handle1;
  VTableHandle handle2;
  VTableRecord record1;
  VTableRecord record2;
  record1.key = "001";
  record1.value = "value1";
  record2.key = "002";
  record2.value = "value2";

  Options opt;
  WritableFile *file;
  opt.env->NewWritableFile("1.vtb", &file);
  VTableBuilder builder(opt, file);

  builder.Add(record1, &handle1);
  builder.Add(record2, &handle2);
  builder.Finish();

  VTableReader reader;
  reader.Open(opt, "1.vtb");

  VTableRecord res_record;
  reader.Get(handle2, &res_record);

  ASSERT_TRUE(res_record.key.ToString() == record2.key.ToString());
  ASSERT_TRUE(res_record.value.ToString() == record2.value.ToString());

  reader.Get(handle1, &res_record);

  ASSERT_TRUE(res_record.key.ToString() == record1.key.ToString());
  ASSERT_TRUE(res_record.value.ToString() == record1.value.ToString());
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}