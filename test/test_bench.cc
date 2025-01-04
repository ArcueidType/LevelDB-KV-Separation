#include <iostream>
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <random>
#include "leveldb/env.h"
#include "leveldb/db.h"

using namespace leveldb;

// Number of key/values to operate in database
constexpr int num_ = 100000;
// Size of each value
constexpr int value_size_ = 1000;
// Number of read operations
constexpr int reads_ = 100000;
// Number of findkeysbyfield operations
constexpr int search_ = 20;

int64_t bytes_ = 0;


Status OpenDB(std::string dbName, DB **db) {
  Options options;
  options.create_if_missing = true;
  return DB::Open(options, dbName, db);
}

// DB::Put()
void InsertData(DB *db, std::vector<int64_t> &lats) {
  WriteOptions writeOptions;
  bytes_ = 0;
  int64_t bytes = 0;
  srand(0);
  std::mt19937 value_seed(100);
  std::uniform_int_distribution<int> value_range(10, 2048);
  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;

  for (int i = 0; i < num_; i++) {
    int key_ = rand() % num_+1;
    int value_ = std::rand() % (num_ + 1);
    int value_size = value_range(value_seed);
    std::string value(value_size, 'a');
    std::string key = std::to_string(key_);
    FieldArray field_array = {
      {"1", value},
    };

    auto fields = Fields(field_array);
    db->Put(writeOptions, key, fields);
    bytes += fields.size();

    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
  bytes_ += bytes;
}

// DB::Get() PointQuery Random
void GetData(DB *db, std::vector<int64_t> &lats) {
  ReadOptions readOptions;
  bytes_ = 0;
  int64_t bytes = 0;
  srand(0);
  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;
  for (int i = 0; i < reads_; i++) {
    int key_ = rand() % num_+1;
    std::string key = std::to_string(key_);
    Fields ret;
    db->Get(readOptions, key, &ret);
    bytes += ret.size();
    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
  bytes_ += bytes;
}

// DB::Iterator()->Seek() PointQuery 
void PointQuery(DB *db, std::vector<int64_t> &lats) {
  ReadOptions options;
  srand(0);
  bytes_ = 0;
  int64_t bytes = 0;
  Iterator* iter = db->NewIterator(options);
  int key_ = 0;

  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;
  for (int i = 0; i < reads_; i++) {
    key_ = (key_ + rand()) % num_+1;
    std::string key = std::to_string(key_);
    iter->Seek(key);
    bytes += iter->fields().size();
    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
  bytes_+=bytes;
  delete iter;
}

// DB::Iterator()->SeekToFirst() RangeQuery
void ReadOrdered(DB *db, std::vector<int64_t> &lats) {
  Iterator* iter = db->NewIterator(ReadOptions());
  int i = 0;
  bytes_ = 0;
  int64_t bytes = 0;
  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;
  for (iter->SeekToFirst(); i < reads_ && iter->Valid(); iter->Next()) {
    ++i;
    bytes+=iter->fields().size();
    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
  bytes_+=bytes;
  delete iter;
}

// DB::FindKeysByField()
void SearchField(DB *db, std::vector<int64_t> &lats) {
  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;
  srand(0);
  for (int i = 0; i < search_; i++) {
    // Iterator *iter = db->NewIterator(ReadOptions());
    int value_ = std::rand() % (num_ + 1);
    Field field_to_search = {"1", std::to_string(value_)};
    const std::vector<std::string> key_ret = db->FindKeysByField(field_to_search); 
    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
}

// Insert many k/vs in order to start background GC
void InsertMany(DB *db) {
  std::vector<int64_t> lats;
  for (int i = 0; i < 18; i++) {
    InsertData(db, lats);
    
    GetData(db, lats);
    db->CompactRange(nullptr, nullptr);
    std::cout << "put and get " << i << " of Many" << std::endl;
  }
}

void InsertToGC(DB *db, std::vector<int64_t> &lats) {
  WriteOptions writeOptions;
  bytes_ = 0;
  int64_t bytes = 0;
  srand(0);
  std::mt19937 value_seed(100);
  std::uniform_int_distribution<int> value_range(10, 2048);
  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;

  const int num_operations = 100000; // 设置足够大的插入次数来触发合并

  for (int i = 0; i < num_operations; i++) {
    int key_ = rand() % num_ + 1;
    int value_ = std::rand() % (num_ + 1);
    int value_size = value_range(value_seed);
    std::string value(value_size, 'a');
    std::string key = std::to_string(key_);
    FieldArray field_array = {{"1", value}};
    auto fields = Fields(field_array);
    
    // 插入数据
    db->Put(writeOptions, key, fields);
    bytes += fields.size();

    // 延迟记录
    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);

    // 你也可以控制内存中VTable的大小，让其更快触发GC
    if (i % 10000 == 0) {
      // 手动触发一次Compaction，强制系统开始合并操作
      db->CompactRange(nullptr, nullptr);
    }
  }

  bytes_ += bytes;
  std::cout << "Finished inserting " << num_operations << " operations." << std::endl;
}

double CalculatePercentile(const std::vector<int64_t>& latencies, double percentile) {
  if (latencies.empty()) return 0.0;

  std::vector<int64_t> sorted_latencies = latencies;
  std::sort(sorted_latencies.begin(), sorted_latencies.end());

  size_t index = static_cast<size_t>(percentile * sorted_latencies.size());
  if (index >= sorted_latencies.size()) index = sorted_latencies.size() - 1;

  return sorted_latencies[index];
}

TEST(TestBench, Throughput) {
  DB *db;
  if(OpenDB("testdb", &db).ok() == false) {
    std::cerr << "open db failed" << std::endl;
    abort();
  }

  std::vector<int64_t> lats;

  // Put()
  auto start_time = std::chrono::steady_clock::now();
  InsertData(db, lats);
  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  std::cout << "Throughput of Put(): " << std::fixed << num_ * 1e6 / duration << " ops/s" << std::endl;
  std::cout << "Throughput of Put(): " << std::setprecision(3) << (bytes_ / 1048576.0) / (duration * 1e-6) << " MB/s" << std::endl << std::endl;
  
  // Get()
  start_time = std::chrono::steady_clock::now();
  GetData(db, lats);
  end_time = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  std::cout << "Throughput of Get(): " << std::fixed << reads_ * 1e6 / duration << " ops/s" << std::endl;
  std::cout << "Throughput of Get(): " << std::setprecision(3) << (bytes_ / 1048576.0) / (duration * 1e-6) << " MB/s" << std::endl << std::endl;
  // Iterator()
  start_time = std::chrono::steady_clock::now();
  ReadOrdered(db, lats);
  end_time = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  std::cout << "Throughput of Iterator(): " << std::fixed << reads_ * 1e6 / duration << " ops/s" << std::endl;
  std::cout << "Throughput of Iterator(): " << std::setprecision(3) << (bytes_ / 1048576.0) / (duration * 1e-6) << " MB/s" << std::endl << std::endl;
  // FindKeysbyField()
  start_time = std::chrono::steady_clock::now();
  SearchField(db, lats);
  end_time = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  std::cout << "Throughput of FindKeysbyField(): " << std::setprecision(3) << search_ * 1e6 / duration << " ops/s" << std::endl << std::endl;
  
  delete db;
}

TEST(TestBench, Latency) {
  DB *db;
  if(OpenDB("testdb", &db).ok() == false) {
    std::cerr << "open db failed" << std::endl;
    abort();
  }

  std::vector<int64_t> put_lats;
  std::vector<int64_t> get_lats;
  std::vector<int64_t> iter_lats;
  std::vector<int64_t> search_lats;
  auto calc_lat = [](const std::vector<int64_t>& latencies) {
    double avg = 0.0;
    for (auto latency : latencies) {
        avg += latency;
    }
    avg /= latencies.size();

    double p75 = CalculatePercentile(latencies, 0.75);
    double p99 = CalculatePercentile(latencies, 0.99);

    return std::make_tuple(avg, p75, p99);
  };

  InsertData(db, put_lats);
  std::tuple<double, double, double> put_latency = calc_lat(put_lats);
  double put_avg = std::get<0>(put_latency);
  double put_p75 = std::get<1>(put_latency);
  double put_p99 = std::get<2>(put_latency);
  std::cout << "Put Latency (avg, P75, P99): " << std::endl << std::setprecision(3) << put_avg * 1e-3 << " micros/op, " << put_p75 * 1e-3 << " micros/op, " << put_p99 * 1e-3 << " micros/op" << std::endl << std::endl;

  GetData(db, get_lats);
  std::tuple<double, double, double> get_latency = calc_lat(get_lats);
  double get_avg = std::get<0>(get_latency);
  double get_p75 = std::get<1>(get_latency);
  double get_p99 = std::get<2>(get_latency);
  std::cout << "Get Latency (avg, P75, P99): " << std::endl << std::setprecision(3) << get_avg * 1e-3 << " micros/op, " << get_p75 * 1e-3 << " micros/op, " << get_p99 * 1e-3 << " micros/op" << std::endl << std::endl; 

  ReadOrdered(db, iter_lats);
  std::tuple<double, double, double> iter_latency = calc_lat(iter_lats);
  double iter_avg = std::get<0>(iter_latency);
  double iter_p75 = std::get<1>(iter_latency);
  double iter_p99 = std::get<2>(iter_latency);
  std::cout << "Iterator Latency (avg, P75, P99): " << std::endl << std::setprecision(3) << iter_avg * 1e-3 << " micros/op, " << iter_p75 * 1e-3 << " micros/op, " << iter_p99 * 1e-3 << " micros/op" << std::endl << std::endl;

  SearchField(db, search_lats);
  std::tuple<double, double, double> search_latency = calc_lat(search_lats);
  double search_avg = std::get<0>(search_latency);
  double search_p75 = std::get<1>(search_latency);
  double search_p99 = std::get<2>(search_latency);
  std::cout << "FindKeysByField Latency (avg, P75, P99): " << std::endl << std::setprecision(3) << search_avg * 1e-3 << " micros/op, " << search_p75 * 1e-3 << " micros/op, " << search_p99 * 1e-3 << " micros/op" << std::endl << std::endl; 
  
  delete db;

}

// TEST(TestBench, GC) {
//   DB *db;
//   if(OpenDB("testdb", &db).ok() == false) {
//     std::cerr << "open db failed" << std::endl;
//     abort();
//   }
//   std::vector<int64_t> lats;
//   std::vector<int64_t> put_lats;
//   std::vector<int64_t> get_lats;
//   std::vector<int64_t> iter_lats;
//   std::vector<int64_t> search_lats;

  
//   InsertMany(db);
//   // std::cout << "put and get 1" << std::endl;
//   // InsertData(db, lats);
//   // GetData(db, lats);
//   // std::cout << "put and get 2" << std::endl;
//   // InsertData(db, lats);
//   // GetData(db, lats);
//   // std::cout << "put and get 3" << std::endl;
//   // InsertData(db, lats);
//   // GetData(db, lats);
//   // std::cout << "put and get 4" << std::endl;
//   // InsertData(db, lats);
//   // GetData(db, lats);

//   InsertData(db, put_lats);
// }

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
