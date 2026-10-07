#include <kvstore/api/kvstore.h>
#include <kvstore/composition/composition_root.h>
#include <kvstore/composition/config.h>
#include <spdlog/spdlog.h>

int main()
{
    kvstore::composition::Composition composition;

    auto result = composition.Create(kvstore::Config{});
    if (!result) {
        return -1;
    }

    auto& kv = *result;

    for( int i = 0; i < 10; ++i)
    {
        auto status = kv.Put("Dreamkiller", "hello kvstore");
        if (!status.ok()) { spdlog::error("Put failed, status={}", status.Error()); }
    }

    return 0;
}