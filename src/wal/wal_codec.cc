#include "../../include/kvstore/core/wal/wal_codec.h"

#include "../../include/kvstore/core/wal/encoder.h"
#include "../../include/kvstore/core/wal/wal_record.h"
#include "../../include/kvstore/core/wal/crc32.h"

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

    // inite Encoder
    Encoder encoder(buffer);

    // Reserved Total length ---> uint64_t
    const auto length_pos = encoder.Size();
    encoder.PutUint64(0);

    const auto payload_begin = encoder.Size();

    // fill sequence ---> uint64_t
    encoder.PutUint64(record.sequnce());

    // fill payload ---> uint8_t
    const auto& operation = record.operation();
    operation.Visit([&]<EncodeType T>(const T& type)
    {
        auto res = type.Encode(encoder);
        // res ? 
    });

    // Length of payload
    const auto payload_end = encoder.Size();
    const auto payload_len = static_cast<uint64_t>(payload_end - payload_begin);

    //Total Length 
    const auto TotalLength = static_cast<uint64_t>(payload_len + sizeof(uint32_t));

    // fill CRC check [sequence][operation]
    auto payload = encoder.View(static_cast<size_t>(payload_begin), static_cast<size_t>(payload_end));
    auto res = CRC32::Compute(payload);

    
}

