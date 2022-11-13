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

// #define NDEBUG
#define HOST

#ifdef HOST
    #include "../../libs/print_lib.hpp"
    #define DUMP(obj) std::cout << #obj ": " << obj << "\n"
#else
    #define DUMP(obj)
#endif

template <typename T, std::size_t N>
std::ostream&
print_array (std::ostream& os,
             const std::array <T, N>& arr,
             std::size_t size)
{
    if constexpr (N != 0) {
        if (size == 0) {
            return os;
        }

        os << arr[0];
        for (std::size_t i = 1; i < std::min (N, size); ++i) {
            os << ", " << arr[i];
        }
    }

    return os;
}

const auto MAX_NUM_KEYS = 4096 / 8;
// typedef uint32_t hash_t;

class btree_num_t {
public:
    typedef int key_t;

    struct node_t {
        unsigned size = 0;
        std::array <key_t,   MAX_NUM_KEYS> keys {-1};           // For kill
        std::array <node_t*, MAX_NUM_KEYS + 1> poss {nullptr};  // For work

        unsigned
        num_pos () const noexcept {
            return size + 1;
        }

        unsigned
        num_keys () const noexcept {
            return size;
        }

        bool
        is_leaf () const noexcept {
            return poss[0] == nullptr;
        }

        std::ostream&
        dump (std::ostream& os = std::cout) const {
            os << "{";
            if (num_keys () != 0) {
                os << "keys = [";
                print_array (os, keys, num_keys ())
                    << "], poss = [";
                print_array (os, poss, num_pos ())
                    << "]";
            }
            return os << "}";
        }

        std::array <key_t, MAX_NUM_KEYS>::iterator
        keys_begin () noexcept {
            return keys.begin ();
        }

        std::array <key_t, MAX_NUM_KEYS>::iterator
        keys_end () noexcept {
            return keys_begin () + num_keys ();
        }

        std::array <node_t*, MAX_NUM_KEYS>::iterator
        poss_begin () noexcept {
            return poss.begin ();
        }

        std::array <node_t*, MAX_NUM_KEYS + 1>::iterator
        poss_end () noexcept {
            return poss_begin () + num_pos ();
        }

        void
        insert (key_t key, node_t* pos) {
            if (num_keys () == MAX_NUM_KEYS) {
                throw std::runtime_error ("insert in already full node");
            }

            const auto it_key = std::upper_bound (keys.begin (), keys_end (), key);
            std::copy (it_key, keys_end (), it_key + 1);
            *it_key = key;

            const auto it_pos = poss.begin () + (it_key - keys.begin ());
            std::copy (it_pos, poss_end (), it_pos + 1);
            *it_pos = pos;

            ++size;
        }

        bool
        remove (key_t key) {
            auto it_key = std::lower_bound (keys_begin (), keys_end (), key);
            if (it_key == keys_end () || *it_key != key) {
                return false;
            }

            auto it_pos = poss_begin () + (it_key - keys_begin ());
            std::copy (it_key + 1, keys_end (), it_key);
            std::copy (it_pos + 1, poss_end (), it_pos);
            --size;

            return true;
        }

        bool
        remove_right (key_t key) {
            auto it_key = std::lower_bound (keys_begin (), keys_end (), key);
            if (it_key == keys_end () || *it_key != key) {
                return false;
            }

            auto it_pos = poss_begin () + (it_key - keys_begin ());
            std::copy (it_key + 1, keys_end (), it_key);
            std::copy (it_pos + 2, poss_end (), it_pos + 1);
            --size;

            return true;
        }
    };

    node_t* root = new node_t;

private:
    void
    destruct_childs (node_t* node) {
        if (node->is_leaf ()) {
            return;
        }

        for (int i = 0; i < node->num_pos (); ++i) {
            destruct_childs (node->poss[i]);
            delete node->poss[i];
        }
    }

public:
    ~btree_num_t () {
        if (root->size != 0) {
            destruct_childs (root);
        }

        delete root;
    }

private:
    struct node_pos_t {
        node_t* node = nullptr;
        int pos = 0;

        node_pos_t () = default;

        node_pos_t (node_t* node, int pos) :
            node (node),
            pos (pos)
        {}
    };

    void
    insert_parts (node_t* node,
                  key_t key, node_t* pos,
                  std::stack <node_t*>& stack)
    {
        const auto num_keys = node->num_keys ();
        if (num_keys < MAX_NUM_KEYS) {
            node->insert (key, pos);
            return;
        } else if (num_keys > MAX_NUM_KEYS) {
            node->dump (std::cerr);
            throw std::runtime_error ("over num keys");
        }

        // Here node->num_keys () == MAX_NUM_KEYS
        auto middle_i = num_keys / 2;
        key_t middle_key = node->keys[middle_i];

        // Create left node
        node_t* left_node = new node_t;
        std::copy (node->keys_begin (), node->keys_begin () + middle_i,
                   left_node->keys_begin ());
        std::copy (node->poss_begin (), node->poss_begin () + middle_i + 1,
                   left_node->poss_begin ());
        left_node->size = middle_i;
        node_t* middle_pos = left_node;

        // Create right node
        auto begin_key = middle_i + 1;
        auto begin_pos = middle_i + 1;
        std::copy (node->keys_begin () + begin_key, node->keys_end (),
                   node->keys_begin ());
        std::copy (node->poss_begin () + begin_pos, node->poss_end (),
                   node->poss_begin ());
        node->size -= 1 + middle_i;

        // Insert key and pos
        if (key < middle_key) {
            left_node->insert (key, pos);
        } else {
            node->insert (key, pos);
        }

        if (stack.size () != 0) {
            node_t* cur_parent = stack.top ();
            stack.pop ();
            insert_parts (cur_parent, middle_key, middle_pos, stack);
        } else {
            node_t* right_node = node;
            assert (right_node == root);

            root = new node_t;
            root->size = 1;
            root->keys[0] = middle_key;
            root->poss[0] = left_node;
            root->poss[1] = right_node;
        }
    }

    // Return parents stack and leaf node
    std::pair <std::stack <node_t*>, node_t*>
    get_path_insert (key_t key) {
        std::stack <node_t*> stack;
        node_t* cur_node = root;

        while (true) {
            if (cur_node->is_leaf ()) {
                return {stack, cur_node};
            }

            auto it_key = std::upper_bound (cur_node->keys_begin (),
                                            cur_node->keys_end (), key);
            int pos_index = it_key - cur_node->keys_begin ();
            stack.push (cur_node);
            cur_node = cur_node->poss[pos_index];
        }
    }

    // todo: opt stack use height tree
    // If not found, second.node == nullptr
    std::pair <std::stack <node_pos_t>, node_pos_t>
    get_path_find (key_t key) const {
        std::stack <node_pos_t> stack;
        node_t* cur_node = root;

        while (true) {
            auto it_key = std::lower_bound (cur_node->keys_begin (),
                                            cur_node->keys_end (), key);

            int key_index = it_key - cur_node->keys_begin ();
            if (it_key != cur_node->keys_end () && *it_key == key) {
                return {stack, {cur_node, key_index}};
            }
            if (cur_node->is_leaf ()) {
                return {stack, {}};
            }

            stack.push ({cur_node, key_index});
            cur_node = cur_node->poss[key_index];
        }

        assert (0);
        return {{}, {}};
    }

public:
    void
    insert (key_t key) {
        auto[parent_stack, node] = get_path_insert (key);
        insert_parts (node, key, nullptr, parent_stack);
    }

public:
    node_pos_t
    find (key_t key) const {
        if (root->num_keys () == 0) {
            return {};
        }

        node_t* cur_node = root;
        while (true) {
            auto it_key = std::lower_bound (cur_node->keys_begin (),
                                            cur_node->keys_end (), key);
            int key_index = it_key - cur_node->keys_begin ();

            if (it_key != cur_node->keys_end () && *it_key == key) {
                return {cur_node, key_index};
            } else {
                if (cur_node->is_leaf ()) {
                    return {};
                }

                cur_node = cur_node->poss[key_index];
            }
        }
        return {};
    }

private:
    // stack.size () > 0
    static bool
    is_rightmost_node (const std::stack <node_pos_t>& stack) {
        assert (stack.size () > 0);

        const auto& parent_pos = stack.top ();
        return parent_pos.pos == parent_pos.node->num_pos () - 1;
    }

    static bool
    is_leftmost_node (const std::stack <node_pos_t>& stack) {
        assert (stack.size () > 0);

        const auto& parent_pos = stack.top ();
        return parent_pos.pos == 0;
    }

    void
    remove_if_leaf_impl (key_t key,
                         std::stack <node_pos_t>& stack,
                         node_pos_t node_pos) {
        node_t* node = node_pos.node;

        node->remove_right (key);
        if (node->size >= MAX_NUM_KEYS / 2) {
            return;
        }

        auto& parent_pos = stack.top ();

        const bool is_rightmost = is_rightmost_node (stack);
        if (!is_rightmost) {
            node_t* left_neighbor = node;
            node_t* right_neighbor = parent_pos.node->poss[parent_pos.pos + 1];

            if (right_neighbor->size > MAX_NUM_KEYS / 2) {
                key_t& parent_key = parent_pos.node->keys[parent_pos.pos];

                left_neighbor->keys[left_neighbor->num_keys ()] = parent_key;
                left_neighbor->poss[left_neighbor->num_pos ()] = right_neighbor->poss[0];
                ++left_neighbor->size;

                const key_t leftmost_key_neighbor = right_neighbor->keys[0];
                parent_key = leftmost_key_neighbor;
                right_neighbor->remove (leftmost_key_neighbor);
                return;
            }
        }

        const bool is_leftmost = is_leftmost_node (stack);
        if (!is_leftmost) {
            node_t* left_neighbor = parent_pos.node->poss[parent_pos.pos - 1];
            node_t* right_neighbor = node;

            if (left_neighbor->size > MAX_NUM_KEYS / 2) {
                key_t& parent_key = parent_pos.node->keys[parent_pos.pos - 1];
                right_neighbor->insert (parent_key,
                                        left_neighbor->poss[left_neighbor->num_pos () - 1]);

                --left_neighbor->size;
                parent_key = left_neighbor->keys[left_neighbor->num_keys ()];
                return;
            }
        }

        // Merge with neighbors
        node_t* right_neighbor = nullptr, *left_neighbor = nullptr;
        key_t parent_key = -1;

        if (!is_rightmost) {
            right_neighbor = parent_pos.node->poss[parent_pos.pos + 1];
            left_neighbor = node;
            parent_key = parent_pos.node->keys[parent_pos.pos];
        } else {
            right_neighbor = node;
            left_neighbor = parent_pos.node->poss[parent_pos.pos - 1];
            parent_key = parent_pos.node->keys[parent_pos.pos - 1];
        }

        // They are both leafs => copy only keys
        *left_neighbor->keys_end () = parent_key;
        std::copy (right_neighbor->keys_begin (), right_neighbor->keys_end (),
                   left_neighbor->keys_end () + 1);
        std::copy (right_neighbor->poss_begin (), right_neighbor->poss_end (),
                   left_neighbor->poss_end ());
        left_neighbor->size += right_neighbor->size + 1;
        delete right_neighbor;

        parent_pos.node->poss[parent_pos.pos + 1] = nullptr;
        parent_pos.node->poss[parent_pos.pos + 0] = left_neighbor;
        // parent_pos.node->remove (parent_key);
        // draw ();
        if (parent_pos.node != root) {
            stack.pop ();
            remove_if_leaf_impl (parent_key, stack, parent_pos);
        } else {
            if (root->size == 1) {
                delete root;
                root = left_neighbor;
            } else {
                parent_pos.node->remove_right (parent_key);
            }
        }

        return;
    }

    // node_pos.node->is_leaf () == false
    void
    remove_if_not_leaf_impl (key_t key,
                             std::stack <node_pos_t>& stack,
                             node_pos_t node_pos) {
        // Save leftmost key in right subtree of node_pos

        node_t* subtree = node_pos.node->poss[node_pos.pos + 1];
        stack.push ({node_pos.node, node_pos.pos + 1});
        while (!subtree->is_leaf ()) {
            stack.push ({subtree, 0});
            subtree = subtree->poss[0];
        }
        key_t leftmost_key = subtree->keys[0];

        // Remove leftmost key
        remove_if_leaf_impl (leftmost_key, stack, {subtree, 0});

        // Change key to leftmoost key
        node_pos_t cur = find (key);
        cur.node->keys[cur.pos] = leftmost_key;

        return;
    }

public:
    bool
    remove (key_t key) {
        if (root->is_leaf ()) {
            return root->remove (key);
        }

        auto[stack, node_pos] = get_path_find (key);
        if (node_pos.node == nullptr) {
            return false;
        }

        if (node_pos.node->is_leaf ()) {
            remove_if_leaf_impl (key, stack, node_pos);
        } else {
            remove_if_not_leaf_impl (key, stack, node_pos);
        }

        return true;
    }

private:
    std::ostream&
    draw_node_keys (std::ostream& os, const node_t& node, unsigned index) const {
        if (node.size == 0) {
            return os;
        }

        os << "node" << index << "[label = \"<p0>";
        for (int i = 0; i < node.size; ++i) {
            os << "| <k" << i << "> " << node.keys[i]
            << "| <p" << i+1 << ">";
        }
        return os << "\"];\n";
    }

    void
    draw_arrow (std::ostream& os,
                unsigned index_from, unsigned index_to,
                unsigned index_ptr, unsigned index_key) const {
        // "node0":f2 -> "node4":f1;
        os << "\"node" << index_from << "\":p" << index_ptr << " -> "
           << "\"node" << index_to   << "\":k" << index_key << ";\n";
    }

    unsigned
    draw_node (std::ostream& os, const node_t* node, unsigned index = 0) const {
        if (node == nullptr) {
            return index;
        }

        const auto parent_index = index;
        draw_node_keys (os, *node, parent_index);
        for (int i = 0; i < node->size + 1; ++i) {
            if (node->poss[i] != nullptr) {
                ++index;
                node_t* child = node->poss[i];
                auto new_index = draw_node (os, child, index);
                draw_arrow (os, parent_index, index, i, (child->size - 1) / 2);
                index = new_index;
            }
        }

        return index;
    }

    void
    draw_impl () const {
        static unsigned number_dump = 0;

        std::string name = "graph_";
        (name += std::to_string (number_dump)) += ".gv";
        std::fstream os {name, std::ios_base::out};

        os << "digraph G {\n"
              "node [shape = record, height=.1];\n";
        draw_node (os, root);
        os << "}";

        ++number_dump;
    }

public:
    void
    draw () const {
        if constexpr (true) {
            draw_impl ();
        }
    }
};
