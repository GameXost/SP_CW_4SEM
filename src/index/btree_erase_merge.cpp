#include "index/btree.h"
#include <stdexcept>

bool BTree::erase(const Value& key) {
    if (_store.getRoot() == NULL_PAGE) return false;

    uint32_t leaf_id = findLeaf(key);
    if (leaf_id == NULL_PAGE) return false;
    if (!eraseFromLeaf(leaf_id, key)) return false;
    
    BTreeNode leaf = _store.load(leaf_id);
    
    if (leaf_id == _store.getRoot()) {
        return true;
    }
    if (leaf.key_count < BTREE_MIN_KEYS) fixUnderflow(leaf_id);
    return true;
}

bool BTree::eraseFromLeaf(uint32_t leaf_id, const Value& key) {
    BTreeNode leaf = _store.load(leaf_id);
    for (uint16_t i = 0; i < leaf.key_count; ++i) {
        if (compareValues(key, leaf.keys[i]) == 0) {
            leaf.keys.erase(leaf.keys.begin() + i);
            leaf.values.erase(leaf.values.begin() + i);
            leaf.key_count--;
            _store.save(leaf_id, leaf);
            return true;
        }
    }
    return false;
}

void BTree::fixUnderflow(uint32_t node_id) {
    BTreeNode node = _store.load(node_id);
    
    if (node.parent_id == NULL_PAGE) {
        if (!node.is_leaf && node.key_count == 0) {
            uint32_t new_root_id = node.children[0];
            BTreeNode new_root = _store.load(new_root_id);
            new_root.parent_id = NULL_PAGE;

            _store.save(new_root_id, new_root);
            _store.setRoot(new_root_id);
            _store.free(node_id);
        }
        return;
    }

    BTreeNode parent = _store.load(node.parent_id);
    int child_idx = findChildIndex(parent, node_id);
    uint16_t cidx = static_cast<uint16_t>(child_idx);
    
    // сначала пробуем занять ключ у сиблингов
    if (child_idx > 0 && tryBorrowLeft(node_id, node.parent_id, cidx)) return;
    if (child_idx < (int)parent.key_count && tryBorrowRight(node_id, node.parent_id, cidx)) return;
    
    // если занять не вышло мержим
    if (child_idx > 0) {
        mergeWithLeft(node_id, node.parent_id, cidx);
    } else {
        mergeWithRight(node_id, node.parent_id, cidx);
    }
}

bool BTree::tryBorrowLeft(uint32_t node_id, uint32_t parent_id, uint16_t child_idx) {
    BTreeNode parent = _store.load(parent_id);
    uint32_t left_id = parent.children[child_idx - 1];
    BTreeNode left = _store.load(left_id);
    
    if (left.key_count <= BTREE_MIN_KEYS) return false;
    BTreeNode node = _store.load(node_id);
    if (node.is_leaf) {
        node.keys.insert(node.keys.begin(), left.keys.back());
        node.values.insert(node.values.begin(), left.values.back());
        node.key_count++;

        left.keys.pop_back();
        left.values.pop_back();
        left.key_count--;
        parent.keys[child_idx - 1] = node.keys.front();
    } else {
        uint32_t borrowed_child = left.children.back();
        node.keys.insert(node.keys.begin(), parent.keys[child_idx - 1]);
        node.children.insert(node.children.begin(), borrowed_child);
        node.key_count++;

        BTreeNode moved = _store.load(borrowed_child);
        moved.parent_id = node_id;
        _store.save(borrowed_child, moved);
        parent.keys[child_idx - 1] = left.keys.back();
        left.keys.pop_back();
        left.children.pop_back();
        left.key_count--;
    }
    _store.save(left_id, left);
    _store.save(node_id, node);
    _store.save(parent_id, parent);
    return true;
}

bool BTree::tryBorrowRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx) {
    BTreeNode parent = _store.load(parent_id);
    uint32_t right_id = parent.children[child_idx + 1];
    BTreeNode right = _store.load(right_id);

    if (right.key_count <= BTREE_MIN_KEYS) return false;
    BTreeNode node = _store.load(node_id);
    if (node.is_leaf) {
        node.keys.push_back(right.keys.front());
        node.values.push_back(right.values.front());
        node.key_count++;

        right.keys.erase(right.keys.begin());
        right.values.erase(right.values.begin());
        right.key_count--;
        parent.keys[child_idx] = right.keys.front();
    } else {
        uint32_t borrowed_child = right.children.front();
        node.keys.push_back(parent.keys[child_idx]);
        node.children.push_back(borrowed_child);
        node.key_count++;

        BTreeNode moved = _store.load(borrowed_child);
        moved.parent_id = node_id;
        _store.save(borrowed_child, moved);
        parent.keys[child_idx] = right.keys.front();
        right.keys.erase(right.keys.begin());
        right.children.erase(right.children.begin());
        right.key_count--;
    }
    _store.save(right_id, right);
    _store.save(node_id, node);
    _store.save(parent_id, parent);
    return true;
}

void BTree::mergeWithLeft(uint32_t node_id, uint32_t parent_id, uint16_t child_idx) {
    BTreeNode parent = _store.load(parent_id);
    uint32_t left_id = parent.children[child_idx - 1];
    BTreeNode left = _store.load(left_id);
    BTreeNode node = _store.load(node_id);

    if (node.is_leaf) {
        for (uint16_t i = 0; i < node.key_count; ++i) {
            left.keys.push_back(node.keys[i]);
            left.values.push_back(node.values[i]);
        }
        left.key_count += node.key_count;
        left.next_leaf = node.next_leaf;
    } else {
        left.keys.push_back(parent.keys[child_idx - 1]);
        left.key_count++;

        for (uint16_t i = 0; i < node.key_count; ++i) {
            left.keys.push_back(node.keys[i]);
        }
        for (uint16_t i = 0; i <= node.key_count; ++i) {
            left.children.push_back(node.children[i]);
            BTreeNode ch = _store.load(node.children[i]);
            ch.parent_id = left_id;
            _store.save(node.children[i], ch);
        }
        left.key_count += node.key_count;
    }
    parent.keys.erase(parent.keys.begin() + (child_idx - 1));
    parent.children.erase(parent.children.begin() + child_idx);
    parent.key_count--;

    _store.save(left_id, left);
    _store.save(parent_id, parent);
    _store.free(node_id);
    
    if (parent.parent_id == NULL_PAGE) {
        if (!parent.is_leaf && parent.key_count == 0) {
            BTreeNode new_root = _store.load(left_id);
            new_root.parent_id = NULL_PAGE;
            _store.save(left_id, new_root);
            _store.setRoot(left_id);
            _store.free(parent_id);
        }
        return;
    }
    if (parent.key_count < BTREE_MIN_KEYS) fixUnderflow(parent_id);
}

void BTree::mergeWithRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx) {
    BTreeNode parent = _store.load(parent_id);
    uint32_t right_id = parent.children[child_idx + 1];
    BTreeNode right = _store.load(right_id);
    BTreeNode node = _store.load(node_id);

    if (node.is_leaf) {
        for (uint16_t i = 0; i < right.key_count; ++i) {
            node.keys.push_back(right.keys[i]);
            node.values.push_back(right.values[i]);
        }
        node.key_count += right.key_count;
        node.next_leaf = right.next_leaf;
    } else {
        node.keys.push_back(parent.keys[child_idx]);
        node.key_count++;
        for (uint16_t i = 0; i < right.key_count; ++i) {
            node.keys.push_back(right.keys[i]);
        }
        for (uint16_t i = 0; i <= right.key_count; ++i) {
            node.children.push_back(right.children[i]);
            BTreeNode ch = _store.load(right.children[i]);
            ch.parent_id = node_id;
            _store.save(right.children[i], ch);
        }
        node.key_count += right.key_count;
    }
    parent.keys.erase(parent.keys.begin() + child_idx);
    parent.children.erase(parent.children.begin() + child_idx + 1);
    parent.key_count--;

    _store.save(node_id, node);
    _store.save(parent_id, parent);
    _store.free(right_id);

    if (parent.parent_id == NULL_PAGE) {
        if (!parent.is_leaf && parent.key_count == 0) {
            BTreeNode new_root = _store.load(node_id);
            new_root.parent_id = NULL_PAGE;
            _store.save(node_id, new_root);
            _store.setRoot(node_id);
            _store.free(parent_id);
        }
        return;
    }
    if (parent.key_count < BTREE_MIN_KEYS) fixUnderflow(parent_id);
}