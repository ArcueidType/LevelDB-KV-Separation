## 1. 项目概述

### 项目背景

本项目基于LevelDB源代码进行修改和优化。`LevelDB`使用`LSM Tree`的数据结构，是`key-value`数据库的典型。LSM树后台为了加速查询，将键值对从磁盘里读取、排序再写入，会带来50倍以上的写放大。这种对HDD有利的优化方式不适用于当下使用较多的SSD，因此我们需要对其功能进行修改和优化。

### 实现功能

本项目要实现的内容及目的是：

+ **字段设计**：模仿关系型数据库，扩展`value`的结构，在`value`中多个字段，并可以通过这些字段进行查询对应的`key`，实现类似关系数据库中按列查询的功能

+ **KV分离**：分离存储LevelDB的`key`和`value`，LSM树中的value为一个指向`Value Log`文件的指针，用户的真实`value`存储在`Value Log`中，减轻LSM树的存储负载，大幅度减小了读写放大的性能影响

## 2. 功能设计

#### 2.1. 字段设计

- 设计目标

  - 将 LevelDB 中的 `value` 组织成字段数组，每个数组元素对应一个字段（字段名：字段值）。

  - 不改变LevelDB原有的存储方式，通过对字段数组的序列化和对字符串解析得到字段数组来实现字段功能

  - 在作为key-value对存储进LevelDB之前可以任意调整字段

  - 可以通过字段查询对应的key

- 实现思路

  - 将字段数组设计为一个class `Fields` ，其拥有对于字段进行操作的各种方法，方便对字段的修改，该类也作为 `Get` 方法和 `Put` 方法返回和插入的对象
  - 根据key查询时如果指定字段名则返回对应的字段，如果未指定字段名则返回所有字段
  - Put仅有value时(原本的Put方法)自动给value增加递增的字段
  - 为Iterator类添加新的方法 `fields` ，用于范围查询时获得查询结果
  - 对于给定的字段，遍历LevelDB得到其对应的所有keys，作为 `DB` 类的新方法 `FindKeysByField`

#### 2.2. KV分离

- 设计目标
  - 拓展key-value存储结构，分离key和value的存储，从而对写放大进行优化
  - 不影响LevelDB原本接口的正常功能
  - 分离存储Value的Value-Log需要有对应的GC(Garbage Collection)功能，在达到一定条件(如Value -Log超过大小阈值)时触发，回收Value-Log中已经失效的Value
  - 确保操作的原子性
- 设计思路
  - 存入LSM-tree中的kv对为 `<key, value-addr>` ，value-addr保存了 `<vLog-offset, value-size>` ，对应value在vLog开始位置的便宜和value的大小
  - vLog的写入可以仿照LevelDB的Memtable机制，以防止频繁的写磁盘
  - vLog采用Append-Only，新数据仅在head添加，在回收时从tail处取得一块chunk，将其中有效的数据Append到head处，同时保证tail被永久保存到LSM-tree中，最后将整块chunk释放
  - 为了优化GC的效率，vLog同时存储key和value，而不是仅存入value
  - **`(可能的优化)`** 因为vLog同时存入了key和value，因此LSM-tree不再需要log file，因为在恢复时可以通过vLog进行恢复，但对整个vLog扫描是不可取的，因此可以定期在LSM-tree中更新head(vLog的末尾)，在恢复时从该head恢复到当前vLog的最末端

## 3. 数据结构设计

#### 3.1. 字段功能

- Field, FieldArray

  - ```c++
    using Field = std::pair<std::string, std::string>;
    using FieldArray = std::vector<std::pair<std::string, std::string>>;
    ```

- Fields

  - 封装有 `std::map<std::string, std::string>` 的class，其接口在 **[4](#4. 接口/函数设计)**中详细描述，用map实现在字段较多时可以获得较高的查询效率
  
- FieldString

  - ```
    FieldString遵从以下格式：
    <field_size1, field1, field_size2, field2, ...>
    field遵从以下格式:
    <name_size, name, value>
    ```


#### 3.2. KV分离

- Key-Value分离结构图示：

![kv-sep](./assets/kvsep_overview.png)

## 4. 接口/函数设计

#### 4.1. 字段功能

- Fields

  - ```c++
    class Fields {
        private:
        	std::map<std::string, std::string> _fields;
        public:
        	// 从FieldArray构建Fields
        	Fields(FieldArray field_array);
        
        	// 从LevelDB存储的Value中解码出Fields
        	Fields(std::string fields_str);
        	~Fields();
        	
        	// 重载[]运算符简便对字段的修改和访问操作
        	std::string& Fields::operator[](const std::string& field_name);
                
            // 获取当前Fields对应的FieldArray
            FieldArray GetFieldArray();
        
        	// 将Fields编码为存入LevelDB的Value
        	std::string Serialize();
    }
    ```
- FindKeysByField

  - ```c++
    // 根据字段值查找所有包含该字段的 key
    std::vector<std::string> FindKeysByField(leveldb::DB* db, Field &field) {
        ...
    }
    ```

#### 4.2. KV分离

kv分离文件VTable的格式定义(vtable format)

- ```c++
  // VTable最基本的存储单位，表示存储的一个key和一个value
  struct VTableRecord {
    Slice key;
    Slice value;
    
    // 将record编码为str
    void Encode(std::string* target) const;
    // 将Slice解码为record
    Status Decode(Slice* input);
    
    // 该record的size
    size_t size() const { return key.size() + value.size(); }
    
    friend bool operator==(const VTableRecord& a, const VTableRecord& b) {
      return a.key == b.key && a.value == b.value;
    }
  };
  
  class RecordEncoder {
  public:
    RecordEncoder() = default;
  
    // 编码一条vTable record
    void Encode(const VTableRecord& record);

    // 获得编码后的records的size
    size_t GetEncodedSize() const { return sizeof(header_) + record_.size(); }

    // 获取编码后的header
    Slice GetHeader() const { return {header_, sizeof(header_)}; }

    // 获得编码后的record
    Slice GetRecord() const { return record_; }
  private:
    char header_[kRecordHeaderSize];
    Slice record_;
  
    std::string record_buff_;
  };
  
  class RecordDecoder {
  public:
  
    // 解码出record的header
    Status DecodeHeader(Slice* input);

    // 解码出record
    Status DecodeRecord(Slice* input, VTableRecord* record) const;

    // 获得解码后的record size
    size_t GetDecodedSize() const { return record_size_; }
  
  private:
    uint32_t record_size_{0};
  };
  
  struct VTableHandle {
    // 表示某个record在VTable中的位置
    uint64_t offset{0};
    uint64_t size{0};
    
    void Encode(std::string* target) const;
    Status Decode(Slice* input);
    
    friend bool operator==(const VTableHandle& a, const VTableHandle& b) {
        return a.offset == b.offset && a.size == b.size;
    }
  };
  
  struct VTableIndex {
    // 存入sstable中的index
    enum Type : unsigned char {
    kVTableIndex = 1,
    };
    
    uint64_t file_number{0};
    VTableHandle vtable_handle;
    
    void Encode(std::string* target) const;
    Status Decode(Slice* input);
    
    friend bool operator==(const VTableIndex& a, const VTableIndex& b) {
    return a.file_number == b.file_number && a.vtable_handle == b.vtable_handle;
    }
  };
  ```
  
VTable构建类 (vtable builder)

- ```c++
  class VTableBuilder {
  public:
    VTableBuilder(const Options& options, WritableFile* file);
  
    // Add a record to the vTable
    void Add(const VTableRecord& record, VTableHandle* handle);

    // Builder status, return non-ok iff some error occurs
    Status status() const { return status_; }

    // Finish building the vTable
    Status Finish();

    // Abandon building the vTable
    void Abandon();

    uint64_t FileSize() const { return file_size_; }

    uint64_t RecordNumber() const { return record_number_; }
  private:
    bool ok() const { return status().ok(); }
  
    WritableFile* file_;
    uint64_t file_size_{0};
    uint64_t record_number_{0};

    Status status_;

    RecordEncoder encoder_;
  };
  ```
  
VTable读取类(vtable reader)

- ```c++
  class VTableReader {
  public:
    VTableReader() = default;
  
    VTableReader(uint64_t fnum, VTableManager *manager) :
      fnum_(fnum),
      manager_(manager) {};

    Status Open(const Options& options, std::string fname);

    Status Get(const VTableHandle& handle,
               VTableRecord* record) const ;

    void Close();
  private:
    Options options_;
    uint64_t fnum_;
    RandomAccessFile* file_{nullptr};
    VTableManager* manager_{nullptr};
    };
  ```
  
VTable管理类(vtable manager)
- ```c++
  struct VTableMeta {
    uint64_t number;
    
    uint64_t records_num;
    
    uint64_t invalid_num;
    
    uint64_t table_size;
    
    uint64_t ref = 0;
    
    void Encode(std::string* target) const;
    Status Decode(Slice* input);
    
    VTableMeta() : number(0), records_num(0), invalid_num(0), table_size(0) {}
  };
  
  class VTableManager {
  public:
    explicit VTableManager(const std::string& dbname, Env* env, size_t gc_threshold) :
    dbname_(dbname),
    env_(env),
    gc_threshold_(gc_threshold) {}
  
    ~VTableManager() = default;
  
    // sign a vtable to meta
    void AddVTable(const VTableMeta& vtable_meta);

    // remove a vtable from meta
    void RemoveVTable(uint64_t file_num);

    // add an invalid num to a vtable
    Status AddInvalid(uint64_t file_num);

    // save meta info to disk
    Status SaveVTableMeta() const;

    // recover meta info from disk
    Status LoadVTableMeta();

    // reference a vtable
    void RefVTable(uint64_t file_num);

    // unref a vtable
    void UnrefVTable(uint64_t file_num);

    // maybe schedule backgroud gc
    void MaybeScheduleGarbageCollect();

    // do backgroud gc work
    static void BackgroudGC(void* gc_info);
  
  private:
    std::string dbname_;
    Env* env_;
    std::map<uint64_t, VTableMeta> vtables_;
    std::vector<uint64_t> invalid_;
    size_t gc_threshold_;
  };
  ```

对 `DoCompactionWork`，`BuildTable`方法修改，完成kv分离

对`get`和`put`方法修改，`iterator`类修改，保证 `leveldb` 基本功能

## 5. 功能测试

### 单元测试

对于实现的代码，首先设计测试用例验证其功能的正确性。

#### 1. 字段

在这一部分中，测试用例需要考虑到能否正确存入含有多字段的`value`，并正确读取，以及是否能根据目标字段找到对应的所有`key`。

```c++
// 测试能否正确存入和读取
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
```

```c++
// 测试能否根据字段查找key
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
}
```

#### 2. KV分离

```c++
// 测试KV分离的写入与读取的正确性
TEST(TestKVSeparate, PutGetIterator){
    std::string key = "k_1";
    std::string value = "ar";
    std::value_addr;

    db->Put(WriteOptions(), key, value, &value_addr);

    // 测试能否通过存储的value_addr读取对应vlog中存储信息
    std::value value_ret;
    GetValue(&value_addr, &value_ret);
    assert(value_ret == value);

    // 测试能否直接通过key读取对应value
    db->Get(ReadOptions(), key, &value_ret);
    assert(value_ret == value);
}
```

### 性能测试 Benchmark

设计代码测试读、写、扫描、字段查询等操作的**吞吐量、延迟和写放大**情况，反映LevelDB的性能。

```c++
// 测试吞吐量
void TestThroughput(leveldb::DB* db, int num_operations) {
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        // Operations
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(
        end_time - start_time).count();
    cout << "Throughput: " << num_operations * 1000 / duration << " OPS" << endl;
}
```

```c++
// 测试延迟
void TestLatency(leveldb::DB* db, int num_operations, 
    std::vector<int64_t> &lat_res) {
    int64_t latency = 0;
    
    auto end_time = std::chrono::steady_clock::now();
    auto last_time = end_time;

    for (int i = 0; i < num_operations; ++i) {
        // Operations

        end_time = std::chrono::steady_clock::now();
        latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - last_time).count(); 
        last_time = end_time;

        lat_res.emplace_back(latency);
    }  
}
```

对于写放大，通过LevelDB的log信息计算系统的写放大。

## 6. 可能遇到的挑战与解决方案

### 字段设计可能的问题

+ 字段数量过多可能导致存储结构复杂，更改字段时性能下降
+ 字段可能包含多种类型，增加解析复杂性
+ 不同字段大小差异大，导致读写性能下降

### KV分离可能的问题

+ 数据一致性：Key和Value存储位置不同，写入或删除时需要保证一致性
+ 系统崩溃、磁盘损坏等可能导致分离存储的Value丢失
+ 分离存储的Value文件的大小优先，如何合适地存储超大Value
+ 分离存储导致读写时需要经过索引，读写放大产生性能影响


## 7. 分工和进度安排

|        功能         |  完成日期  |   分工   |
|:-----------------:|:------:|:------:|
|  Fields类和相关接口实现   | 12月1日  |  韩晨旭   |
| 修改LevelDB接口实现字段功能 | 12月1日  |  韩晨旭   |
|  vTable format实现  | 12月19日 |  韩晨旭   |
| vTable builder实现  | 12月26日 |  韩晨旭   |
|  vTable reader实现  | 12月26日 |  韩晨旭   |
| vTable manager实现  | 12月31日 |  韩晨旭   |
|      接口方法修改       | 12月31日 |  韩晨旭   |
|      功能正确性测试      | 12月31日 |  韩晨旭   |
|       吞吐量测试       |  1月4日  |   李畅   |
|       延迟测试        |  1月4日  |   李畅   |
|      写放大测试对比      |  1月4日  |   李畅   |
|    尝试对系统性能进行优化    |  1月4日  | 韩晨旭、李畅 |