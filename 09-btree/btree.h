/*
    https://github.com/DenRen/BabichevAlgo/blob/main/H5/H5.4-bigbook/btree.hpp
    m clean && m test_btree.out && ./test_btree.out
*/

#pragma once

#include <iostream>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <map>
#include <queue>
#include <set>
#include <iomanip>
#include <stack>
#include <fstream>

#define NDEBUG
// #define HOST

#ifdef HOST
#include "../../libs/print_lib.hpp"
#define DUMP(obj) std::cout << #obj ": " << obj << "\n"
#else
#define DUMP(obj)
#endif

template <typename T>
std::ostream&
print_vector(std::ostream& os,
             const std::vector <T>& arr,
             std::size_t size)
{
    if (size == 0 || arr.empty())
        return os;

    os << arr[0];
    for (std::size_t i = 1; i < std::min(arr.size(), size); ++i)
        os << ", " << arr[i];

    return os;
}

template <typename Key>
class BTree
{
public:
    struct Node
    {
        unsigned size = 0;
        std::vector <Key> keys;
        std::vector <Node*> poss;

        Node(unsigned capacity)
            : keys(capacity, -1)            // For kill
            , poss(capacity + 1, nullptr)   // For work
        {}

        unsigned num_pos() const noexcept { return size + 1; }
        unsigned num_keys() const noexcept { return size; }
        bool is_leaf() const noexcept { return poss[0] == nullptr; }

        std::ostream&
        dump(std::ostream& os = std::cout) const
        {
            os << "{";

            if (num_keys() != 0)
            {
                os << "keys = [";
                print_vector(os, keys, num_keys()) << "], poss = [";
                print_vector(os, poss, num_pos()) << "]";
            }

            return os << "}";
        }

        auto keys_begin() noexcept { return keys.begin(); }
        auto keys_end() noexcept { return keys_begin() + num_keys(); } // todo
        auto poss_begin() noexcept { return poss.begin(); }
        auto poss_end() noexcept { return poss_begin() + num_pos(); }

        void insert(Key key,
                    Node* pos)
        {
            if (num_keys() == keys.size())
                throw std::runtime_error("insert in already full node");

            const auto it_key = std::upper_bound(keys.begin(), keys_end(), key);
            std::copy(it_key, keys_end(), it_key + 1);
            *it_key = key;

            const auto it_pos = poss.begin() + (it_key - keys.begin());
            std::copy(it_pos, poss_end(), it_pos + 1);
            *it_pos = pos;

            ++size;
        }

        bool remove(Key key)
        {
            auto it_key = std::lower_bound(keys_begin(), keys_end(), key);
            if (it_key == keys_end() || *it_key != key)
                return false;

            auto it_pos = poss_begin() + (it_key - keys_begin());
            std::copy(it_key + 1, keys_end(), it_key);
            std::copy(it_pos + 1, poss_end(), it_pos);
            --size;

            return true;
        }

        bool remove_right(Key key)
        {
            auto it_key = std::lower_bound(keys_begin(), keys_end(), key);
            if (it_key == keys_end() || *it_key != key) {
                return false;
            }

            auto it_pos = poss_begin() + (it_key - keys_begin());
            std::copy(it_key + 1, keys_end(), it_key);
            std::copy(it_pos + 2, poss_end(), it_pos + 1);
            --size;

            return true;
        }
    };

private:
    void destruct_childs(Node* node)
    {
        if (node->is_leaf())
            return;

        for (int i = 0; i < (int)node->num_pos(); ++i)
        {
            destruct_childs(node->poss[i]);
            delete node->poss[i];
        }
    }

    unsigned m_num_keys;
    Node* root;

public:
    BTree(unsigned num_keys)
        : m_num_keys{ std::max(3u, num_keys) }
        , root{ new Node{ m_num_keys } }
    {}

    ~BTree()
    {
        if (root->size != 0)
            destruct_childs(root);

        delete root;
    }

private:
    struct node_pos_t
    {
        Node* node = nullptr;
        int pos = 0;

        node_pos_t() = default;

        node_pos_t(Node* node,
                   int pos)
            : node(node)
            , pos(pos)
        {}
    };

    using ParentStack = std::stack<node_pos_t>;

public:
    class Iterator
    {
    public:
        Iterator(node_pos_t node_pos, ParentStack parent_stack)
            : node_pos{ node_pos }
            , parent_stack{ std::move(parent_stack) }
        {}

        Key& operator*()
        {
            return node_pos.node->keys[node_pos.pos];
        }
        Iterator& operator++()
        {
            if (!node_pos.node->is_leaf())
            {
                int pos_end = node_pos.node->num_pos();
                if (++node_pos.pos == pos_end && parent_stack.empty()) // Only root state
                    return *this;

                do {
                    parent_stack.push(node_pos);
                    node_pos.node = node_pos.node->poss[node_pos.pos];
                    node_pos.pos = 0;
                } while (!node_pos.node->is_leaf());

                return *this;
            }

            int pos_end = node_pos.node->num_keys();
            while (node_pos.pos + 1 == pos_end)
            {
                if (parent_stack.empty())
                {
                    ++node_pos.pos;
                    return *this;           // node_pos_t{ root, root->num_pos() }
                }
                node_pos = parent_stack.top();
                parent_stack.pop();
                pos_end = node_pos.node->num_pos();    // Already not leaf
            }

            if (node_pos.node->is_leaf())
                ++node_pos.pos;

            return *this;
        }

        bool operator==(const Iterator& rhs) noexcept
        {
            return (node_pos.node == rhs.node_pos.node) && (node_pos.pos == rhs.node_pos.pos);
        }

        bool operator!=(const Iterator& rhs) noexcept
        {
            return !(*this == rhs);
        }

        bool is_end()
        {
            Node* node = node_pos.node;
            int end_pos = node->num_keys() + !node->is_leaf();
            return parent_stack.empty() && (node_pos.pos == end_pos);
        }
    private:
        node_pos_t node_pos;
        ParentStack parent_stack;
    };

    Iterator begin()
    {
        ParentStack parent_stack;
        Node* curr = root;
        while (!curr->is_leaf())
        {
            parent_stack.emplace(curr, 0);
            curr = *curr->poss_begin();
        }

        return { node_pos_t{ curr, 0 }, std::move(parent_stack) };
    }

    Iterator end()
    {
        int end_pos = root->num_keys() + !root->is_leaf();
        return { node_pos_t{ root, (int) end_pos }, {} };
    }

private:
    void insert_parts(Node* node,
                      Key key,
                      Node* pos,
                      std::stack <Node*>& stack)
    {
        const auto num_keys = node->num_keys();
        if (num_keys < m_num_keys)
        {
            node->insert(key, pos);
            return;
        }
        else if (num_keys > m_num_keys)
        {
            node->dump(std::cerr);
            throw std::runtime_error("over num keys");
        }

        // Here node->num_keys() == MAX_NUM_KEYS
        const auto middle_i = num_keys / 2;
        Key middle_key = node->keys[middle_i];

        // Create left node
        Node* left_node = new Node{ m_num_keys };
        std::copy(node->keys_begin(), node->keys_begin() + middle_i, left_node->keys_begin());
        std::copy(node->poss_begin(), node->poss_begin() + middle_i + 1, left_node->poss_begin());
        left_node->size = middle_i;
        Node* middle_pos = left_node;

        // Create right node
        auto begin_key = middle_i + 1;
        auto begin_pos = middle_i + 1;
        std::copy(node->keys_begin() + begin_key, node->keys_end(), node->keys_begin());
        std::copy(node->poss_begin() + begin_pos, node->poss_end(), node->poss_begin());
        node->size -= 1 + middle_i;

        // Insert key and pos
        if (key < middle_key)
            left_node->insert(key, pos);
        else
            node->insert(key, pos);

        if (stack.size() != 0)
        {
            Node* cur_parent = stack.top();
            stack.pop();
            insert_parts(cur_parent, middle_key, middle_pos, stack);
        }
        else
        {
            Node* right_node = node;
            assert(right_node == root);

            root = new Node{ m_num_keys };
            root->size = 1;
            root->keys[0] = middle_key;
            root->poss[0] = left_node;
            root->poss[1] = right_node;
        }
    }

    // Return parents stack and leaf node
    std::pair <std::stack <Node*>, Node*>
    get_path_insert(Key key)
    {
        std::stack <Node*> stack;
        Node* cur_node = root;

        while (true)
        {
            if (cur_node->is_leaf())
                return { stack, cur_node };

            auto it_key = std::upper_bound(cur_node->keys_begin(), cur_node->keys_end(), key);
            int pos_index = it_key - cur_node->keys_begin();
            stack.push(cur_node);
            cur_node = cur_node->poss[pos_index];
        }
    }

    // todo: opt stack use height tree
    // If not found, second.node == nullptr
    std::pair <ParentStack, node_pos_t>
    get_path_find(Key key) const
    {
        ParentStack stack;
        Node* cur_node = root;

        while (true)
        {
            auto it_key = std::lower_bound(cur_node->keys_begin(), cur_node->keys_end(), key);

            int key_index = it_key - cur_node->keys_begin();
            if (it_key != cur_node->keys_end() && *it_key == key)
                return { stack, {cur_node, key_index} };

            if (cur_node->is_leaf())
                return { stack, {} };

            stack.emplace(cur_node, key_index);
            cur_node = cur_node->poss[key_index];
        }

        assert(0);
        return { {}, {} };
    }

public:
    void insert(Key key)
    {
        auto [parent_stack, node] = get_path_insert(key);
        insert_parts(node, key, nullptr, parent_stack);
    }

public:
    node_pos_t contain(Key key) const
    {
        if (root->num_keys() == 0)
            return {};

        Node* cur_node = root;
        while (true)
        {
            auto it_key = std::lower_bound(cur_node->keys_begin(), cur_node->keys_end(), key);
            int key_index = it_key - cur_node->keys_begin();

            if (it_key != cur_node->keys_end() && *it_key == key)
                return { cur_node, key_index };
            else
            {
                if (cur_node->is_leaf())
                    return {};

                cur_node = cur_node->poss[key_index];
            }
        }

        return {};
    }

    Iterator find(Key key) // const TODO: add ConstIterator
    {
        if (root->num_keys() == 0)
            return end();

        ParentStack parent_stack;
        Node* cur_node = root;
        while (true)
        {
            auto it_key = std::lower_bound(cur_node->keys_begin(), cur_node->keys_end(), key);
            int key_index = it_key - cur_node->keys_begin();

            if (it_key != cur_node->keys_end() && *it_key == key)
                return {{ cur_node, key_index }, parent_stack};
            else
            {
                if (cur_node->is_leaf())
                    return end();

                parent_stack.emplace(cur_node, key_index);
                cur_node = cur_node->poss[key_index];
            }
        }

        return end();
    }

private:
    // stack.size () > 0
    static bool is_rightmost_node(const ParentStack& stack)
    {
        assert(stack.size() > 0);

        const auto& parent_pos = stack.top();
        return parent_pos.pos == (int)parent_pos.node->num_pos() - 1;
    }

    static bool is_leftmost_node(const ParentStack& stack)
    {
        assert(stack.size() > 0);

        const auto& parent_pos = stack.top();
        return parent_pos.pos == 0;
    }

    void remove_if_leaf_impl(Key key,
                             ParentStack& stack,
                             node_pos_t node_pos)
    {
        Node* node = node_pos.node;

        node->remove_right(key);
        if (node->size >= m_num_keys / 2)
            return;

        auto& parent_pos = stack.top();

        const bool is_rightmost = is_rightmost_node(stack);
        if (!is_rightmost)
        {
            Node* left_neighbor = node;
            Node* right_neighbor = parent_pos.node->poss[parent_pos.pos + 1];

            if (right_neighbor->size > m_num_keys / 2)
            {
                Key& parent_key = parent_pos.node->keys[parent_pos.pos];

                left_neighbor->keys[left_neighbor->num_keys()] = parent_key;
                left_neighbor->poss[left_neighbor->num_pos()] = right_neighbor->poss[0];
                ++left_neighbor->size;

                const Key leftmost_key_neighbor = right_neighbor->keys[0];
                parent_key = leftmost_key_neighbor;
                right_neighbor->remove(leftmost_key_neighbor);
                return;
            }
        }

        const bool is_leftmost = is_leftmost_node(stack);
        if (!is_leftmost)
        {
            Node* left_neighbor = parent_pos.node->poss[parent_pos.pos - 1];
            Node* right_neighbor = node;

            if (left_neighbor->size > m_num_keys / 2) {
                Key& parent_key = parent_pos.node->keys[parent_pos.pos - 1];
                right_neighbor->insert(parent_key,
                    left_neighbor->poss[left_neighbor->num_pos() - 1]);

                --left_neighbor->size;
                parent_key = left_neighbor->keys[left_neighbor->num_keys()];
                return;
            }
        }

        // Merge with neighbors
        Node* right_neighbor = nullptr, * left_neighbor = nullptr;
        Key parent_key = -1;

        if (!is_rightmost)
        {
            right_neighbor = parent_pos.node->poss[parent_pos.pos + 1];
            left_neighbor = node;
            parent_key = parent_pos.node->keys[parent_pos.pos];
        }
        else
        {
            right_neighbor = node;
            left_neighbor = parent_pos.node->poss[parent_pos.pos - 1];
            parent_key = parent_pos.node->keys[parent_pos.pos - 1];
        }

        // They are both leafs => copy only keys
        *left_neighbor->keys_end() = parent_key;
        std::copy(right_neighbor->keys_begin(), right_neighbor->keys_end(), left_neighbor->keys_end() + 1);
        std::copy(right_neighbor->poss_begin(), right_neighbor->poss_end(), left_neighbor->poss_end());
        left_neighbor->size += right_neighbor->size + 1;
        delete right_neighbor;

        parent_pos.node->poss[parent_pos.pos + 1] = nullptr;
        parent_pos.node->poss[parent_pos.pos + 0] = left_neighbor;
        if (parent_pos.node != root)
        {
            stack.pop();
            remove_if_leaf_impl(parent_key, stack, parent_pos);
        }
        else
        {
            if (root->size == 1)
            {
                delete root;
                root = left_neighbor;
            }
            else
                parent_pos.node->remove_right(parent_key);
        }

        return;
    }

    // node_pos.node->is_leaf () == false
    void remove_if_not_leaf_impl(Key key,
                                 ParentStack& stack,
                                 node_pos_t node_pos)
    {
        // Save leftmost key in right subtree of node_pos
        Node* subtree = node_pos.node->poss[node_pos.pos + 1];
        stack.emplace(node_pos.node, node_pos.pos + 1);
        while (!subtree->is_leaf())
        {
            stack.emplace(subtree, 0);
            subtree = subtree->poss[0];
        }
        Key leftmost_key = subtree->keys[0];

        // Remove leftmost key
        remove_if_leaf_impl(leftmost_key, stack, { subtree, 0 });

        // Change key to leftmoost key
        node_pos_t cur = contain(key);
        cur.node->keys[cur.pos] = leftmost_key;

        return;
    }

public:
    bool remove(Key key)
    {
        if (root->is_leaf())
            return root->remove(key);

        auto [stack, node_pos] = get_path_find(key);
        if (node_pos.node == nullptr)
            return false;

        if (node_pos.node->is_leaf())
            remove_if_leaf_impl(key, stack, node_pos);
        else
            remove_if_not_leaf_impl(key, stack, node_pos);

        return true;
    }

private:
    std::ostream&
    draw_node_keys(std::ostream& os,
                   const Node& node,
                   unsigned index) const
    {
        if (node.size == 0)
            return os;

        os << "node" << index << "[label = \"<p0>";
        for (int i = 0; i < (int)node.size; ++i)
            os << "| <k" << i << "> " << node.keys[i] << "| <p" << i + 1 << ">";

        return os << "\"];\n";
    }

    void draw_arrow(std::ostream& os,
                    unsigned index_from,
                    unsigned index_to,
                    unsigned index_ptr,
                    unsigned index_key) const
    {
        // "node0":f2 -> "node4":f1;
        os << "\"node" << index_from << "\":p" << index_ptr << " -> "
           << "\"node" << index_to   << "\":k" << index_key << ";\n";
    }

    unsigned draw_node(std::ostream& os,
                       const Node* node,
                       unsigned index = 0) const
    {
        if (node == nullptr)
            return index;

        const auto parent_index = index;
        draw_node_keys(os, *node, parent_index);
        for (int i = 0; i < (int)node->size + 1; ++i)
        {
            if (node->poss[i] != nullptr)
            {
                ++index;
                Node* child = node->poss[i];
                auto new_index = draw_node(os, child, index);
                draw_arrow(os, parent_index, index, i, (child->size - 1) / 2);
                index = new_index;
            }
        }

        return index;
    }

    void draw_impl() const
    {
        static unsigned number_dump = 0;

        std::string name = "graph_";
        (name += std::to_string(number_dump)) += ".gv";
        std::fstream os{ name, std::ios_base::out };

        os << "digraph G {\n"
              "node [shape = record, height=.1];\n";
        draw_node(os, root);
        os << "}";

        ++number_dump;
    }

public:
    void draw() const
    {
        if constexpr (true)
        {
            draw_impl();
        }
    }
};
