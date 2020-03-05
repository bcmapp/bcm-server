#include "../test_common.h"
#include<unistd.h>
#include "unistd.h"
#include "group/io_ctx_executor.h"
#include "utils/sync_latch.h"
#include <thread>

#include <mutex>


static const int kPoolSize = 5;

TEST_CASE("IoCtxExecutor")
{
    bcm::IoCtxExecutor executor(kPoolSize);
    for (int n = 0; n < 3; n++) {
        std::cout << "\n round " << n << std::endl;

        bcm::SyncLatch s(kPoolSize + 1);
        for (int i = 0; i < kPoolSize; i++) {
            executor.execInPool([&s, i, n]() {
                std::cout << "run on thread: " << i << ", task: " << n << std::endl;
                s.sync();
            });
        }
        s.sync();
    }

    class A {
    public:
        
        void incr(std::thread::id key)
        {
            std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexCircle);
            if (m.find(key) != m.end()) {
                m[key] += 1;
            } else {
                m[key] = 1;
            }
        }

        void print()
        {
            std::unique_lock<std::recursive_mutex> mutexRecu(m_mutexCircle);
            for (auto& it : m)
            {
                std::cout << "key: " << it.first << ", value: " << it.second << std::endl;
            }
        }
        
    private:
        std::map<std::thread::id, int32_t>  m;
        std::recursive_mutex m_mutexCircle;
    };
    
    A  a;
    
    for (int n = 0; n < 10000; n++) {
        std::cout << "\n round " << n << std::endl;
        executor.execInPool([&a, n]() {
            std::cout << "run on thread: " << std::this_thread::get_id() << ", task: " << n << std::endl;
            a.incr(std::this_thread::get_id());
            
            usleep(1000);
        });
    }
    
    sleep(10);
    std::cout << "round end" << std::endl;
    
    
    a.print();
    
    executor.stop();
}