#include "skiplist.h"

#include <algorithm>
#include <limits>
#include <random>

// SkipList Constructor. head node, level에 따른 초기 설정 필요
SkipList::SkipList(int max_level, float p)
    : head_(nullptr), max_level_(std::max(1, max_level)), p_(p), next_seq_(1) {
  // code
  // 가장 아래 레벨(Level 0)의 head 노드 생성
  Node* bottom = new Node{INT_MIN, 0, "", false, nullptr, nullptr};

  // 현재 레벨을 가리킬 포인터 (초기에는 bottom)
  Node* curr = bottom;

  // 위로 레벨을 쌓으면서 각 레벨의 head 노드 생성
  for (int i = 1; i < max_level_; i++) {
    // 새로운 head 생성
    // down 포인터를 이용해 바로 아래 레벨과 연결
    Node* newHead = new Node{INT_MIN, 0, "", false, nullptr, curr};

    // curr을 새로 생성한 head로 갱신 (위로 이동)
    curr = newHead;
  }

  // 최종적으로 가장 위 레벨의 head를 head_로 설정
  head_ = curr;
}

// SkipList Destructor. 생성한 노드에 대해 모두 delete
SkipList::~SkipList() {
  // code
  // 가장 위 레벨부터 시작
  Node* level = head_;

  // 레벨을 따라 아래로 내려가면서 반복
  while (level) {

    // 현재 레벨에서 순회할 포인터
    Node* curr = level;

    // 다음 레벨을 미리 저장 (삭제 후 접근 방지)
    Node* nextLevel = level->down;

    // 현재 레벨의 모든 노드를 왼쪽 → 오른쪽으로 삭제
    while (curr) {
      Node* tmp = curr; // 삭제할 노드 저장
      curr = curr->next;  // 먼저 다음 노드로 이동 (안전)
      delete tmp;  // 현재 노드 메모리 해제
    }

    // 아래 레벨로 이동
    level = nextLevel;
  }
}

// SkipList Put operation시 높이 설정 함수
int SkipList::RandomLevel() {
  // code
  // 기본 레벨은 1 (무조건 최소 한 층은 존재)
  int lvl = 1;
  // p_ 확률로 레벨을 하나씩 증가
  // rand()/RAND_MAX → 0 ~ 1 사이 랜덤값 생성
  while (((double)rand() / RAND_MAX) < p_ && lvl < max_level_) {
    lvl++;
  }
  // 최종적으로 결정된 레벨 반환
  return lvl;
}

// SkipList에 새로운 key 및 value를 삽입하는 Put 함수
void SkipList::Put(int key, const std::string& value) {
  // code
  // 삽입할 노드의 높이 결정
  int level = RandomLevel();

  // 현재 삽입에 사용할 sequence number (최신 값)
  int64_t seq = next_seq_++;

  // 각 레벨에서 삽입 위치를 저장할 경로
  std::vector<Node*> path;
  FindGreaterOrEqual(key, seq, &path);

  // 아래 레벨부터 연결하기 위한 포인터
  Node* downNode = nullptr;

  // 아래 레벨부터 위로 올라가면서 삽입
  for (int i = 0; i < level; i++) {

    // 해당 레벨의 이전 노드
    Node* prev = path[i];

    // 새 노드 생성
    Node* newNode = new Node{
        key,
        seq,
        value,
        false,          
        prev->next,     // next 연결
        downNode        // 아래 레벨 연결
    };

    // 이전 노드와 연결
    prev->next = newNode;

    // 다음 레벨 연결을 위해 저장
    downNode = newNode;
  }
}

// SkipList에서 key에 해당하는 value 찾기. 존재하면 true, 없으면 (tombstone
// 고려) false 반환. value는 out_value에 저장
bool SkipList::Get(int key, std::string* out_value) const {
  RangeEntry entry;
  // tombstone 정보를 함께 확인하기 위해 내부 조회 API를 사용한다.
  if (!GetEntry(key, &entry) || entry.tombstone) {
    return false;
  }
  *out_value = entry.value;
  return true;
}

bool SkipList::GetEntry(int key, RangeEntry* out_entry) const {
  // seq를 매우 크게 잡으면 "해당 key의 가장 앞(최신)" 위치로 감
  Node* node = FindGreaterOrEqual(key, INT64_MAX, nullptr);

  if (!node || node->key != key) {
    return false;
  }

  out_entry->key = node->key;
  out_entry->value = node->value;
  out_entry->tombstone = node->tombstone;

  return true;
}

// SkipList Delete operation. Tombstone으로 삭제 진행
bool SkipList::Delete(int key) {

  // code
  // 삭제도 Put처럼 처리 (tombstone 삽입)
  int level = RandomLevel();
  int64_t seq = next_seq_++;

  std::vector<Node*> path;
  FindGreaterOrEqual(key, seq, &path);

  Node* downNode = nullptr;

  // tombstone 노드 삽입
  for (int i = 0; i < level; i++) {
    Node* prev = path[i];

    Node* newNode = new Node{
        key,
        seq,
        "",        // value 없음
        true,      // tombstone 표시
        prev->next,
        downNode
    };

    prev->next = newNode;
    downNode = newNode;
  }

  return true;
}

// SkipList range scan operation. 해당하는 노드를 vector에 모아 반환
std::vector<std::pair<int, std::string>>
SkipList::RangeScan(int start_key, int end_key) const {
  std::vector<std::pair<int, std::string>> out;

  auto entries = RangeScanEntries(start_key, end_key);

  out.reserve(entries.size());

  for (const auto& entry : entries) {
    if (!entry.tombstone) { 
      out.emplace_back(entry.key, entry.value);
    }
  }

  return out;
}

std::vector<SkipList::RangeEntry>
SkipList::RangeScanEntries(int start_key, int end_key) const {
  std::vector<RangeEntry> out;

  Node* curr = FindGreaterOrEqual(start_key, INT64_MAX, nullptr);
  
  bool first = true;
  int last_key = 0;

  while (curr && curr->key <= end_key) {

    if (first || curr->key != last_key) {
      out.push_back({curr->key, curr->value, curr->tombstone});
      last_key = curr->key;
      first = false;
    }

    curr = curr->next;
  }

  return out;
}

bool SkipList::Less(int a_key, int64_t a_seq,
  int b_key, int64_t b_seq) {
if (a_key != b_key) return a_key < b_key;
return a_seq > b_seq; // 최신 먼저
}

SkipList::Node* SkipList::FindGreaterOrEqual(
  int key, int64_t seq, std::vector<Node*>* update) const {

    Node* curr = head_;
    Node* last = nullptr; // bottom level prev
  
    while (curr) {
      while (curr->next &&
             Less(curr->next->key, curr->next->seq, key, seq)) {
        curr = curr->next;
      }
  
      if (update) {
        update->push_back(curr);
      }
  
      last = curr; // 마지막(prev) 저장
      curr = curr->down;
    }
  
    if (update) {
      std::reverse(update->begin(), update->end());
    }
  
    return last ? last->next : nullptr;  
}