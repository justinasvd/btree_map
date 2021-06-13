#ifndef ART_DETAIL_ART_NODES_HEADER_INCLUDED
#define ART_DETAIL_ART_NODES_HEADER_INCLUDED

#include "art_node_base.h"
#include "dump_byte.h"
#include "ffs_nonzero.h"

#if !defined(NDEBUG)
#include <iostream>
#endif

#if defined(__SSE2__)
#include <emmintrin.h>
#include <smmintrin.h>
#endif

#include <algorithm>
#include <array>

#include <boost/core/ignore_unused.hpp>

namespace art
{
namespace detail
{

[[noreturn]] inline void cannot_happen(const char* file, int line, const char* func)
{
#ifndef NDEBUG
    std::cerr << "Execution reached an unreachable point at " << file << ':' << line << ": " << func
              << '\n';
    std::abort();
#else
    boost::ignore_unused(file);
    boost::ignore_unused(line);
    boost::ignore_unused(func);
    __builtin_unreachable();
#endif
}

#define ART_DETAIL_CANNOT_HAPPEN() cannot_happen(__FILE__, __LINE__, __func__)

#if defined(__SSE2__)
// Idea from https://stackoverflow.com/a/32945715/80458
inline auto _mm_cmple_epu8(__m128i x, __m128i y) noexcept
{
    return _mm_cmpeq_epi8(_mm_max_epu8(y, x), y);
}
#endif

template <typename Db> class basic_inode_impl : public art_node_base<typename Db::bitwise_key>
{
    using base_t = art_node_base<typename Db::bitwise_key>;

public:
    using inode_type = basic_inode_impl<Db>;
    using inode4_type = basic_inode_4<Db>;
    using inode16_type = basic_inode_16<Db>;
    using inode48_type = basic_inode_48<Db>;
    using inode256_type = basic_inode_256<Db>;

    using leaf_type = typename Db::leaf_type;
    using bitwise_key = typename Db::bitwise_key;
    using node_ptr = typename Db::node_ptr;

    using leaf_unique_ptr = unique_node_ptr<leaf_type, Db>;
    using const_iterator = typename Db::const_iterator;

public:
    [[nodiscard]] constexpr unsigned int num_children() const noexcept
    {
        return children_count != 0 ? static_cast<unsigned>(children_count) : 256;
    }

    [[nodiscard]] static constexpr unsigned int capacity(node_ptr node) noexcept
    {
        switch (node.tag()) {
        case node_type::I4:
            return inode4_type::capacity;
        case node_type::I16:
            return inode16_type::capacity;
        case node_type::I48:
            return inode48_type::capacity;
        case node_type::I256:
            return inode256_type::capacity;
        case node_type::LEAF:
            return 1;
        default:
            ART_DETAIL_CANNOT_HAPPEN();
        }
    }

    [[nodiscard]] static constexpr const_iterator find_child(node_ptr node,
                                                             std::uint8_t key_byte) noexcept
    {
        switch (node.tag()) {
        case node_type::I4:
            return static_cast<inode4_type*>(node.get())->find_child(key_byte);
        case node_type::I16:
            return static_cast<inode16_type*>(node.get())->find_child(key_byte);
        case node_type::I48:
            return static_cast<inode48_type*>(node.get())->find_child(key_byte);
        case node_type::I256:
            return static_cast<inode256_type*>(node.get())->find_child(key_byte);
        default:
            ART_DETAIL_CANNOT_HAPPEN();
        }
    }

    [[nodiscard]] static constexpr const_iterator leftmost_child(node_ptr node,
                                                                 unsigned start = 0) noexcept
    {
        switch (node.tag()) {
        case node_type::I4:
            return static_cast<inode4_type*>(node.get())->leftmost_child(start);
        case node_type::I16:
            return static_cast<inode16_type*>(node.get())->leftmost_child(start);
        case node_type::I48:
            return static_cast<inode48_type*>(node.get())->leftmost_child(start);
        case node_type::I256:
            return static_cast<inode256_type*>(node.get())->leftmost_child(start);
        default:
            ART_DETAIL_CANNOT_HAPPEN();
        }
    }

    [[nodiscard]] static constexpr const_iterator leftmost_leaf(node_ptr node,
                                                                unsigned start = 0) noexcept
    {
        const_iterator pos(node, 0);
        while (pos.tag() != node_type::LEAF) {
            pos = leftmost_child(pos.node(), start);
            start = 0;
        }
        return pos;
    }

    static void dump(std::ostream& os, const node_ptr node)
    {
        os << "node at: " << node.get();
        if (BOOST_UNLIKELY(node == nullptr)) {
            os << '\n';
            return;
        }

        os << ", type = ";
        switch (node.tag()) {
        case node_type::LEAF:
            os << "LEAF: ";
            static_cast<const leaf_type*>(node.get())->dump(os);
            os << '\n';
            break;
        case node_type::I4:
            os << "I4: ";
            static_cast<const inode4_type*>(node.get())->dump(os);
            break;
        case node_type::I16:
            os << "I16: ";
            static_cast<const inode16_type*>(node.get())->dump(os);
            break;
        case node_type::I48:
            os << "I48: ";
            static_cast<const inode48_type*>(node.get())->dump(os);
            break;
        default:
            assert(node.tag() == node_type::I256);
            os << "I256: ";
            static_cast<const inode256_type*>(node.get())->dump(os);
        }
    }

    [[nodiscard]] const_iterator self_iterator(node_type tag) noexcept
    {
        return const_iterator(node_ptr::create(this, tag), pos_in_parent, parent_);
    }

protected:
    constexpr basic_inode_impl(std::uint8_t min_size, bitwise_key key) noexcept
        : base_t(key)
        , parent_{}
        , pos_in_parent{}
        , children_count(min_size)
    {
    }

    static constexpr void assign_parent(inode_type& inode, node_ptr parent,
                                        std::uint8_t index) noexcept
    {
        inode.parent_ = parent;
        inode.pos_in_parent = index;
    }

    void dump(std::ostream& os) const
    {
        base_t::dump(os, this->prefix());
        os << ", parent = " << parent_.get() << ", #children = " << num_children();
    }

private:
    node_ptr parent_;
    std::uint8_t pos_in_parent;

protected:
    std::uint8_t children_count;
};

template <typename Db, unsigned MinSize, unsigned Capacity, node_type NodeType,
          class SmallerDerived, class LargerDerived, class Derived>
class basic_inode : public basic_inode_impl<Db>
{
    static_assert(NodeType != node_type::LEAF, "An inode cannot have a leaf tag");
    static_assert(!std::is_same<Derived, LargerDerived>::value, "grow(inode) -> inode");
    static_assert(!std::is_same<SmallerDerived, Derived>::value, "shrink(inode) -> inode");
    static_assert(!std::is_same<SmallerDerived, LargerDerived>::value,
                  "shrink(inode) -> grow(inode)");
    static_assert(MinSize < Capacity, "Misconfigured inode capacity: min size >= capacity");

    using parent_type = basic_inode_impl<Db>;

public:
    using bitwise_key = typename parent_type::bitwise_key;
    using node_ptr = typename parent_type::node_ptr;
    using leaf_unique_ptr = typename parent_type::leaf_unique_ptr;

    using smaller_inode_type = SmallerDerived;
    using larger_inode_type = LargerDerived;

    static constexpr unsigned min_size = MinSize;
    static constexpr unsigned capacity = Capacity;

    explicit constexpr basic_inode(bitwise_key key) noexcept
        : parent_type(MinSize, key)
    {
    }

    [[nodiscard]] constexpr bool is_full() const noexcept
    {
        return this->children_count == capacity;
    }

    [[nodiscard]] constexpr bool is_min_size() const noexcept
    {
        return this->children_count == min_size;
    }

    [[nodiscard]] static constexpr node_type static_type() noexcept { return NodeType; }

    [[nodiscard]] node_ptr tagged_self() noexcept { return node_ptr::create(this, NodeType); }

protected:
    explicit constexpr basic_inode(const LargerDerived& source_node) noexcept
        : basic_inode_impl<Db>(Capacity, source_node.prefix())
    {
        assert(source_node.is_min_size());
        assert(is_full());
    }

    void reparent(node_ptr node, std::uint8_t index) noexcept
    {
        if (node.tag() != node_type::LEAF) {
            parent_type::assign_parent(*static_cast<parent_type*>(node.get()), tagged_self(),
                                       index);
        }
    }
};

// A class used as a sentinel for basic_inode template args: the
// larger node type for the largest node type and the smaller node type for
// the smallest node type.
class fake_inode final
{
public:
    fake_inode() = delete;
};

template <typename Db>
using basic_inode_4_parent =
    basic_inode<Db, 2, 4, node_type::I4, fake_inode, basic_inode_16<Db>, basic_inode_4<Db>>;

template <typename Db> class basic_inode_4 : public basic_inode_4_parent<Db>
{
    using parent_type = basic_inode_4_parent<Db>;
    using bitwise_key = typename parent_type::bitwise_key;
    using key_size_type = typename bitwise_key::size_type;
    using inode4_type = typename parent_type::inode4_type;
    using inode16_type = typename parent_type::inode16_type;

    using node_ptr = typename parent_type::node_ptr;
    using leaf_type = typename parent_type::leaf_type;
    using leaf_unique_ptr = typename parent_type::leaf_unique_ptr;
    using const_iterator = typename Db::const_iterator;
    using iterator = typename Db::iterator;

private:
    friend Db;

    // Forward parent's c-tors here
    using basic_inode_4_parent<Db>::basic_inode_4_parent;

    [[nodiscard]] constexpr iterator populate(node_ptr child1, leaf_unique_ptr child2,
                                              std::uint8_t key_byte) noexcept
    {
        assert(child1.tag() != node_type::LEAF);
        child1->shift_right(this->prefix_length());
        const std::uint8_t child1_key = child1->front();
        child1->shift_right(1); // Consume front byte
        return add_two_to_empty(child1_key, child1, key_byte, std::move(child2));
    }

    [[nodiscard]] constexpr iterator populate(leaf_type* child1, leaf_unique_ptr child2,
                                              key_size_type offset) noexcept
    {
        const key_size_type trim = offset + this->prefix_length();
        return add_two_to_empty(child1->prefix()[trim], node_ptr::create(child1, node_type::LEAF),
                                child2->prefix()[trim], std::move(child2));
    }

public:
    constexpr basic_inode_4(const inode16_type& source_node, std::uint8_t child_to_delete)
        : parent_type(source_node)
    {
        const auto* source_keys_itr = source_node.keys.byte_array.cbegin();
        auto* keys_itr = keys.byte_array.begin();
        const auto* source_children_itr = source_node.children.cbegin();
        auto* children_itr = children.begin();

        std::uint8_t index = 0;
        while (source_keys_itr != source_node.keys.byte_array.cbegin() + child_to_delete) {
            *keys_itr++ = *source_keys_itr++;
            *children_itr = *source_children_itr++;
            this->reparent(*children_itr++, index++);
        }

        ++source_keys_itr;
        ++source_children_itr;

        while (source_keys_itr != source_node.keys.byte_array.cbegin() + inode16_type::min_size) {
            *keys_itr++ = *source_keys_itr++;
            *children_itr = *source_children_itr++;
            this->reparent(*children_itr++, index++);
        }

        assert(std::is_sorted(keys.byte_array.cbegin(),
                              keys.byte_array.cbegin() + this->children_count));
    }

    [[nodiscard]] constexpr iterator add(leaf_unique_ptr child, std::uint8_t key_byte) noexcept
    {
        auto children_count = this->children_count;

        assert(std::is_sorted(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count));

        const auto first_lt = ((keys.integer & 0xFFU) < key_byte) ? 1 : 0;
        const auto second_lt = (((keys.integer >> 8U) & 0xFFU) < key_byte) ? 1 : 0;
        const auto third_lt =
            ((children_count == 3) && ((keys.integer >> 16U) & 0xFFU) < key_byte) ? 1 : 0;
        const auto insert_pos_index = static_cast<unsigned>(first_lt + second_lt + third_lt);

        for (typename decltype(keys.byte_array)::size_type i = children_count; i > insert_pos_index;
             --i) {
            keys.byte_array[i] = keys.byte_array[i - 1];
            // TODO(laurynas): Node4 children fit into a single YMM register on AVX
            // onwards, see if it is possible to do shift/insert with it. Checked
            // plain AVX, it seems that at least AVX2 is required.
            children[i] = children[i - 1];
            this->reparent(children[i - 1], i);
        }
        keys.byte_array[insert_pos_index] = static_cast<std::uint8_t>(key_byte);
        children[insert_pos_index] = node_ptr::create(child.release(), node_type::LEAF);

        ++children_count;
        this->children_count = children_count;

        assert(std::is_sorted(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count));

        return iterator(children[insert_pos_index], insert_pos_index, this->tagged_self());
    }

    constexpr void remove(std::uint8_t child_index) noexcept
    {
        auto children_count = this->children_count;

        assert(child_index < children_count);
        assert(std::is_sorted(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count));

        for (typename decltype(keys.byte_array)::size_type i = child_index;
             i < static_cast<unsigned>(this->children_count - 1); ++i) {
            // TODO(laurynas): see the AVX2 TODO at add method
            keys.byte_array[i] = keys.byte_array[i + 1];
            children[i] = children[i + 1];
            this->reparent(children[i + 1], i);
        }

        --children_count;
        this->children_count = children_count;

        assert(std::is_sorted(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count));
    }

    [[nodiscard]] constexpr node_ptr leave_last_child(std::uint8_t child_to_delete) noexcept
    {
        assert(this->is_min_size());
        assert(child_to_delete <= 1);

        const std::uint8_t child_to_leave = 1 - child_to_delete;
        const auto child_to_leave_ptr = children[child_to_leave];

        if (child_to_leave_ptr.tag() != node_type::LEAF) {
            // Now we have to prepend inode_4's prefix to the last remaining node
            child_to_leave_ptr->shift_left(keys.byte_array[child_to_leave]);
            child_to_leave_ptr->shift_left(this->prefix());
            parent_type::assign_parent(
                *static_cast<basic_inode_impl<Db>*>(child_to_leave_ptr.get()), node_ptr{}, 0);
        }
        return child_to_leave_ptr;
    }

    [[nodiscard, gnu::pure]] const_iterator find_child(std::uint8_t key_byte) noexcept
    {
#if defined(__SSE2__)
        const auto replicated_search_key = _mm_set1_epi8(static_cast<char>(key_byte));
        const auto keys_in_sse_reg = _mm_cvtsi32_si128(static_cast<std::int32_t>(keys.integer));
        const auto matching_key_positions = _mm_cmpeq_epi8(replicated_search_key, keys_in_sse_reg);
        const auto mask = (1U << this->children_count) - 1;
        const auto bit_field =
            static_cast<unsigned>(_mm_movemask_epi8(matching_key_positions)) & mask;
        if (bit_field != 0) {
            const auto i = static_cast<unsigned>(__builtin_ctz(bit_field));
            return const_iterator(children[i], i, this->tagged_self());
        }
#else  // No SSE
       // Bit twiddling:
       // contains_byte:     __builtin_ffs:   for key index:
       //    0x80000000               0x20                3
       //      0x800000               0x18                2
       //      0x808000               0x10                1
       //          0x80                0x8                0
       //           0x0                0x0        not found
        const auto result = static_cast<decltype(keys.byte_array)::size_type>(
            // __builtin_ffs takes signed argument:
            // NOLINTNEXTLINE(hicpp-signed-bitwise)
            __builtin_ffs(static_cast<std::int32_t>(contains_byte(keys.integer, key_byte))) >> 3);

        if ((result != 0) && (result <= this->children_count))
            return const_iterator(children[result - 1], result - 1, this->tagged_self());
#endif // __SSE__

        return const_iterator{};
    }

    [[nodiscard]] constexpr const_iterator leftmost_child(unsigned start) noexcept
    {
        if (start < this->children_count)
            return const_iterator(children[start], start, this->tagged_self());
        return const_iterator{};
    }

    constexpr void replace(const_iterator pos, node_ptr child) noexcept
    {
        const std::uint8_t child_index = pos.index();
        assert(pos.parent() == this->tagged_self() && pos.node() == children[child_index]);
        children[child_index] = child;
        this->reparent(child, child_index);
    }

    constexpr void delete_subtree(Db& db_instance) noexcept
    {
        const auto children_count_copy = this->children_count;
        for (std::uint8_t i = 0; i < children_count_copy; ++i) {
            db_instance.deallocate(children[i]);
        }
    }

    [[gnu::cold, gnu::noinline]] void dump(std::ostream& os) const
    {
        parent_type::dump(os);
        const auto children_count_copy = this->children_count;
        os << ", key bytes =";
        for (std::uint8_t i = 0; i < children_count_copy; i++)
            dump_byte(os, keys.byte_array[i]);
        os << ", children:\n";
        for (std::uint8_t i = 0; i < children_count_copy; i++)
            parent_type::dump(os, children[i]);
    }

protected:
    constexpr iterator add_two_to_empty(std::uint8_t key1, node_ptr child1, std::uint8_t key2,
                                        leaf_unique_ptr&& child2) noexcept
    {
        assert(key1 != key2);
        assert(this->children_count == 2);

        const std::uint8_t key1_i = key1 < key2 ? 0 : 1;
        const std::uint8_t key2_i = key1_i == 0 ? 1 : 0;
        keys.byte_array[key1_i] = key1;
        children[key1_i] = child1;
        this->reparent(child1, key1_i);

        keys.byte_array[key2_i] = key2;
        children[key2_i] = node_ptr::create(child2.release(), node_type::LEAF);
        keys.byte_array[2] = std::uint8_t{0};
        keys.byte_array[3] = std::uint8_t{0};

        assert(std::is_sorted(keys.byte_array.cbegin(),
                              keys.byte_array.cbegin() + this->children_count));

        return iterator(children[key2_i], key2_i, this->tagged_self());
    }

    union {
        std::array<std::uint8_t, basic_inode_4::capacity> byte_array;
        std::uint32_t integer;
    } keys;

    std::array<node_ptr, basic_inode_4::capacity> children;

private:
    friend basic_inode_16<Db>;
};

static constexpr std::uint8_t empty_child = 0xFF;

template <typename Db>
using basic_inode_16_parent = basic_inode<Db, 5, 16, node_type::I16, basic_inode_4<Db>,
                                          basic_inode_48<Db>, basic_inode_16<Db>>;

template <typename Db> class basic_inode_16 : public basic_inode_16_parent<Db>
{
private:
    using parent_type = basic_inode_16_parent<Db>;
    using inode4_type = typename parent_type::inode4_type;
    using inode16_type = typename parent_type::inode16_type;
    using inode48_type = typename parent_type::inode48_type;
    using node_ptr = typename parent_type::node_ptr;
    using leaf_unique_ptr = typename parent_type::leaf_unique_ptr;
    using const_iterator = typename Db::const_iterator;
    using iterator = typename Db::iterator;

private:
    friend Db;

    using basic_inode_16_parent<Db>::basic_inode_16_parent;

    [[nodiscard]] constexpr iterator populate(unique_node_ptr<inode4_type, Db> source_node,
                                              leaf_unique_ptr child, std::uint8_t key_byte) noexcept
    {
        assert(source_node->is_full());
        assert(this->is_min_size());

        const auto keys_integer = source_node->keys.integer;
        const auto first_lt = ((keys_integer & 0xFFU) < key_byte) ? 1 : 0;
        const auto second_lt = (((keys_integer >> 8U) & 0xFFU) < key_byte) ? 1 : 0;
        const auto third_lt = (((keys_integer >> 16U) & 0xFFU) < key_byte) ? 1 : 0;
        const auto fourth_lt = (((keys_integer >> 24U) & 0xFFU) < key_byte) ? 1 : 0;
        const auto insert_pos_index =
            static_cast<unsigned>(first_lt + second_lt + third_lt + fourth_lt);

        unsigned i = 0;
        for (; i < insert_pos_index; ++i) {
            keys.byte_array[i] = source_node->keys.byte_array[i];
            children[i] = source_node->children[i];
            this->reparent(children[i], i);
        }

        keys.byte_array[i] = static_cast<std::uint8_t>(key_byte);
        children[i] = node_ptr::create(child.release(), node_type::LEAF);
        iterator inserted(children[i], i, this->tagged_self());
        ++i;

        for (; i <= inode4_type::capacity; ++i) {
            keys.byte_array[i] = source_node->keys.byte_array[i - 1];
            children[i] = source_node->children[i - 1];
            this->reparent(children[i], i);
        }
        return inserted;
    }

public:
    constexpr basic_inode_16(inode48_type& source_node, std::uint8_t child_to_delete) noexcept
        : parent_type(source_node)
    {
        source_node.verify_remove_preconditions(child_to_delete);
        source_node.child_indices[child_to_delete] = empty_child;

        // TODO(laurynas): consider AVX512 gather?
        unsigned next_child = 0;
        unsigned i = 0;
        while (true) {
            const auto source_child_i = source_node.child_indices[i];
            if (source_child_i != empty_child) {
                keys.byte_array[next_child] = static_cast<std::uint8_t>(i);
                const auto source_child_ptr = source_node.children.pointer_array[source_child_i];
                assert(source_child_ptr != nullptr);
                children[next_child] = source_child_ptr;
                this->reparent(children[next_child], next_child);
                ++next_child;
                if (next_child == basic_inode_16::capacity)
                    break;
            }
            assert(i < 255);
            ++i;
        }

        assert(basic_inode_16::capacity == this->children_count);
        assert(std::is_sorted(keys.byte_array.cbegin(),
                              keys.byte_array.cbegin() + basic_inode_16::capacity));
    }

    [[nodiscard]] constexpr iterator add(leaf_unique_ptr child, std::uint8_t key_byte) noexcept
    {
        auto children_count = this->children_count;

        const auto insert_pos_index = get_sorted_key_array_insert_position(key_byte);
        if (insert_pos_index != children_count) {
            assert(keys.byte_array[insert_pos_index] != key_byte);
            std::copy_backward(keys.byte_array.cbegin() + insert_pos_index,
                               keys.byte_array.cbegin() + children_count,
                               keys.byte_array.begin() + children_count + 1);

            for (std::uint8_t i = insert_pos_index; i != children_count; ++i)
                this->reparent(children[i], i + 1);

            std::copy_backward(children.begin() + insert_pos_index,
                               children.begin() + children_count,
                               children.begin() + children_count + 1);
        }
        keys.byte_array[insert_pos_index] = key_byte;
        children[insert_pos_index] = node_ptr::create(child.release(), node_type::LEAF);
        ++children_count;
        this->children_count = children_count;

        assert(std::is_sorted(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count));

        return iterator(children[insert_pos_index], insert_pos_index, this->tagged_self());
    }

    constexpr void remove(std::uint8_t child_index) noexcept
    {
        auto children_count = this->children_count;
        assert(child_index < children_count);
        assert(std::is_sorted(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count));

        for (unsigned i = child_index + 1; i < children_count; ++i) {
            keys.byte_array[i - 1] = keys.byte_array[i];
            children[i - 1] = children[i];
            this->reparent(children[i], i - 1);
        }

        --children_count;
        this->children_count = children_count;

        assert(std::is_sorted(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count));
    }

    [[nodiscard, gnu::pure]] constexpr const_iterator find_child(std::uint8_t key_byte) noexcept
    {
#if defined(__SSE2__)
        const auto replicated_search_key = _mm_set1_epi8(static_cast<char>(key_byte));
        const auto matching_key_positions = _mm_cmpeq_epi8(replicated_search_key, keys.sse);
        const auto mask = (1U << this->children_count) - 1;
        const auto bit_field =
            static_cast<unsigned>(_mm_movemask_epi8(matching_key_positions)) & mask;
        if (bit_field != 0) {
            const auto i = static_cast<unsigned>(__builtin_ctz(bit_field));
            return const_iterator(children[i], i, this->tagged_self());
        }
#else
#error Needs porting
#endif
        return const_iterator{};
    }

    [[nodiscard]] constexpr const_iterator leftmost_child(unsigned start) noexcept
    {
        if (start < this->children_count)
            return const_iterator(children[start], start, this->tagged_self());
        return const_iterator{};
    }

    constexpr void replace(const_iterator pos, node_ptr child) noexcept
    {
        const std::uint8_t child_index = pos.index();
        assert(pos.parent() == this->tagged_self() && pos.node() == children[child_index]);
        children[child_index] = child;
        this->reparent(children[child_index], child_index);
    }

    constexpr void delete_subtree(Db& db_instance) noexcept
    {
        const auto children_count = this->children_count;
        for (std::uint8_t i = 0; i < children_count; ++i)
            db_instance.deallocate(children[i]);
    }

    [[gnu::cold, gnu::noinline]] void dump(std::ostream& os) const
    {
        parent_type::dump(os);
        const auto children_count = this->children_count;
        os << ", key bytes =";
        for (std::uint8_t i = 0; i < children_count; ++i)
            dump_byte(os, keys.byte_array[i]);
        os << ", children:\n";
        for (std::uint8_t i = 0; i < children_count; ++i)
            parent_type::dump(os, children[i]);
    }

private:
    [[nodiscard, gnu::pure]] constexpr std::uint8_t get_sorted_key_array_insert_position(
        std::uint8_t key_byte) const noexcept
    {
        const auto children_count = this->children_count;

        assert(children_count < basic_inode_16::capacity);
        assert(std::is_sorted(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count));
        assert(std::adjacent_find(keys.byte_array.cbegin(),
                                  keys.byte_array.cbegin() + children_count) >=
               keys.byte_array.cbegin() + children_count);

#if defined(__SSE2__)
        const auto replicated_insert_key = _mm_set1_epi8(static_cast<char>(key_byte));
        const auto lesser_key_positions = _mm_cmple_epu8(replicated_insert_key, keys.sse);
        const auto mask = (1U << children_count) - 1;
        const auto bit_field =
            static_cast<unsigned>(_mm_movemask_epi8(lesser_key_positions)) & mask;
        const std::uint8_t result = (bit_field != 0) ? __builtin_ctz(bit_field) : children_count;
#else
        const std::uint8_t result =
            std::lower_bound(keys.byte_array.cbegin(), keys.byte_array.cbegin() + children_count,
                             key_byte) -
            keys.byte_array.cbegin();
#endif

        assert(result == children_count || keys.byte_array[result] != key_byte);
        return result;
    }

protected:
    union {
        std::array<std::uint8_t, basic_inode_16::capacity> byte_array;
        __m128i sse;
    } keys;
    std::array<node_ptr, basic_inode_16::capacity> children;

private:
    friend basic_inode_4<Db>;
    friend basic_inode_48<Db>;
};

template <typename Db>
using basic_inode_48_parent = basic_inode<Db, 17, 48, node_type::I48, basic_inode_16<Db>,
                                          basic_inode_256<Db>, basic_inode_48<Db>>;

template <typename Db> class basic_inode_48 : public basic_inode_48_parent<Db>
{
    using parent_type = basic_inode_48_parent<Db>;
    using inode16_type = typename parent_type::inode16_type;
    using inode48_type = typename parent_type::inode48_type;
    using inode256_type = typename parent_type::inode256_type;
    using node_ptr = typename parent_type::node_ptr;
    using leaf_unique_ptr = typename parent_type::leaf_unique_ptr;
    using const_iterator = typename Db::const_iterator;
    using iterator = typename Db::iterator;

private:
    friend Db;

    using basic_inode_48_parent<Db>::basic_inode_48_parent;

    [[nodiscard]] constexpr iterator populate(unique_node_ptr<inode16_type, Db> source_node,
                                              leaf_unique_ptr child, std::uint8_t key_byte) noexcept
    {
        assert(source_node->is_full());
        assert(this->is_min_size());

        auto* const __restrict__ source_node_ptr = source_node.get();

        // TODO(laurynas): initialize at declaration
        // Cannot use memset without C++20 atomic_ref, but even then check whether
        // this compiles to memset already
        std::fill(child_indices.begin(), child_indices.end(), empty_child);

        // TODO(laurynas): consider AVX512 scatter?
        for (std::uint8_t i = 0; i != inode16_type::capacity; ++i) {
            const std::uint8_t existing_key_byte = source_node_ptr->keys.byte_array[i];
            child_indices[existing_key_byte] = i;
            children.pointer_array[i] = source_node_ptr->children[i];
            this->reparent(children.pointer_array[i], existing_key_byte);
        }

        assert(child_indices[key_byte] == empty_child);
        child_indices[key_byte] = inode16_type::capacity;
        children.pointer_array[inode16_type::capacity] =
            node_ptr::create(child.release(), node_type::LEAF);
        iterator inserted(children.pointer_array[inode16_type::capacity], key_byte,
                          this->tagged_self());

        std::fill(std::next(children.pointer_array.begin(), inode16_type::capacity + 1),
                  children.pointer_array.end(), nullptr);
        return inserted;
    }

public:
    constexpr basic_inode_48(inode256_type& source_node, std::uint8_t child_to_delete) noexcept
        : parent_type(source_node)
    {
        source_node.children[child_to_delete] = nullptr;

        std::fill(child_indices.begin(), child_indices.end(), empty_child);

        std::uint8_t next_child = 0;
        for (unsigned child_i = 0; child_i < 256; child_i++) {
            const auto child_ptr = source_node.children[child_i];
            if (child_ptr == nullptr)
                continue;

            child_indices[child_i] = next_child;
            children.pointer_array[next_child] = source_node.children[child_i];
            this->reparent(children.pointer_array[next_child], child_i);
            ++next_child;

            if (next_child == this->children_count)
                break;
        }
    }

    constexpr iterator add(leaf_unique_ptr child, std::uint8_t key_byte) noexcept
    {
        assert(child_indices[key_byte] == empty_child);
        unsigned i{0};
#if defined(__SSE4_2__)
        const auto nullptr_vector = _mm_setzero_si128();
        while (true) {
            const auto ptr_vec0 = _mm_load_si128(&children.pointer_vector[i]);
            const auto ptr_vec1 = _mm_load_si128(&children.pointer_vector[i + 1]);
            const auto ptr_vec2 = _mm_load_si128(&children.pointer_vector[i + 2]);
            const auto ptr_vec3 = _mm_load_si128(&children.pointer_vector[i + 3]);
            const auto vec0_cmp = _mm_cmpeq_epi64(ptr_vec0, nullptr_vector);
            const auto vec1_cmp = _mm_cmpeq_epi64(ptr_vec1, nullptr_vector);
            const auto vec2_cmp = _mm_cmpeq_epi64(ptr_vec2, nullptr_vector);
            const auto vec3_cmp = _mm_cmpeq_epi64(ptr_vec3, nullptr_vector);
            // OK to treat 64-bit comparison result as 32-bit vector: we need to find
            // the first 0xFF only.
            const auto vec01_cmp = _mm_packs_epi32(vec0_cmp, vec1_cmp);
            const auto vec23_cmp = _mm_packs_epi32(vec2_cmp, vec3_cmp);
            const auto vec_cmp = _mm_packs_epi32(vec01_cmp, vec23_cmp);
            const auto cmp_mask = static_cast<std::uint64_t>(_mm_movemask_epi8(vec_cmp));
            if (cmp_mask != 0) {
                i = (i << 1U) + (ffs_nonzero(cmp_mask) >> 1U);
                break;
            }
            i += 4;
        }
#else  // No SSE4.2 support
        node_ptr child_ptr;
        while (true) {
            child_ptr = children.pointer_array[i];
            if (child_ptr == nullptr)
                break;
            assert(i < 255);
            ++i;
        }
#endif // #if defined(__SSE4_2__)
        assert(children.pointer_array[i] == nullptr);
        child_indices[key_byte] = static_cast<std::uint8_t>(i);
        children.pointer_array[i] = node_ptr::create(child.release(), node_type::LEAF);
        ++this->children_count;

        return iterator(children.pointer_array[i], key_byte, this->tagged_self());
    }

    [[nodiscard]] constexpr const_iterator leftmost_child(unsigned start) noexcept
    {
        if (start < 256) {
            unsigned key_byte{start};
            while (true) {
                unsigned i = child_indices[key_byte];
                if (i != empty_child)
                    return const_iterator(children.pointer_array[i], key_byte, this->tagged_self());
                if (key_byte == 255)
                    break;
                ++key_byte;
            }
        }
        return const_iterator{};
    }

    constexpr void remove(std::uint8_t child_index) noexcept
    {
        verify_remove_preconditions(child_index);
        children.pointer_array[child_indices[child_index]] = nullptr;
        child_indices[child_index] = empty_child;
        --this->children_count;
    }

    [[nodiscard]] constexpr const_iterator find_child(std::uint8_t key_byte) noexcept
    {
        if (child_indices[static_cast<std::uint8_t>(key_byte)] != empty_child) {
            const auto child_i = child_indices[static_cast<std::uint8_t>(key_byte)];
            return const_iterator(children.pointer_array[child_i],
                                  static_cast<std::uint8_t>(key_byte), this->tagged_self());
        }
        return const_iterator{};
    }

    constexpr void replace(const_iterator pos, node_ptr child) noexcept
    {
        assert(pos.parent() == this->tagged_self());
        const auto child_i = child_indices[pos.index()];
        assert(pos.node() == children.pointer_array[child_i]);
        children.pointer_array[child_i] = child;
        this->reparent(children.pointer_array[child_i], pos.index());
    }

    constexpr void delete_subtree(Db& db_instance) noexcept
    {
        for (unsigned i = 0; i < this->capacity; ++i) {
            const auto child = children.pointer_array[i];
            if (child != nullptr) {
                db_instance.deallocate(child);
            }
        }
    }

    [[gnu::cold, gnu::noinline]] void dump(std::ostream& os) const
    {
        parent_type::dump(os);
        os << ", key bytes & child indices\n";
        for (unsigned i = 0; i < 256; i++)
            if (child_indices[i] != empty_child) {
                os << " ";
                dump_byte(os, static_cast<std::uint8_t>(i));
                os << ", child index = " << static_cast<unsigned>(child_indices[i]) << ": ";
                assert(children.pointer_array[child_indices[i]] != nullptr);
                parent_type::dump(os, children.pointer_array[child_indices[i]]);
            }
    }

private:
    constexpr void verify_remove_preconditions(std::uint8_t child_index) const noexcept
    {
        assert(child_indices[child_index] != empty_child);
        assert(children.pointer_array[child_indices[child_index]] != nullptr);
        boost::ignore_unused(child_index);
    }

    std::array<std::uint8_t, 256> child_indices;
    union children_union {
        std::array<node_ptr, basic_inode_48::capacity> pointer_array;
#if defined(__SSE2__)
        static_assert(basic_inode_48::capacity % 2 == 0, "inode_48 capacity is odd");
        // To support unrolling without remainder
        static_assert((basic_inode_48::capacity / 2) % 4 == 0,
                      "inode_48 cannot support unrolling without remainder");
        // No std::array below because it would ignore the alignment attribute
        // NOLINTNEXTLINE(modernize-avoid-c-arrays)
        __m128i pointer_vector[basic_inode_48::capacity / 2]; // NOLINT(runtime/arrays)
#endif
    } children;

    friend basic_inode_16<Db>;
    friend basic_inode_256<Db>;
};

template <typename Db>
using basic_inode_256_parent =
    basic_inode<Db, 49, 256, node_type::I256, basic_inode_48<Db>, fake_inode, basic_inode_256<Db>>;

template <typename Db> class basic_inode_256 : public basic_inode_256_parent<Db>
{
    using parent_type = basic_inode_256_parent<Db>;
    using inode48_type = typename parent_type::inode48_type;
    using inode256_type = typename parent_type::inode256_type;
    using node_ptr = typename parent_type::node_ptr;
    using leaf_unique_ptr = typename parent_type::leaf_unique_ptr;
    using const_iterator = typename Db::const_iterator;
    using iterator = typename Db::iterator;

private:
    friend Db;

    using basic_inode_256_parent<Db>::basic_inode_256_parent;

    [[nodiscard]] constexpr iterator populate(unique_node_ptr<inode48_type, Db> source_node,
                                              leaf_unique_ptr child, std::uint8_t key_byte) noexcept
    {
        assert(source_node->is_full());
        assert(this->is_min_size());

        unsigned children_copied = 0;
        unsigned i = 0;
        while (true) {
            const auto children_i = source_node->child_indices[i];
            if (children_i == empty_child) {
                children[i] = nullptr;
            } else {
                children[i] = source_node->children.pointer_array[children_i];
                this->reparent(children[i], i);
                ++children_copied;
                if (children_copied == inode48_type::capacity)
                    break;
            }
            ++i;
        }

        ++i;
        for (; i < basic_inode_256::capacity; ++i)
            children[i] = nullptr;

        assert(children[key_byte] == nullptr);
        children[key_byte] = node_ptr::create(child.release(), node_type::LEAF);

        return iterator(children[key_byte], key_byte, this->tagged_self());
    }

public:
    [[nodiscard]] constexpr iterator add(leaf_unique_ptr child, std::uint8_t key_byte) noexcept
    {
        assert(children[key_byte] == nullptr);
        children[key_byte] = node_ptr::create(child.release(), node_type::LEAF);
        ++this->children_count;

        return iterator(children[key_byte], key_byte, this->tagged_self());
    }

    constexpr void remove(std::uint8_t child_index) noexcept
    {
        assert(children[child_index] != nullptr);
        children[child_index] = nullptr;
        --this->children_count;
    }

    [[nodiscard]] constexpr const_iterator find_child(std::uint8_t key_byte) noexcept
    {
        if (children[key_byte] != nullptr)
            return const_iterator(children[key_byte], key_byte, this->tagged_self());
        return const_iterator{};
    }

    [[nodiscard]] constexpr const_iterator leftmost_child(unsigned key_byte) noexcept
    {
        for (; key_byte != children.size(); ++key_byte) {
            if (children[key_byte] != nullptr)
                return const_iterator(children[key_byte], key_byte, this->tagged_self());
        }
        return const_iterator{};
    }

    constexpr void replace(const_iterator pos, node_ptr child) noexcept
    {
        const std::uint8_t key_byte = pos.index();
        assert(pos.parent() == this->tagged_self() && pos.node() == children[key_byte]);
        children[key_byte] = child;
        this->reparent(child, key_byte);
    }

    template <typename Function>
    constexpr void for_each_child(Function func) const noexcept(noexcept(func(0, node_ptr{})))
    {
        for (unsigned i = 0; i != 256; ++i) {
            const auto child_ptr = children[i];
            if (child_ptr != nullptr) {
                func(i, child_ptr);
            }
        }
    }

    constexpr void delete_subtree(Db& db_instance) noexcept
    {
        for_each_child(
            [&db_instance](unsigned, node_ptr child) noexcept { db_instance.deallocate(child); });
    }

    [[gnu::cold, gnu::noinline]] void dump(std::ostream& os) const
    {
        parent_type::dump(os);
        os << ", key bytes & children:\n";
        for_each_child([&os](unsigned i, node_ptr child) {
            os << ' ';
            dump_byte(os, static_cast<std::uint8_t>(i));
            os << ' ';
            parent_type::dump(os, child);
        });
    }

private:
    std::array<node_ptr, basic_inode_256::capacity> children;

    friend inode48_type;
};

} // namespace detail
} // namespace art

#endif // ART_DETAIL_ART_NODES_HEADER_INCLUDED
