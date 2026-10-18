#pragma once

#include <kvstore/core/memtable/SkipList.h>

#include <algorithm>
#include <random>
#include <tuple>

namespace kvstore::core::memtable
{

template <typename Key, typename Value>
SkipList<Key, Value>::Node::Node(std::size_t level) : forward(level, nullptr)
{}

template <typename Key, typename Value>
template <typename K, typename... Args>
SkipList<Key, Value>::Node::Node(std::size_t level, K&& key, Args&&... value_args)
    // Use std::piecewise_construct to construct the entry in place
    : entry(std::in_place,
            std::piecewise_construct,
            std::forward_as_tuple(std::forward<K>(key)),
            std::forward_as_tuple(std::forward<Args>(value_args)...)),
      forward(level, nullptr)
{}

template <typename Key, typename Value>
SkipList<Key, Value>::Iterator::Iterator(Node* node) : node_(node)
{}

template <typename Key, typename Value>
typename SkipList<Key, Value>::Iterator::reference SkipList<Key, Value>::Iterator::operator*() const
{
    return *(node_->entry); // *optional<Entry> to Entry&
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::Iterator::pointer SkipList<Key, Value>::Iterator::operator->() const
{
    return &*(node_->entry);
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::Iterator& SkipList<Key, Value>::Iterator::operator++()
{
    node_ = node_->forward[0];
    return *this;
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::Iterator SkipList<Key, Value>::Iterator::operator++(int)
{
    Iterator current = *this;
    ++(*this);
    return current;
}

template <typename Key, typename Value>
bool SkipList<Key, Value>::Iterator::operator==(const Iterator& other) const
{
    return node_ == other.node_;
}

template <typename Key, typename Value>
bool SkipList<Key, Value>::Iterator::operator==(const ConstIterator& other) const
{
    return node_ == other.node_;
}

template <typename Key, typename Value>
SkipList<Key, Value>::ConstIterator::ConstIterator(Node* node) : node_(node)
{}

template <typename Key, typename Value>
SkipList<Key, Value>::ConstIterator::ConstIterator(const Iterator& other) : node_(other.node_)
{}

template <typename Key, typename Value>
typename SkipList<Key, Value>::ConstIterator::reference SkipList<Key, Value>::ConstIterator::operator*() const
{
    return *(node_->entry);
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::ConstIterator::pointer SkipList<Key, Value>::ConstIterator::operator->() const
{
    return &*node_->entry;
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::ConstIterator& SkipList<Key, Value>::ConstIterator::operator++()
{
    node_ = node_->forward[0];
    return *this;
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::ConstIterator SkipList<Key, Value>::ConstIterator::operator++(int)
{
    ConstIterator current = *this;
    ++(*this);
    return current;
}

template <typename Key, typename Value>
bool SkipList<Key, Value>::ConstIterator::operator==(const ConstIterator& other) const
{
    return node_ == other.node_;
}

template <typename Key, typename Value>
bool SkipList<Key, Value>::ConstIterator::operator==(const Iterator& other) const
{
    return node_ == other.node_;
}

template <typename Key, typename Value>
SkipList<Key, Value>::SkipList() : head_(new Node(kMaxLevel))
{}

template <typename Key, typename Value>
SkipList<Key, Value>::SkipList(SkipList&& other) noexcept
    // equal
    /*
        SkipList<Key, Value>::SkipList(SkipList&& other) noexcept
        : head_(other.head_),
        size_(other.size_),
        max_height_(other.max_height_)
        {
            other.head_ = nullptr;
            other.size_ = 0;
            other.max_height_ = 1;
        }
    */

    // std::exchange : 1.  get old value 2. assign new value 3. return old value

    : head_(std::exchange(other.head_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      max_height_(std::exchange(other.max_height_, 1))
{}

template <typename Key, typename Value>
SkipList<Key, Value>& SkipList<Key, Value>::operator=(SkipList&& other) noexcept
{
    if (this != &other)
    {
        ClearNodes();
        head_ = std::exchange(other.head_, nullptr);
        size_ = std::exchange(other.size_, 0);
        max_height_ = std::exchange(other.max_height_, 1);
    }
    return *this;
}

template <typename Key, typename Value>
SkipList<Key, Value>::~SkipList()
{
    ClearNodes();
}

template <typename Key, typename Value>
bool SkipList<Key, Value>::empty() const noexcept
{
    return size_ == 0;
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::size_type SkipList<Key, Value>::size() const noexcept
{
    return size_;
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::iterator SkipList<Key, Value>::begin() noexcept
{
    return iterator(head_ == nullptr ? nullptr : head_->forward[0]);
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::const_iterator SkipList<Key, Value>::begin() const noexcept
{
    return cbegin();
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::const_iterator SkipList<Key, Value>::cbegin() const noexcept
{
    return const_iterator(head_ == nullptr ? nullptr : head_->forward[0]);
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::iterator SkipList<Key, Value>::end() noexcept
{
    return iterator(nullptr);
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::const_iterator SkipList<Key, Value>::end() const noexcept
{
    return const_iterator(nullptr);
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::const_iterator SkipList<Key, Value>::cend() const noexcept
{
    return const_iterator(nullptr);
}

template <typename Key, typename Value>
void SkipList<Key, Value>::clear() noexcept
{
    if (head_ == nullptr) { return; }

    Node* current = head_->forward[0];
    while (current != nullptr)
    {
        Node* next = current->forward[0];
        delete current;
        current = next;
    }

    // clear head_ forward pointers and reset size and max_height
    std::fill(head_->forward.begin(), head_->forward.end(), nullptr);
    size_ = 0;
    max_height_ = 1;
}

template <typename Key, typename Value>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::insert(const value_type& entry)
{
    return EmplaceUnique(entry.first, entry.second);
}

template <typename Key, typename Value>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::insert(value_type&& entry)
{
    return EmplaceUnique(entry.first, std::move(entry.second));
}

template <typename Key, typename Value>
template <typename U>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::insert(const Key& key, U&& value)
{
    return EmplaceUnique(key, std::forward<U>(value));
}

template <typename Key, typename Value>
template <typename U>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::insert(Key&& key, U&& value)
{
    return EmplaceUnique(std::move(key), std::forward<U>(value));
}

template <typename Key, typename Value>
template <typename... Args>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::try_emplace(const Key& key, Args&&... args)
{
    return EmplaceUnique(key, std::forward<Args>(args)...);
}

template <typename Key, typename Value>
template <typename... Args>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::try_emplace(Key&& key, Args&&... args)
{
    return EmplaceUnique(std::move(key), std::forward<Args>(args)...);
}

template <typename Key, typename Value>
template <typename U>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::insert_or_assign(const Key& key, U&& value)
{
    Node* node = FindNode(key);
    if (node != nullptr)
    {
        node->entry->second = std::forward<U>(value);
        return {iterator(node), false};
    }

    return EmplaceUnique(key, std::forward<U>(value));
}

template <typename Key, typename Value>
template <typename U>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::insert_or_assign(Key&& key, U&& value)
{
    Node* node = FindNode(key);
    if (node != nullptr)
    {
        node->entry->second = std::forward<U>(value);
        return {iterator(node), false};
    }
    return EmplaceUnique(std::move(key), std::forward<U>(value));
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::iterator SkipList<Key, Value>::find(const Key& key)
{
    return iterator(FindNode(key));
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::const_iterator SkipList<Key, Value>::find(const Key& key) const
{
    return const_iterator(FindNode(key));
}

template <typename Key, typename Value>
bool SkipList<Key, Value>::contains(const Key& key) const
{
    return FindNode(key) != nullptr;
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::iterator SkipList<Key, Value>::lower_bound(const Key& key)
{
    return iterator(LowerBoundNode(key));
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::const_iterator SkipList<Key, Value>::lower_bound(const Key& key) const
{
    return const_iterator(LowerBoundNode(key));
}

template <typename Key, typename Value>
template <typename K, typename... Args>
std::pair<typename SkipList<Key, Value>::iterator, bool> SkipList<Key, Value>::EmplaceUnique(K&& key, Args&&... value_args)
{
    EnsureInitialized();

    std::array<Node*, kMaxLevel> predecessors{};

    // get the next node that is greater than or equal to the key and fill the predecessors array
    Node* next = FindGreaterOrEqual(key, &predecessors);

    if (next != nullptr && Equivalent(next->entry->first, key))
    {
        return {iterator(next), false};
    }

    const std::size_t level = RandomLevel();

    // Starting from the current highest effective layer, fill the predecessors array with head_ for any levels that are not yet used
    for (std::size_t index = max_height_; index < level; ++index)
    {
        predecessors[index] = head_;
    }

    Node* node = new Node(level, std::forward<K>(key), std::forward<Args>(value_args)...);
    for (std::size_t index = 0; index < level; ++index)
    {
        node->forward[index] = predecessors[index]->forward[index];
        predecessors[index]->forward[index] = node;
    }

    if (level > max_height_)
    {
        max_height_ = level;
    }

    ++size_;
    return {iterator(node), true};
}

template <typename Key, typename Value>
void SkipList<Key, Value>::EnsureInitialized()
{
    if (head_ == nullptr)
    {
        head_ = new Node(kMaxLevel);
    }
}

template <typename Key, typename Value>
bool SkipList<Key, Value>::Less(const Key& left, const Key& right)
{
    return left < right;
}

template <typename Key, typename Value>
bool SkipList<Key, Value>::Equivalent(const Key& left, const Key& right)
{
    return !Less(left, right) && !Less(right, left);
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::Node* SkipList<Key, Value>::FindNode(const Key& key) const
{
    Node* node = LowerBoundNode(key);
    return node != nullptr && Equivalent(node->entry->first, key) ? node : nullptr;
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::Node* SkipList<Key, Value>::LowerBoundNode(const Key& key) const
{
    return FindGreaterOrEqual(key, nullptr);
}

template <typename Key, typename Value>
typename SkipList<Key, Value>::Node* SkipList<Key, Value>::FindGreaterOrEqual(const Key& key, std::array<Node*, kMaxLevel>* predecessors) const
{
    if (head_ == nullptr)
    {
        return nullptr;
    }

    Node* current = head_;
    for (std::size_t level = max_height_; level > 0; --level)
    {
        const std::size_t index = level - 1;
        while (current->forward[index] && Less(current->forward[index]->entry->first, key))
        {
            current = current->forward[index];
        }

        if (predecessors != nullptr)
        {
            (*predecessors)[index] = current;
        }
    }

    return current->forward[0];
}

template <typename Key, typename Value>
std::size_t SkipList<Key, Value>::RandomLevel()
{
    thread_local std::mt19937 generator(std::random_device{}());
    thread_local std::bernoulli_distribution promote(0.25);

    std::size_t level = 1;
    while (level < kMaxLevel && promote(generator))
    {
        ++level;
    }
    return level;
}

template <typename Key, typename Value>
void SkipList<Key, Value>::ClearNodes() noexcept
{
    if (head_ == nullptr){ return; }

    // delete all nodes except head_ (set head_ forward pointers to nullptr)
    clear();
    delete head_;
    head_ = nullptr;
}

} // namespace kvstore::core::memtable
