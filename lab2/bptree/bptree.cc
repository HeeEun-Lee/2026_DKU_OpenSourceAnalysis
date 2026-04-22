#include "bptree.h"

#include <algorithm>
#include <limits>
#include <utility>

// B+Tree Constructor. degree 설정 등 최초 설정 진행
BPlusTree::BPlusTree(int degree) : root_(nullptr), degree_(degree) {
  if (degree_ < 3) {
    degree_ = 3;
  }
}

// B+Tree Destructor. B+Tree에 존재하는 모든 Node delete 필요
BPlusTree::~BPlusTree() {
  Destroy(root_);
  root_ = nullptr;
}

void BPlusTree::Destroy(Node* node) {
  if (node == nullptr) {
    return;
  }
  if (!node->is_leaf) {
    for (Node* child : node->children) {
      Destroy(child);
    }
  }
  delete node;
}

int BPlusTree::FirstKey(const Node* node) const {
  const Node* current = node;
  while (current != nullptr && !current->is_leaf) {
    if (current->children.empty()) {
      return 0;
    }
    current = current->children.front();
  }
  if (current == nullptr || current->keys.empty()) {
    return 0;
  }
  return current->keys.front();
}

void BPlusTree::UpdateAncestorsFirstKeys(const std::vector<Node*>& path) {
  for (size_t depth = 1; depth < path.size(); ++depth) {
    Node* parent = path[depth - 1];
    Node* child = path[depth];
    if (parent == nullptr || parent->is_leaf) {
      continue;
    }
    auto it = std::find(parent->children.begin(), parent->children.end(), child);
    if (it == parent->children.end()) {
      continue;
    }
    size_t child_index = static_cast<size_t>(it - parent->children.begin());
    if (child_index > 0 && child_index - 1 < parent->keys.size()) {
      parent->keys[child_index - 1] = FirstKey(child);
    }
  }
}

BPlusTree::Node* BPlusTree::FindLeaf(int key) const {
  Node* node = root_;
  while (node != nullptr && !node->is_leaf) {
    size_t idx =
        static_cast<size_t>(std::upper_bound(node->keys.begin(),
                                             node->keys.end(),
                                             key) -
                            node->keys.begin());
    node = node->children[idx];
  }
  return node;
}

BPlusTree::InsertResult BPlusTree::InsertRecursive(Node* node,
                                                   int key,
                                                   const std::string& value) {
  if (node->is_leaf) {
    auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    size_t idx = static_cast<size_t>(it - node->keys.begin());
    if (it != node->keys.end() && *it == key) {
      node->values[idx] = value;
      return {false, 0, nullptr};
    }

    node->keys.insert(it, key);
    node->values.insert(node->values.begin() + static_cast<long>(idx), value);

    if (static_cast<int>(node->keys.size()) <= degree_) {
      return {false, 0, nullptr};
    }

    Node* right = new Node(true);
    size_t mid = node->keys.size() / 2;
    right->keys.assign(node->keys.begin() + static_cast<long>(mid),
                       node->keys.end());
    right->values.assign(node->values.begin() + static_cast<long>(mid),
                         node->values.end());
    node->keys.resize(mid);
    node->values.resize(mid);
    right->next = node->next;
    node->next = right;
    return {true, right->keys.front(), right};
  }

  size_t child_index =
      static_cast<size_t>(std::upper_bound(node->keys.begin(),
                                           node->keys.end(),
                                           key) -
                          node->keys.begin());
  InsertResult child_result =
      InsertRecursive(node->children[child_index], key, value);
  if (!child_result.split) {
    return {false, 0, nullptr};
  }

  node->keys.insert(node->keys.begin() + static_cast<long>(child_index),
                    child_result.promoted_key);
  node->children.insert(
      node->children.begin() + static_cast<long>(child_index + 1),
      child_result.right);

  if (static_cast<int>(node->keys.size()) <= degree_) {
    return {false, 0, nullptr};
  }

  Node* right = new Node(false);
  size_t mid = node->keys.size() / 2;
  int promoted = node->keys[mid];

  right->keys.assign(node->keys.begin() + static_cast<long>(mid + 1),
                     node->keys.end());
  right->children.assign(node->children.begin() + static_cast<long>(mid + 1),
                         node->children.end());

  node->keys.resize(mid);
  node->children.resize(mid + 1);

  return {true, promoted, right};
}

// B+Tree Put operation
void BPlusTree::Put(int key, const std::string& value) {
  if (root_ == nullptr) {
    root_ = new Node(true);
    root_->keys.push_back(key);
    root_->values.push_back(value);
    return;
  }

  InsertResult result = InsertRecursive(root_, key, value);
  if (!result.split) {
    return;
  }

  Node* new_root = new Node(false);
  new_root->keys.push_back(result.promoted_key);
  new_root->children.push_back(root_);
  new_root->children.push_back(result.right);
  root_ = new_root;
}

// B+Tree Get operation
bool BPlusTree::Get(int key, std::string* value) const {
  Node* leaf = FindLeaf(key);
  if (leaf == nullptr) {
    return false;
  }
  auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
  if (it == leaf->keys.end() || *it != key) {
    return false;
  }
  if (value != nullptr) {
    *value = leaf->values[static_cast<size_t>(it - leaf->keys.begin())];
  }
  return true;
}

// B+Tree Range Scan operation
std::vector<std::pair<int, std::string>>
BPlusTree::RangeScan(int start_key, int end_key) const {
  std::vector<std::pair<int, std::string>> out;
  if (root_ == nullptr || start_key > end_key) {
    return out;
  }

  Node* leaf = FindLeaf(start_key);
  while (leaf != nullptr) {
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), start_key);
    size_t idx = static_cast<size_t>(it - leaf->keys.begin());
    for (; idx < leaf->keys.size(); ++idx) {
      int key = leaf->keys[idx];
      if (key > end_key) {
        return out;
      }
      out.emplace_back(key, leaf->values[idx]);
    }
    leaf = leaf->next;
    start_key = std::numeric_limits<int>::min();
  }

  return out;
}

// B+Tree Delete operation. In-place update로 진행 됨으로, 실제 노드 삭제가
// 진행되야함
bool BPlusTree::Delete(int key) {
  if (root_ == nullptr) {
    return false;
  }

  std::vector<Node*> path;
  Node* node = root_;
  path.push_back(node);
  while (!node->is_leaf) {
    size_t idx =
        static_cast<size_t>(std::upper_bound(node->keys.begin(),
                                             node->keys.end(),
                                             key) -
                            node->keys.begin());
    node = node->children[idx];
    path.push_back(node);
  }

  Node* leaf = node;
  auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
  if (it == leaf->keys.end() || *it != key) {
    return false;
  }

  size_t idx = static_cast<size_t>(it - leaf->keys.begin());
  leaf->keys.erase(leaf->keys.begin() + static_cast<long>(idx));
  leaf->values.erase(leaf->values.begin() + static_cast<long>(idx));

  if (leaf == root_) {
    if (leaf->keys.empty()) {
      delete root_;
      root_ = nullptr;
    }
    return true;
  }

  if (!leaf->keys.empty()) {
    // 재균형은 생략하고, 상위 분기 키만 현재 첫 키에 맞춰 유지한다.
    UpdateAncestorsFirstKeys(path);
    return true;
  }

  // Empty leaf는 트리에서 제거한다.
  Node* child_to_remove = leaf;
  size_t depth = path.size() - 1;
  while (depth > 0) {
    Node* parent = path[depth - 1];
    auto child_it = std::find(parent->children.begin(),
                              parent->children.end(),
                              child_to_remove);
    if (child_it == parent->children.end()) {
      break;
    }
    size_t child_index = static_cast<size_t>(child_it - parent->children.begin());

    if (child_to_remove->is_leaf && child_index > 0) {
      Node* predecessor = parent->children[child_index - 1];
      while (!predecessor->is_leaf) {
        predecessor = predecessor->children.back();
      }
      predecessor->next = child_to_remove->next;
    }

    parent->children.erase(child_it);
    if (!parent->keys.empty()) {
      if (child_index == 0) {
        parent->keys.erase(parent->keys.begin());
      } else {
        parent->keys.erase(parent->keys.begin() +
                           static_cast<long>(child_index - 1));
      }
    }
    delete child_to_remove;

    if (parent == root_) {
      if (root_->children.empty()) {
        delete root_;
        root_ = nullptr;
      } else if (!root_->is_leaf && root_->children.size() == 1) {
        Node* old_root = root_;
        root_ = root_->children[0];
        delete old_root;
      }
      break;
    }

    if (!parent->keys.empty()) {
      break;
    }

    child_to_remove = parent;
    --depth;
  }

  return true;
}
