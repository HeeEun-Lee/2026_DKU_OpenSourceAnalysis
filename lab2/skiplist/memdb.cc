#include "memdb.h"

#include <map>
#include <unordered_set>
#include <utility>

// SkipList를 사용하여 Out-of-place update를 진행하는 InMemoryDB
// Memtable 크기를 지정하여 가득 찼을 시 Immutable Memtable로 변경

InMemoryDB::MemTable::MemTable(const MemDBOptions& options)
    : list(options.skiplist_max_height, options.skiplist_p), size_bytes(0),
      immutable(false) {}

InMemoryDB::InMemoryDB(const MemDBOptions& options)
    : options_(options), mutable_(std::make_unique<MemTable>(options_)) {}

// Put operation 구현
// sequence number 구현 필요
void InMemoryDB::Put(int key, const std::string& value) {
  // code
  size_t bytes = EntryBytes(key, value);

  // 용량 체크
  EnsureMutableCapacity(bytes);

  // SkipList에 삽입
  mutable_->list.Put(key, value);

  // 사이즈 증가
  mutable_->size_bytes += bytes;
}

// Get operation 구현
bool InMemoryDB::Get(int key, std::string* out_value) const {

  // 1) 최신 데이터가 있는 mutable부터 확인
  // tombstone이면 "삭제됨"이므로 즉시 false 반환(older 탐색 중단)
  SkipList::RangeEntry entry;
  if (mutable_->list.GetEntry(key, &entry)) {
    if (entry.tombstone) {
      return false;
    }
    *out_value = entry.value;
    return true;
  }

  // 2) immutable은 최신 -> 오래된 순서로 조회
  // 최근 테이블에서 tombstone을 만나면 더 과거 값은 가려져야 하므로 즉시 종료
  for (auto it = immutables_.rbegin(); it != immutables_.rend(); ++it) {
    if (!(*it)->list.GetEntry(key, &entry)) {
      continue;
    }
    if (entry.tombstone) {
      return false;
    }
    *out_value = entry.value;
    return true;
  }

  return false;
}

// Delete operation 구현. Tombstone 사용
void InMemoryDB::Delete(int key) {
  // code
  size_t bytes = EntryBytes(key, "");

  EnsureMutableCapacity(bytes);

  mutable_->list.Delete(key);

  mutable_->size_bytes += bytes;
}

// RangeScan operation 구현
std::vector<std::pair<int, std::string>>
InMemoryDB::RangeScan(int start_key, int end_key) const {
  std::vector<std::pair<int, std::string>> out;

  // result: 최종 결과(같은 key는 최신 버전 1개만 유지)
  // deleted: 더 최신 계층에서 tombstone이 확인된 key 집합
  //          이후 오래된 계층에서 같은 key가 나와도 무시한다.
  std::map<int, std::string> result;
  std::unordered_set<int> deleted;

  // 1️⃣ mutable
  auto vec = mutable_->list.RangeScanEntries(start_key, end_key);

  for (const auto& p : vec) {

    if (p.tombstone) {
      deleted.insert(p.key);
      result.erase(p.key);   // 🔥 중요
    } else {
      result[p.key] = p.value;
    }
  }
  
  // 2️⃣ immutable (최신 것부터 순회)
  for (auto table_it = immutables_.rbegin(); table_it != immutables_.rend();
       ++table_it) {

    auto v = (*table_it)->list.RangeScanEntries(start_key, end_key);

    for (const auto& p : v) {

      // 이미 최신 계층에서 tombstone 확인된 key는 무조건 무시
      if (deleted.count(p.key)) continue;

      if (p.tombstone) {
        deleted.insert(p.key);
        result.erase(p.key);   // 🔥 중요
      } else {
        // 최신 계층에서 아직 값이 채워지지 않은 key만 반영
        if (result.find(p.key) == result.end()) {
          result[p.key] = p.value;
        }
      }
    }
  }

  // 3️⃣ 변환
  for (const auto& kv : result) {
    out.push_back(kv);
  }

  return out;
  
}

// Memtable size 제한 확인하는 함수
void InMemoryDB::EnsureMutableCapacity(size_t entry_bytes) {
  // code
  if (mutable_->size_bytes + entry_bytes > options_.max_memtable_bytes) {

    // 기존 mutable → immutable로 이동
    mutable_->immutable = true;
    immutables_.push_back(std::move(mutable_));

    // 새 mutable 생성
    mutable_ = std::make_unique<MemTable>(options_);
  }
}

// 필요시 사용
size_t InMemoryDB::ImmutableCount() const { return immutables_.size(); }

size_t InMemoryDB::MutableSizeBytes() const { return mutable_->size_bytes; }

size_t InMemoryDB::EntryBytes(int key, const std::string& value) const {
  return sizeof(key) + value.size();
}
