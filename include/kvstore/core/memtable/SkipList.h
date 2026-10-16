#pragma once

#include <array>
#include <cstddef>
#include <iterator>
#include <kvstore/common/types.h>
#include <optional>
#include <utility>
#include <vector>

namespace kvstore::core::memtable
{

template <typename T>
class SkipList
{
private:
    static constexpr std::size_t kMaxLevel = 16;
    using Entry = std::pair<const Key, T>;      // const Key to prevent external modification of the key

    struct Node
    {
        std::optional<Entry> entry;
        std::vector<Node*> forward;

        explicit Node(std::size_t level);

        template <typename K, typename... Args>
        Node(std::size_t level, K&& key, Args&&... value_args);
    };

public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = Entry;
    using size_type = std::size_t;

    class ConstIterator;

    class Iterator : public std::iterator<std::forward_iterator_tag, Entry>
    {
        friend class SkipList;
        friend class ConstIterator;

        explicit Iterator(Node* node);

    public:
        using iterator_base = std::iterator<std::forward_iterator_tag, Entry>;
        using iterator_category = iterator_base::iterator_category;
        using iterator_concept = iterator_base::iterator_concept;
        using difference_type = iterator_base::difference_type;
        using value_type = iterator_base::value_type;
        using reference = iterator_base::reference;
        using pointer = iterator_base::pointer;

        Iterator() = default;

        reference operator*() const;
        pointer operator->() const;
        Iterator& operator++();
        Iterator operator++(int);
        bool operator==(const Iterator& other) const;
        bool operator==(const ConstIterator& other) const;

    private:
        Node* node_ = nullptr;
    };

    class ConstIterator
    {
        friend class SkipList;
        friend class Iterator;

        explicit ConstIterator(Node* node);

    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Entry;
        using reference = const Entry&;
        using pointer = const Entry*;

        ConstIterator() = default;
        ConstIterator(const Iterator& other);

        reference operator*() const;
        pointer operator->() const;
        ConstIterator& operator++();
        ConstIterator operator++(int);
        bool operator==(const ConstIterator& other) const;
        bool operator==(const Iterator& other) const;

    private:
        Node* node_ = nullptr;
    };

    using iterator = Iterator;
    using const_iterator = ConstIterator;

    SkipList();
    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;
    SkipList(SkipList&& other) noexcept;
    SkipList& operator=(SkipList&& other) noexcept;
    ~SkipList();

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] size_type size() const noexcept;

    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    const_iterator cbegin() const noexcept;
    iterator end() noexcept;
    const_iterator end() const noexcept;
    const_iterator cend() const noexcept;

    void clear() noexcept;

    std::pair<iterator, bool> insert(const value_type& entry);
    std::pair<iterator, bool> insert(value_type&& entry);

    template <typename U>
    std::pair<iterator, bool> insert(const Key& key, U&& value);

    template <typename U>
    std::pair<iterator, bool> insert(Key&& key, U&& value);

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const Key& key, Args&&... args);

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(Key&& key, Args&&... args);

    template <typename U>
    std::pair<iterator, bool> insert_or_assign(const Key& key, U&& value);

    template <typename U>
    std::pair<iterator, bool> insert_or_assign(Key&& key, U&& value);
    iterator find(const Key& key);
    const_iterator find(const Key& key) const;
    [[nodiscard]] bool contains(const Key& key) const;
    iterator lower_bound(const Key& key);
    const_iterator lower_bound(const Key& key) const;

private:
    template <typename K, typename... Args>
    std::pair<iterator, bool> EmplaceUnique(K&& key, Args&&... value_args);
    void EnsureInitialized();
    [[nodiscard]] static bool Less(const Key& left, const Key& right);
    [[nodiscard]] static bool Equivalent(const Key& left, const Key& right);
    Node* FindNode(const Key& key) const;
    Node* LowerBoundNode(const Key& key) const;
    Node* FindGreaterOrEqual(const Key& key, std::array<Node*, kMaxLevel>* predecessors) const;
    [[nodiscard]] static std::size_t RandomLevel();
    void ClearNodes() noexcept;

    Node* head_ = nullptr;
    size_type size_ = 0;
    std::size_t max_height_ = 1;
};

} // namespace kvstore::core::memtable

#include <kvstore/core/memtable/SkipList.tpp>
