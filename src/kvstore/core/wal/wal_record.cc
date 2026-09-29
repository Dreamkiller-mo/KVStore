#include <kvstore/core/wal/wal_record.h>
#include <cstdint>

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

bool PutWALRecord::Encode(Encoder& enc) const
{
    // fill type
    enc.PutUint8(static_cast<std::uint8_t>(WALRecordType::PUT));

    // fill key length
    enc.PutUint64(static_cast<std::uint64_t>(key.size()));

    // fill key
    enc.PutBytes(key);

    // fill value length
    enc.PutUint64(static_cast<std::uint64_t>(value.size()));

    // fill value
    enc.PutBytes(value);

    return true;
}

bool DeleteWALRecord::Encode(Encoder& enc) const
{
    // fill type
    enc.PutUint8(static_cast<std::uint8_t>(WALRecordType::DELETE));

    // fill key length
    enc.PutUint64(static_cast<std::uint64_t>(key.size()));

    // fill key
    enc.PutBytes(key);

    // fill value length
    enc.PutUint64(0);

    return true;
}