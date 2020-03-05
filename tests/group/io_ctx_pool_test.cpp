#include "../test_common.h"

#include "group/io_ctx_pool.h"
#include "utils/sync_latch.h"

static const int kPoolSize = 5;

TEST_CASE("IoCtxPool")
{
    bcm::IoCtxPool pool(kPoolSize);
    for (int n = 0; n < 3; n++) {
        TLOG << "\nround " << n;
        bcm::SyncLatch s(kPoolSize + 1);
        for (int i = 0; i < kPoolSize; i++) {
            auto ioc = pool.getIoCtxByGid(i);
            ioc->post([&s, i]() {
                TLOG << "run on thread " << i;
                s.sync();
            });
        }
        s.sync();
    }
    pool.shutdown();
}