#include <kvstore/core/wal/wal_codec.h>
#include <kvstore/core/wal/encoder.h>
#include <kvstore/core/wal/wal_record.h>
#include <kvstore/core/wal/crc32c.h>
#include <spdlog/spdlog.h>

using namespace kvstore::core::wal;

/*

+----------------+
| Total Length   |
+----------------+
| Sequence       |
+----------------+
| Type           |
+----------------+
| Key Length     |
+----------------+
| Key            |
+----------------+
| Value Length   |
+----------------+
| Value          |
+----------------+
| Checksum       |
+----------------+


*/

void WALCodec::Encode(const WALRecord& record, std::vector<std::uint8_t>& buffer) const
{
    const auto start = buffer.size();
    // inite Encoder
    Encoder encoder(buffer);

    // Reserved Total length ---> uint64_t
    const auto length_pos = encoder.Size();
    encoder.PutUint64(0);

    const auto payload_begin = encoder.Size();

    // fill sequence ---> uint64_t
    encoder.PutUint64(record.sequnce());

    // fill payload ---> uint8_t
    bool success = true;
    const auto& operation = record.operation();
    operation.Visit([&]<EncodeType T>(const T& type)
    {
        success = type.Encode(encoder);
    });

    if(!success)
    {
        // rollback
        buffer.resize(start);
    }

    // Length of payload
    const auto payload_end = encoder.Size();
    const auto payload_len = static_cast<uint64_t>(payload_end - payload_begin);

    //Total Length ---> uint64_t
    const auto total_length = static_cast<uint64_t>(payload_len + sizeof(uint32_t));

    // fill CRC check [sequence][operation] ---> uint32_t
    auto payload = encoder.View(static_cast<size_t>(payload_begin), static_cast<size_t>(payload_end));
    auto Checksum = CRC32C::Compute(payload);
    encoder.PutUint32(Checksum);

    // backfill Total Length
    encoder.PutUint64At(length_pos, total_length);
    
    spdlog::info("WAL Encode sequence={} bytes={}",record.sequnce(),buffer.size());
}

