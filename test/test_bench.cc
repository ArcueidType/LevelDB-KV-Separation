#include <iostream>
#include <gtest/gtest.h>
#include <chrono>
#include <vector>
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
constexpr int search_ = 50;
Status OpenDB(std::string dbName, DB **db) {
  Options options;
  options.create_if_missing = true;
  return DB::Open(options, dbName, db);
}

// DB::Put()
void InsertData(DB *db, std::vector<int64_t> &lats) {
  WriteOptions writeOptions;
  srand(0);
  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;

  for (int i = 0; i < num_; ++i) {
    int key_ = rand() % num_+1;
    int value_ = std::rand() % (num_ + 1);
    std::string key = std::to_string(key_);
    FieldArray field_array = {
      {"1", std::to_string(value_)},
    };

    auto fields = Fields(field_array);
    db->Put(writeOptions, key, fields);

    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
}

// DB::Get()
void GetData(DB *db, std::vector<int64_t> &lats) {
  ReadOptions readOptions;
  srand(0);
  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;
  for (int i = 0; i < reads_; ++i) {
    int key_ = rand() % num_+1;
    std::string key = std::to_string(key_);
    Fields ret;
    db->Get(readOptions, key, &ret);
    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
  
}

// DB::Iterator()
void ReadOrdered(DB *db, std::vector<int64_t> &lats) {
  Iterator* iter = db->NewIterator(ReadOptions());
  int i = 0;

  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;
  for (iter->SeekToFirst(); i < reads_ && iter->Valid(); iter->Next()) {
    ++i;
    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
  delete iter;
}

// DB::FindKeysByField()
void SearchField(DB *db, std::vector<int64_t> &lats) {
  int64_t latency = 0;
  auto end_time = std::chrono::steady_clock::now();
  auto last_time = end_time;
  srand(0);
  for (int i = 0; i < search_; ++i) {
    int value_ = std::rand() % (num_ + 1);
    Field field_to_search = {"1", std::to_string(value_)};
    const std::vector<std::string> key_ret = db->FindKeysByField(field_to_search);
    end_time = std::chrono::steady_clock::now();
    latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - last_time).count();
    last_time = end_time;
    lats.emplace_back(latency);
  }
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
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  std::cout << "Throughput of Put(): " << num_ / duration << " ops/ms" << std::endl;
  // Get()
  start_time = std::chrono::steady_clock::now();
  GetData(db, lats);
  end_time = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  std::cout << "Throughput of Get(): " << reads_ / duration << " ops/ms" << std::endl;
  // Iterator()
  start_time = std::chrono::steady_clock::now();
  ReadOrdered(db, lats);
  end_time = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  std::cout << "Throughput of Iterator(): " << reads_ / duration << " ops/ms" << std::endl;
  // FindKeysbyField()
  start_time = std::chrono::steady_clock::now();
  SearchField(db, lats);
  end_time = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  std::cout << "Throughput of FindKeysbyField(): " << search_ / duration << " ops/ms" << std::endl;
  
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
  auto [put_avg, put_p75, put_p99] = calc_lat(put_lats);
  std::cout << "Put Latency (avg, P75, P99): " << put_avg << " micros/op, " << put_p75 << " micros/op, " << put_p99 << " micros/op" << std::endl;

  GetData(db, get_lats);
  auto [get_avg, get_p75, get_p99] = calc_lat(get_lats);
  std::cout << "Get Latency (avg, P75, P99): " << get_avg << " micros/op, " << get_p75 << " micros/op, " << get_p99 << " micros/op" << std::endl;

  ReadOrdered(db, iter_lats);
  auto [iter_avg, iter_p75, iter_p99] = calc_lat(iter_lats);
  std::cout << "Iterator Latency (avg, P75, P99): " << iter_avg << " micros/op, " << iter_p75 << " micros/op, " << iter_p99 << " micros/op" << std::endl;

  SearchField(db, search_lats);
  auto [search_avg, search_p75, search_p99] = calc_lat(search_lats);
  std::cout << "FindKeysByField Latency (avg, P75, P99): " << search_avg << " micros/op, " << search_p75 << " micros/op, " << search_p99 << " micros/op" << std::endl; 
  
  delete db;

}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
