#include "index/btree.h"
#include <stdexcept>

// вставляет (key, rid) на позицию pos в лист
static void leafInsertAt(BTreeNode& node, uint16_t pos, const Value& key, RowId rid) {
    node.keys.insert(node.keys.begin() + pos, key);
    node.values.insert(node.values.begin() + pos, rid);
    ++node.key_count;
}

// вставляет (key, right_child) на позицию pos в internal-узел
static void internalInsertAt(BTreeNode& node, uint16_t pos, const Value& key, uint32_t right_id) {
    node.keys.insert(node.keys.begin() + pos, key);
    node.children.insert(node.children.begin() + pos + 1, right_id);
    ++node.key_count;
}

// lower_bound по отсортированному keys[0..count)
static uint16_t lowerBound(const std::vector<Value>& keys, uint16_t count, const Value& key) {
    uint16_t lo = 0, hi = count;
    while (lo < hi) {
        uint16_t mid = (lo + hi) / 2;
        if (BTree::compareValues(keys[mid], key) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// проверяет уникальность, находит лист и вставляет
void BTree::insert(const Value& key, RowId rid) {
    if (find(key).has_value())
        throw std::runtime_error("BTree::insert: duplicate key (UNIQUE index violation)");

    uint32_t leaf_id = findLeaf(key);
    BTreeNode leaf = _store.load(leaf_id);
    leafInsertAt(leaf, lowerBound(leaf.keys, leaf.key_count, key), key, rid);
    _store.save(leaf_id, leaf);

    if (leaf.key_count > BTREE_MAX_KEYS)
        redistributeOrSplitLeaf(leaf_id);
}

void BTree::redistributeOrSplitLeaf(uint32_t leaf_id) {
    BTreeNode leaf = _store.load(leaf_id);

    // корень-лист: split 1->2, создать internal-корень
    if (leaf.parent_id == NULL_PAGE) {
        uint32_t new_id = _store.alloc();
        BTreeNode new_leaf;
        new_leaf.is_leaf = true;

        uint16_t left_n = leaf.key_count / 2;
        uint16_t right_n = leaf.key_count - left_n;

        new_leaf.keys.assign(leaf.keys.begin() + left_n, leaf.keys.end());
        new_leaf.values.assign(leaf.values.begin() + left_n, leaf.values.end());
        new_leaf.key_count = right_n;

        leaf.keys.resize(left_n);
        leaf.values.resize(left_n);
        leaf.key_count = left_n;

        new_leaf.next_leaf = leaf.next_leaf;
        leaf.next_leaf = new_id;

        uint32_t root_id = _store.alloc();
        BTreeNode root;
        root.is_leaf = false;
        root.key_count = 1;
        root.keys = {new_leaf.keys[0]};
        root.children = {leaf_id, new_id};

        leaf.parent_id = root_id;
        new_leaf.parent_id = root_id;

        _store.save(leaf_id, leaf);
        _store.save(new_id, new_leaf);
        _store.save(root_id, root);
        _store.setRoot(root_id);
        return;
    }

    BTreeNode parent = _store.load(leaf.parent_id);
    int idx = findChildIndex(parent, leaf_id);

    uint32_t left_sib_id = (idx > 0) ? parent.children[idx - 1] : NULL_PAGE;
    uint32_t right_sib_id = (idx < (int)parent.key_count) ? parent.children[idx + 1] : NULL_PAGE;

    // перераспределение с левым: самый левый ключ leaf уходит в конец left_sib
    if (left_sib_id != NULL_PAGE) {
        BTreeNode left_sib = _store.load(left_sib_id);
        if (left_sib.key_count < BTREE_MAX_KEYS) {
            left_sib.keys.push_back(leaf.keys.front());
            left_sib.values.push_back(leaf.values.front());
            ++left_sib.key_count;

            leaf.keys.erase(leaf.keys.begin());
            leaf.values.erase(leaf.values.begin());
            --leaf.key_count;

            parent.keys[idx - 1] = leaf.keys.front();

            _store.save(left_sib_id, left_sib);
            _store.save(leaf_id, leaf);
            _store.save(leaf.parent_id, parent);
            return;
        }
    }

    // перераспределение с правым: самый правый ключ leaf уходит в начало right_sib
    if (right_sib_id != NULL_PAGE) {
        BTreeNode right_sib = _store.load(right_sib_id);
        if (right_sib.key_count < BTREE_MAX_KEYS) {
            right_sib.keys.insert(right_sib.keys.begin(), leaf.keys.back());
            right_sib.values.insert(right_sib.values.begin(), leaf.values.back());
            ++right_sib.key_count;

            leaf.keys.pop_back();
            leaf.values.pop_back();
            --leaf.key_count;

            parent.keys[idx] = right_sib.keys.front();

            _store.save(right_sib_id, right_sib);
            _store.save(leaf_id, leaf);
            _store.save(leaf.parent_id, parent);
            return;
        }
    }

    // split 2->3 с правым: объединить leaf+right_sib, поровну разделить на три
    if (right_sib_id != NULL_PAGE) {
        BTreeNode right_sib = _store.load(right_sib_id);

        std::vector<Value> all_keys = leaf.keys;
        std::vector<RowId> all_vals = leaf.values;
        all_keys.insert(all_keys.end(), right_sib.keys.begin(), right_sib.keys.end());
        all_vals.insert(all_vals.end(), right_sib.values.begin(), right_sib.values.end());

        uint16_t total = (uint16_t)all_keys.size();
        uint16_t n1 = total / 3;
        uint16_t n2 = (total - n1) / 2;
        uint16_t n3 = total - n1 - n2;

        uint32_t mid_id = _store.alloc();
        BTreeNode mid;
        mid.is_leaf = true;
        mid.parent_id = leaf.parent_id;

        leaf.keys.assign(all_keys.begin(), all_keys.begin() + n1);
        leaf.values.assign(all_vals.begin(), all_vals.begin() + n1);
        leaf.key_count = n1;

        mid.keys.assign(all_keys.begin() + n1, all_keys.begin() + n1 + n2);
        mid.values.assign(all_vals.begin() + n1, all_vals.begin() + n1 + n2);
        mid.key_count = n2;

        right_sib.keys.assign(all_keys.begin() + n1 + n2, all_keys.end());
        right_sib.values.assign(all_vals.begin() + n1 + n2, all_vals.end());
        right_sib.key_count = n3;

        mid.next_leaf = right_sib_id;
        leaf.next_leaf = mid_id;

        _store.save(leaf_id, leaf);
        _store.save(mid_id, mid);
        _store.save(right_sib_id, right_sib);

        parent.keys[idx] = right_sib.keys[0];
        internalInsertAt(parent, idx, mid.keys[0], mid_id);
        _store.save(leaf.parent_id, parent);

        if (parent.key_count > BTREE_MAX_KEYS)
            redistributeOrSplitInternal(leaf.parent_id);
        return;
    }

    // split 2->3 с левым: объединить left_sib+leaf, поровну разделить на три
    {
        BTreeNode left_sib = _store.load(left_sib_id);

        std::vector<Value> all_keys = left_sib.keys;
        std::vector<RowId> all_vals = left_sib.values;
        all_keys.insert(all_keys.end(), leaf.keys.begin(), leaf.keys.end());
        all_vals.insert(all_vals.end(), leaf.values.begin(), leaf.values.end());

        uint16_t total = (uint16_t)all_keys.size();
        uint16_t n1 = total / 3;
        uint16_t n2 = (total - n1) / 2;
        uint16_t n3 = total - n1 - n2;

        uint32_t mid_id = _store.alloc();
        BTreeNode mid;
        mid.is_leaf = true;
        mid.parent_id = leaf.parent_id;

        left_sib.keys.assign(all_keys.begin(), all_keys.begin() + n1);
        left_sib.values.assign(all_vals.begin(), all_vals.begin() + n1);
        left_sib.key_count = n1;

        mid.keys.assign(all_keys.begin() + n1, all_keys.begin() + n1 + n2);
        mid.values.assign(all_vals.begin() + n1, all_vals.begin() + n1 + n2);
        mid.key_count = n2;

        leaf.keys.assign(all_keys.begin() + n1 + n2, all_keys.end());
        leaf.values.assign(all_vals.begin() + n1 + n2, all_vals.end());
        leaf.key_count = n3;

        mid.next_leaf = leaf_id;
        left_sib.next_leaf = mid_id;

        _store.save(left_sib_id, left_sib);
        _store.save(mid_id, mid);
        _store.save(leaf_id, leaf);

        parent.keys[idx - 1] = mid.keys[0];
        internalInsertAt(parent, idx - 1, left_sib.keys[0], mid_id);
        _store.save(leaf.parent_id, parent);

        if (parent.key_count > BTREE_MAX_KEYS)
            redistributeOrSplitInternal(leaf.parent_id);
    }
}

void BTree::redistributeOrSplitInternal(uint32_t node_id) {
    BTreeNode node = _store.load(node_id);

    // корень переполнился: split 1->2, медиана уходит в новый корень
    if (node.parent_id == NULL_PAGE) {
        uint32_t new_id = _store.alloc();
        BTreeNode right;
        right.is_leaf = false;

        uint16_t left_n = node.key_count / 2;
        Value median = node.keys[left_n];
        uint16_t right_n = node.key_count - left_n - 1;

        right.keys.assign(node.keys.begin() + left_n + 1, node.keys.end());
        right.children.assign(node.children.begin() + left_n + 1, node.children.end());
        right.key_count = right_n;

        node.keys.resize(left_n);
        node.children.resize(left_n + 1);
        node.key_count = left_n;

        for (uint32_t ch : right.children) {
            BTreeNode child = _store.load(ch);
            child.parent_id = new_id;
            _store.save(ch, child);
        }

        uint32_t root_id = _store.alloc();
        BTreeNode root;
        root.is_leaf = false;
        root.key_count = 1;
        root.keys = {median};
        root.children = {node_id, new_id};

        node.parent_id = root_id;
        right.parent_id = root_id;

        _store.save(node_id, node);
        _store.save(new_id, right);
        _store.save(root_id, root);
        _store.setRoot(root_id);
        return;
    }

    BTreeNode parent = _store.load(node.parent_id);
    int idx = findChildIndex(parent, node_id);

    uint32_t left_sib_id = (idx > 0) ? parent.children[idx - 1] : NULL_PAGE;
    uint32_t right_sib_id = (idx < (int)parent.key_count) ? parent.children[idx + 1] : NULL_PAGE;

    // повернуть разделитель вниз, крайний правый потомок left_sib переходит в node
    if (left_sib_id != NULL_PAGE) {
        BTreeNode left_sib = _store.load(left_sib_id);
        if (left_sib.key_count < BTREE_MAX_KEYS) {
            node.keys.insert(node.keys.begin(), parent.keys[idx - 1]);
            node.children.insert(node.children.begin(), left_sib.children.back());
            ++node.key_count;

            BTreeNode moved = _store.load(left_sib.children.back());
            moved.parent_id = node_id;
            _store.save(left_sib.children.back(), moved);

            parent.keys[idx - 1] = left_sib.keys.back();
            left_sib.keys.pop_back();
            left_sib.children.pop_back();
            --left_sib.key_count;

            _store.save(left_sib_id, left_sib);
            _store.save(node_id, node);
            _store.save(node.parent_id, parent);
            return;
        }
    }

    // повернуть разделитель вниз, крайний левый потомок right_sib переходит в node
    if (right_sib_id != NULL_PAGE) {
        BTreeNode right_sib = _store.load(right_sib_id);
        if (right_sib.key_count < BTREE_MAX_KEYS) {
            node.keys.push_back(parent.keys[idx]);
            node.children.push_back(right_sib.children.front());
            ++node.key_count;

            BTreeNode moved = _store.load(right_sib.children.front());
            moved.parent_id = node_id;
            _store.save(right_sib.children.front(), moved);

            parent.keys[idx] = right_sib.keys.front();
            right_sib.keys.erase(right_sib.keys.begin());
            right_sib.children.erase(right_sib.children.begin());
            --right_sib.key_count;

            _store.save(right_sib_id, right_sib);
            _store.save(node_id, node);
            _store.save(node.parent_id, parent);
            return;
        }
    }

    // split 2->3 с правым: node.keys + sep + right_sib.keys, два ключа-медианы поднять в родителя
    if (right_sib_id != NULL_PAGE) {
        BTreeNode right_sib = _store.load(right_sib_id);
        Value sep = parent.keys[idx];

        std::vector<Value> all_keys = node.keys;
        all_keys.push_back(sep);
        all_keys.insert(all_keys.end(), right_sib.keys.begin(), right_sib.keys.end());

        std::vector<uint32_t> all_ch = node.children;
        all_ch.insert(all_ch.end(), right_sib.children.begin(), right_sib.children.end());

        uint16_t total = (uint16_t)all_keys.size();
        uint16_t k1 = (total - 2) / 3;
        uint16_t k2 = (total - 2 - k1) / 2;
        uint16_t k3 = total - 2 - k1 - k2;

        Value sep0 = all_keys[k1];
        Value sep1 = all_keys[k1 + 1 + k2];

        uint32_t mid_id = _store.alloc();
        BTreeNode mid;
        mid.is_leaf = false;
        mid.parent_id = node.parent_id;

        node.keys.assign(all_keys.begin(), all_keys.begin() + k1);
        node.children.assign(all_ch.begin(), all_ch.begin() + k1 + 1);
        node.key_count = k1;

        mid.keys.assign(all_keys.begin() + k1 + 1, all_keys.begin() + k1 + 1 + k2);
        mid.children.assign(all_ch.begin() + k1 + 1, all_ch.begin() + k1 + 1 + k2 + 1);
        mid.key_count = k2;

        right_sib.keys.assign(all_keys.begin() + k1 + 1 + k2 + 1, all_keys.end());
        right_sib.children.assign(all_ch.begin() + k1 + 1 + k2 + 1, all_ch.end());
        right_sib.key_count = k3;

        for (uint32_t ch : mid.children) {
            BTreeNode child = _store.load(ch);
            child.parent_id = mid_id;
            _store.save(ch, child);
        }

        _store.save(node_id, node);
        _store.save(mid_id, mid);
        _store.save(right_sib_id, right_sib);

        parent.keys[idx] = sep1;
        internalInsertAt(parent, idx, sep0, mid_id);
        _store.save(node.parent_id, parent);

        if (parent.key_count > BTREE_MAX_KEYS)
            redistributeOrSplitInternal(node.parent_id);
        return;
    }

    // split 2->3 с левым: left_sib.keys + sep + node.keys, два ключа-медианы поднять в родителя
    {
        BTreeNode left_sib = _store.load(left_sib_id);
        Value sep = parent.keys[idx - 1];

        std::vector<Value> all_keys = left_sib.keys;
        all_keys.push_back(sep);
        all_keys.insert(all_keys.end(), node.keys.begin(), node.keys.end());

        std::vector<uint32_t> all_ch = left_sib.children;
        all_ch.insert(all_ch.end(), node.children.begin(), node.children.end());

        uint16_t total = (uint16_t)all_keys.size();
        uint16_t k1 = (total - 2) / 3;
        uint16_t k2 = (total - 2 - k1) / 2;
        uint16_t k3 = total - 2 - k1 - k2;

        Value sep0 = all_keys[k1];
        Value sep1 = all_keys[k1 + 1 + k2];

        uint32_t mid_id = _store.alloc();
        BTreeNode mid;
        mid.is_leaf = false;
        mid.parent_id = node.parent_id;

        left_sib.keys.assign(all_keys.begin(), all_keys.begin() + k1);
        left_sib.children.assign(all_ch.begin(), all_ch.begin() + k1 + 1);
        left_sib.key_count = k1;

        mid.keys.assign(all_keys.begin() + k1 + 1, all_keys.begin() + k1 + 1 + k2);
        mid.children.assign(all_ch.begin() + k1 + 1, all_ch.begin() + k1 + 1 + k2 + 1);
        mid.key_count = k2;

        node.keys.assign(all_keys.begin() + k1 + 1 + k2 + 1, all_keys.end());
        node.children.assign(all_ch.begin() + k1 + 1 + k2 + 1, all_ch.end());
        node.key_count = k3;

        for (uint32_t ch : mid.children) {
            BTreeNode child = _store.load(ch);
            child.parent_id = mid_id;
            _store.save(ch, child);
        }

        _store.save(left_sib_id, left_sib);
        _store.save(mid_id, mid);
        _store.save(node_id, node);

        parent.keys[idx - 1] = sep0;
        internalInsertAt(parent, idx, sep1, mid_id);
        _store.save(node.parent_id, parent);

        if (parent.key_count > BTREE_MAX_KEYS)
            redistributeOrSplitInternal(node.parent_id);
    }
}
