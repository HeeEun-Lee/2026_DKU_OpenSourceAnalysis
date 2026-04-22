#ifndef LAB2_SKIPLIST_H
#define LAB2_SKIPLIST_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// 필요시 내부 function, 변수 등 선언 가능

class SkipList {
public:
  struct RangeEntry { // memdb에서 range scan을 할때 필요할 수도 있는 구조체
    int key;
    std::string value;
    bool tombstone;
  };

  explicit SkipList(int max_level = 16, float p = 0.5f);
  ~SkipList();

  SkipList(const SkipList&) = delete;
  SkipList& operator=(const SkipList&) = delete;

  void Put(int key, const std::string& value);
  bool Get(int key, std::string* out_value) const;
  // key의 "최신 버전"을 tombstone 포함 형태로 조회한다.
  // - 반환값: key 존재 여부
  // - out_entry->tombstone == true 이면 삭제 마커를 의미
  // - Get()은 이 함수를 감싸서 tombstone이면 false를 반환한다.
  bool GetEntry(int key, RangeEntry* out_entry) const;
  bool Delete(int key);
  // [start_key, end_key] 범위에서 각 key의 최신 버전만 1개씩 반환한다.
  // tombstone 여부를 유지해야 MemDB 계층에서 삭제 전파를 정확히 처리할 수 있다.
  std::vector<RangeEntry> RangeScanEntries(int start_key, int end_key) const;
  std::vector<std::pair<int, std::string>>
  RangeScan(int start_key,
            int end_key) const; // skiplist 내부 range scan와 memdb range
                                // scan의 차이를 고려하여 설계
private:
  struct Node {
    int key;
    int64_t seq; // 각 key의 버전을 구분하기 위한 sequence number (클수록 최신)
    std::string value;
    bool tombstone; // 삭제 여부 (true면 해당 key는 삭제된 상태)
    Node* next;
    Node* down; // 필요시 추가 노드 포인터 선언하여 사용 가능
  };

  int RandomLevel();
  // (key, seq) 이상인 첫 번째 노드를 찾는다.
  // update 벡터에는 각 레벨에서의 이전 노드를 저장한다.
  // 삽입 시 해당 경로를 사용하여 bottom → top으로 노드를 연결한다.
  Node* FindGreaterOrEqual(int key, int64_t seq,
                           std::vector<Node*>* update) const;
  // (key, seq) 기준 비교 함수
  // key는 오름차순, 같은 key일 경우 seq는 내림차순 (최신 먼저)
  static bool Less(int a_key, int64_t a_seq, int b_key, int64_t b_seq); 

  Node* head_;
  int max_level_;
  float p_;
  int64_t next_seq_;
};

#endif // LAB2_SKIPLIST_H
