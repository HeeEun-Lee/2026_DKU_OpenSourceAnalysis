#ifndef LAB2_MEMDB_H
#define LAB2_MEMDB_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "skiplist.h"

struct MemDBOptions {
  // mutable memtable 최대 크기.
  // Put/Delete 전에 예상 엔트리 크기를 더했을 때 이 값을 넘으면
  // 기존 mutable을 immutable로 승격하고 새 mutable을 만든다.
  size_t max_memtable_bytes = 4 * 1024 * 1024;
  float skiplist_p = 0.5f;
  int skiplist_max_height = 16;
};

class InMemoryDB {
 public:
  explicit InMemoryDB(const MemDBOptions& options);

  void Put(int key, const std::string& value);
  bool Get(int key, std::string* out_value) const;
  void Delete(int key);
  std::vector<std::pair<int, std::string>> RangeScan(int start_key, int end_key) const;

  size_t ImmutableCount() const;
  size_t MutableSizeBytes() const;

 private:
  struct MemTable {
    explicit MemTable(const MemDBOptions& options);

    // key -> (value/version/tombstone)을 보관하는 skiplist.
    SkipList list;
    // 이 memtable에 누적된 엔트리의 근사 메모리 사용량.
    size_t size_bytes;
    // true면 쓰기 금지(immutable), false면 현재 쓰기 대상(mutable).
    bool immutable;
  };

  void EnsureMutableCapacity(size_t entry_bytes);
  size_t EntryBytes(int key, const std::string& value) const;

  MemDBOptions options_;
  // 현재 쓰기가 일어나는 활성 memtable.
  std::unique_ptr<MemTable> mutable_;
  // 과거 memtable들. push_back 순서가 시간순(오래된 것 -> 최신).
  // 조회 시에는 rbegin()부터 순회해 최신 것을 먼저 본다.
  std::vector<std::unique_ptr<MemTable>> immutables_;
};

#endif  // LAB2_MEMDB_H
