#pragma once

#include <kvstore/core/memtable/SkipList.h>

#include <algorithm>
#include <random>
#include <tuple>

namespace kvstore::core::memtable
{

template <typename T>
SkipList<T>::Node::Node(std::size_t level) : forward(level, nullptr)
{
}

template <typename T>
template <typename K, typename... Args>
SkipList<T>::Node::Node(std::size_t level, K&& key, Args&&... value_args)
    : entry(std::in_place,
            std::piecewise_construct,
            std::forward_as_tuple(std::forward<K>(key)),
            std::forward_as_tuple(std::forward<Args>(value_args)...)),
      forward(level, nullptr)
{
}

template <typename T>
SkipList<T>::Iterator::Iterator(Node* node) : node_(node)
{
}

template <typename T>
typename SkipList<T>::Iterator::reference SkipList<T>::Iterator::operator*() const
{
    return *node_->entry;
}

template <typename T>
typename SkipList<T>::Iterator::pointer SkipList<T>::Iterator::operator->() const
{
    return &*node_->entry;
}

template <typename T>
typename SkipList<T>::Iterator& SkipList<T>::Iterator::operator++()
{
    node_ = node_->forward[0];
    return *this;
}

template <typename T>
typename SkipList<T>::Iterator SkipList<T>::Iterator::operator++(int)
{
    Iterator current = *this;
    ++(*this);
    return current;
}

template <typename T>
bool SkipList<T>::Iterator::operator==(const Iterator& other) const
{
    return node_ == other.node_;
}

template <typename T>
bool SkipList<T>::Iterator::operator==(const ConstIterator& other) const
{
    return node_ == other.node_;
}

template <typename T>
SkipList<T>::ConstIterator::ConstIterator(Node* node) : node_(node)
{
}

template <typename T>
SkipList<T>::ConstIterator::ConstIterator(const Iterator& other) : node_(other.node_)
{
}

template <typename T>
typename SkipList<T>::ConstIterator::reference SkipList<T>::ConstIterator::operator*() const
{
    return *node_->entry;
}

template <typename T>
typename SkipList<T>::ConstIterator::pointer SkipList<T>::ConstIterator::operator->() const
{
    return &*node_->entry;
}

template <typename T>
typename SkipList<T>::ConstIterator& SkipList<T>::ConstIterator::operator++()
{
    node_ = node_->forward[0];
    return *this;
}

template <typename T>
typename SkipList<T>::ConstIterator SkipList<T>::ConstIterator::operator++(int)
{
    ConstIterator current = *this;
    ++(*this);
    return current;
}

template <typename T>
bool SkipList<T>::ConstIterator::operator==(const ConstIterator& other) const
{
    return node_ == other.node_;
}

template <typename T>
bool SkipList<T>::ConstIterator::operator==(const Iterator& other) const
{
    return node_ == other.node_;
}

template <typename T>
SkipList<T>::SkipList() : head_(new Node(kMaxLevel))
{
}

template <typename T>
SkipList<T>::SkipList(SkipList&& other) noexcept
    : head_(std::exchange(other.head_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      max_height_(std::exchange(other.max_height_, 1))
{
}

template <typename T>
SkipList<T>& SkipList<T>::operator=(SkipList&& other) noexcept
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

template <typename T>
SkipList<T>::~SkipList()
{
    ClearNodes();
}

template <typename T>
bool SkipList<T>::empty() const noexcept
{
    return size_ == 0;
}

template <typename T>
typename SkipList<T>::size_type SkipList<T>::size() const noexcept
{
    return size_;
}

template <typename T>
typename SkipList<T>::iterator SkipList<T>::begin() noexcept
{
    return iterator(head_ == nullptr ? nullptr : head_->forward[0]);
}

template <typename T>
typename SkipList<T>::const_iterator SkipList<T>::begin() const noexcept
{
    return cbegin();
}

template <typename T>
typename SkipList<T>::const_iterator SkipList<T>::cbegin() const noexcept
{
    return const_iterator(head_ == nullptr ? nullptr : head_->forward[0]);
}

template <typename T>
typename SkipList<T>::iterator SkipList<T>::end() noexcept
{
    return iterator(nullptr);
}

template <typename T>
typename SkipList<T>::const_iterator SkipList<T>::end() const noexcept
{
    return const_iterator(nullptr);
}

template <typename T>
typename SkipList<T>::const_iterator SkipList<T>::cend() const noexcept
{
    return const_iterator(nullptr);
}

template <typename T>
void SkipList<T>::clear() noexcept
{
    if (head_ == nullptr)
    {
        return;
    }

    Node* current = head_->forward[0];
    while (current != nullptr)
    {
        Node* next = current->forward[0];
        delete current;
        current = next;
    }

    std::fill(head_->forward.begin(), head_->forward.end(), nullptr);
    size_ = 0;
    max_height_ = 1;
}

template <typename T>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::insert(const value_type& entry)
{
    return EmplaceUnique(entry.first, entry.second);
}

template <typename T>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::insert(value_type&& entry)
{
    return EmplaceUnique(entry.first, std::move(entry.second));
}

template <typename T>
template <typename U>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::insert(const Key& key, U&& value)
{
    return EmplaceUnique(key, std::forward<U>(value));
}

template <typename T>
template <typename U>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::insert(Key&& key, U&& value)
{
    return EmplaceUnique(std::move(key), std::forward<U>(value));
}

template <typename T>
template <typename... Args>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::try_emplace(
    const Key& key,
    Args&&... args)
{
    return EmplaceUnique(key, std::forward<Args>(args)...);
}

template <typename T>
template <typename... Args>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::try_emplace(
    Key&& key,
    Args&&... args)
{
    return EmplaceUnique(std::move(key), std::forward<Args>(args)...);
}

template <typename T>
template <typename U>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::insert_or_assign(
    const Key& key,
    U&& value)
{
    Node* node = FindNode(key);
    if (node != nullptr)
    {
        node->entry->second = std::forward<U>(value);
        return {iterator(node), false};
    }
    return EmplaceUnique(key, std::forward<U>(value));
}

template <typename T>
template <typename U>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::insert_or_assign(
    Key&& key,
    U&& value)
{
    Node* node = FindNode(key);
    if (node != nullptr)
    {
        node->entry->second = std::forward<U>(value);
        return {iterator(node), false};
    }
    return EmplaceUnique(std::move(key), std::forward<U>(value));
}

template <typename T>
typename SkipList<T>::iterator SkipList<T>::find(const Key& key)
{
    return iterator(FindNode(key));
}

template <typename T>
typename SkipList<T>::const_iterator SkipList<T>::find(const Key& key) const
{
    return const_iterator(FindNode(key));
}

template <typename T>
bool SkipList<T>::contains(const Key& key) const
{
    return FindNode(key) != nullptr;
}

template <typename T>
typename SkipList<T>::iterator SkipList<T>::lower_bound(const Key& key)
{
    return iterator(LowerBoundNode(key));
}

template <typename T>
typename SkipList<T>::const_iterator SkipList<T>::lower_bound(const Key& key) const
{
    return const_iterator(LowerBoundNode(key));
}

template <typename T>
template <typename K, typename... Args>
std::pair<typename SkipList<T>::iterator, bool> SkipList<T>::EmplaceUnique(
    K&& key,
    Args&&... value_args)
{
    EnsureInitialized();

    std::array<Node*, kMaxLevel> predecessors{};
    Node* next = FindGreaterOrEqual(key, &predecessors);
    if (next != nullptr && Equivalent(next->entry->first, key))
    {
        return {iterator(next), false};
    }

    const std::size_t level = RandomLevel();
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

template <typename T>
void SkipList<T>::EnsureInitialized()
{
    if (head_ == nullptr)
    {
        head_ = new Node(kMaxLevel);
    }
}

template <typename T>
bool SkipList<T>::Less(const Key& left, const Key& right)
{
    return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end());
}

template <typename T>
bool SkipList<T>::Equivalent(const Key& left, const Key& right)
{
    return !Less(left, right) && !Less(right, left);
}

template <typename T>
typename SkipList<T>::Node* SkipList<T>::FindNode(const Key& key) const
{
    Node* node = LowerBoundNode(key);
    return node != nullptr && Equivalent(node->entry->first, key) ? node : nullptr;
}

template <typename T>
typename SkipList<T>::Node* SkipList<T>::LowerBoundNode(const Key& key) const
{
    return FindGreaterOrEqual(key, nullptr);
}

template <typename T>
typename SkipList<T>::Node* SkipList<T>::FindGreaterOrEqual(
    const Key& key,
    std::array<Node*, kMaxLevel>* predecessors) const
{
    if (head_ == nullptr)
    {
        return nullptr;
    }

    Node* current = head_;
    for (std::size_t level = max_height_; level-- > 0;)
    {
        while (current->forward[level] && Less(current->forward[level]->entry->first, key))
        {
            current = current->forward[level];
        }
        if (predecessors != nullptr)
        {
            (*predecessors)[level] = current;
        }
    }

    return current->forward[0];
}

template <typename T>
std::size_t SkipList<T>::RandomLevel()
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

template <typename T>
void SkipList<T>::ClearNodes() noexcept
{
    if (head_ == nullptr)
    {
        return;
    }

    clear();
    delete head_;
    head_ = nullptr;
}

} // namespace kvstore::core::memtable
