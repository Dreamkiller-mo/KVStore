#pragma once

#include "../../common/types.h"
#include "encoder.h"
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>

namespace kvstore::core::wal
{

class Operation;
// operation type
enum class WALRecordType : std::uint8_t
{
    PUT,
    DELETE
};

struct PutWALRecord
{
    WALRecordType type = WALRecordType::PUT;

    Key key;

    Value value;

    [[nodiscard]] bool Encode(Encoder& enc) const;

};

struct DeleteWALRecord
{
    WALRecordType type = WALRecordType::DELETE;

    Key key;

    [[nodiscard]] bool Encode(Encoder& enc) const;

};


// Operation的能力约束
template<typename T>
concept WALOperation = requires(T record)
{
    requires std::same_as<
        std::remove_cvref_t<decltype(record.type)>,
        WALRecordType
    >;

    requires std::same_as<
        std::remove_cvref_t<decltype(record.key)>,
        Key
    >;
};

// encapsulate (封装) std::variant
class Operation
{
public:
    using Payload = std::variant<PutWALRecord, DeleteWALRecord>;
    
    // explicit Operation(PutWALRecord record) : payload_(std::move(record)){}
    // explicit Operation(DeleteWALRecord record) : payload_(std::move(record)){}
    template<typename T>
    requires WALOperation<T>
    Operation(T&& arg) : payload_(std::forward<T>(arg)) {}

    // bool IsPut() const { return std::holds_alternative<PutWALRecord>(payload_); }
    // bool IsDelete() const { return std::holds_alternative<DeleteWALRecord>(payload_);}
    template <typename T>
    [[nodiscard]] bool Is() const { return std::holds_alternative<T>(payload_); }

    // const PutWALRecord& AsPut() const { return std::get<PutWALRecord>(payload_); }
    // const DeleteWALRecord& AsDelete() const { return std::get<DeleteWALRecord>(payload_); }

    template <typename T>
    [[nodiscard]] const T& As() const { return std::get<T>(payload_); }

    // adapt std::visit
    template <typename Visitor>
    [[nodiscard]] decltype(auto) Visit(Visitor&& visitor) const { return std::visit(std::forward<Visitor>(visitor), payload_); }
    
private:
    Payload payload_;    // 装载的容器
};

class WALRecord
{
public:
    WALRecord(Sequence sequence, Operation operation);

    [[nodiscard]] kvstore::Sequence sequnce() const { return sequence_; }

    [[nodiscard]] const Operation& operation() const { return operation_; }

private:
    // operation number
    kvstore::Sequence sequence_;
    
    // operation type
    Operation operation_;

};

}   //namespace kvstore::core::wal