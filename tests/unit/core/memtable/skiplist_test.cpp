#include <kvstore/core/memtable/SkipList.h>

#include <cstdint>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

void Check(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

using kvstore::Key;
using kvstore::core::memtable::SkipList;

struct Record
{
    explicit Record(int value) : value(value) {}
    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;
    Record(Record&&) = default;
    Record& operator=(Record&&) = default;

    int value;
};

struct ThrowingValue
{
    explicit ThrowingValue(bool should_throw)
    {
        if (should_throw)
        {
            throw std::runtime_error("value construction failed");
        }
    }
};

void TestEmptyAndBounds()
{
    SkipList<std::string> list;
    Check(list.empty(), "new list should be empty");
    Check(list.size() == 0, "new list should have size zero");
    Check(list.begin() == list.end(), "empty begin should equal end");
    Check(list.find(Key{}) == list.end(), "empty find should return end");
    Check(!list.contains(Key{}), "empty list should not contain a key");
    Check(list.lower_bound(Key{}) == list.end(), "empty lower_bound should return end");

    const auto& const_list = list;
    Check(const_list.begin() == const_list.end(), "const empty begin should equal end");
    Check(const_list.cbegin() == const_list.cend(), "const empty range should be empty");
}

void TestInsertFindAndUpdate()
{
    SkipList<std::string> list;
    auto [first, inserted] = list.insert(Key{0x10}, "ten");
    Check(inserted, "first insert should succeed");
    Check(first->first == Key{0x10} && first->second == "ten", "inserted item should match");
    Check(list.size() == 1, "single insertion should set size to one");
    Check(list.find(Key{0x10}) != list.end(), "find should locate inserted key");
    Check(list.contains(Key{0x10}), "contains should locate inserted key");
    Check(list.find(Key{0x11}) == list.end(), "find should miss absent key");
    Check(!list.contains(Key{0x11}), "contains should miss absent key");

    auto [duplicate, inserted_again] = list.insert(Key{0x10}, "duplicate");
    Check(!inserted_again, "duplicate insert should report no insertion");
    Check(duplicate->second == "ten", "duplicate insert should preserve existing value");
    Check(list.size() == 1, "duplicate insert should not change size");

    auto [updated, inserted_update] = list.insert_or_assign(Key{0x10}, "updated");
    Check(!inserted_update, "update should report that no new key was inserted");
    Check(updated->second == "updated", "update should replace existing value");
    Check(list.size() == 1, "update should not change size");

    auto [new_item, inserted_new] = list.insert_or_assign(Key{0x20}, "twenty");
    Check(inserted_new, "insert_or_assign should insert absent key");
    Check(new_item->second == "twenty" && list.size() == 2, "new assigned value should be present");
}

void TestOrderingAndLowerBound()
{
    SkipList<std::string> list;
    list.insert(Key{0x30}, "thirty");
    list.insert(Key{0x10}, "ten");
    list.insert(Key{0x20}, "twenty");

    const std::vector<Key> expected{Key{0x10}, Key{0x20}, Key{0x30}};
    std::size_t index = 0;
    for (const auto& [key, value] : list)
    {
        Check(key == expected[index], "iteration should be in ascending key order");
        Check(!value.empty(), "iteration should preserve values");
        ++index;
    }
    Check(index == expected.size(), "iteration should visit all elements");

    Check(list.lower_bound(Key{0x00})->first == Key{0x10}, "lower_bound below minimum failed");
    Check(list.lower_bound(Key{0x20})->first == Key{0x20}, "lower_bound on existing key failed");
    Check(list.lower_bound(Key{0x15})->first == Key{0x20}, "lower_bound between keys failed");
    Check(list.lower_bound(Key{0x40}) == list.end(), "lower_bound above maximum should return end");

    const auto& const_list = list;
    SkipList<std::string>::const_iterator const_it = list.begin();
    Check(const_it == const_list.begin(), "iterator should convert to const_iterator");
    Check(const_list.find(Key{0x20})->second == "twenty", "const find failed");
}

void TestEmptyKeyValueAndReuse()
{
    SkipList<std::vector<std::uint8_t>> list;
    const Key empty_key;
    const std::vector<std::uint8_t> empty_value;

    Check(list.insert(empty_key, empty_value).second, "empty key and value should be insertable");
    Check(list.contains(empty_key), "empty key should be findable");
    Check(list.find(empty_key)->second.empty(), "empty value should be preserved");
    list.clear();
    Check(list.empty() && list.size() == 0, "clear should empty the list");
    Check(list.begin() == list.end(), "clear should reset iteration");
    Check(list.insert(Key{0x01}, std::vector<std::uint8_t>{0x02}).second,
          "list should be reusable after clear");
}

void TestMoveAndGenericValue()
{
    SkipList<Record> list;
    Check(list.try_emplace(Key{0x01}, 42).second, "try_emplace should construct an arbitrary value type");
    Check(list.find(Key{0x01})->second.value == 42, "constructed value should be stored");

    SkipList<Record> moved(std::move(list));
    Check(list.empty(), "moved-from list should be empty");
    Check(moved.find(Key{0x01})->second.value == 42, "move should transfer elements");
    Check(list.try_emplace(Key{0x02}, 7).second, "moved-from list should be reusable");

    SkipList<Record> assigned;
    assigned = std::move(moved);
    Check(moved.empty(), "move-assigned-from list should be empty");
    Check(assigned.find(Key{0x01})->second.value == 42, "move assignment should transfer elements");
}

void TestConstructionExceptionLeavesListValid()
{
    SkipList<ThrowingValue> list;
    bool threw = false;
    try
    {
        list.try_emplace(Key{0x01}, true);
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    Check(threw, "throwing value constructor should propagate its exception");
    Check(list.empty(), "failed value construction should not add an element");
    Check(list.begin() == list.end(), "failed value construction should not corrupt links");
    Check(list.try_emplace(Key{0x01}, false).second, "list should remain usable after construction failure");
}

void TestAgainstMapWithRandomKeys()
{
    SkipList<std::string> list;
    std::map<Key, std::string> reference;
    std::mt19937 generator(0x5eed);
    std::uniform_int_distribution<int> length_distribution(0, 8);
    std::uniform_int_distribution<int> byte_distribution(0, 255);

    for (int index = 0; index < 5000; ++index)
    {
        Key key(static_cast<std::size_t>(length_distribution(generator)));
        for (auto& byte : key)
        {
            byte = static_cast<std::uint8_t>(byte_distribution(generator));
        }

        const std::string value = "value-" + std::to_string(index);
        auto [position, inserted] = list.insert_or_assign(key, value);
        (void)position;
        (void)inserted;
        reference[key] = value;
    }

    Check(list.size() == reference.size(), "random workload size should match std::map");

    auto actual = list.begin();
    auto expected = reference.begin();
    for (; expected != reference.end(); ++expected, ++actual)
    {
        Check(actual != list.end(), "skip list ended before std::map");
        Check(actual->first == expected->first, "random workload key order differs from std::map");
        Check(actual->second == expected->second, "random workload value differs from std::map");
        Check(list.find(expected->first) != list.end(), "random workload key should be findable");
    }
    Check(actual == list.end(), "skip list has extra elements after std::map traversal");
}

} // namespace

int main()
{
    TestEmptyAndBounds();
    TestInsertFindAndUpdate();
    TestOrderingAndLowerBound();
    TestEmptyKeyValueAndReuse();
    TestMoveAndGenericValue();
    TestConstructionExceptionLeavesListValid();
    TestAgainstMapWithRandomKeys();
}
