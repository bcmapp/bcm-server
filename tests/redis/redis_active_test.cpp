#include "../test_common.h"
#include <unordered_map>
#include <shared_mutex>
#include "fiber/fiber_timer.h"
#include "utils/time.h"
#include <unistd.h>


using namespace bcm;

class MockRedisDbManager {
public:
    MockRedisDbManager(){};

    static MockRedisDbManager* Instance()
    {
        static MockRedisDbManager gs_instance;
        return &gs_instance;
    }

    void setRedisDbConfig(std::string p00, std::string p01, std::string p02, std::string p10, std::string p11, std::string p12)
    {
        std::vector<std::string> partition0, partition1;
        partition0.push_back(p00);  // "partition0_redis0"
        partition0.push_back(p01);  // "partition0_redis1"
        partition0.push_back(p02);  // "partition0_redis2"
        partition1.push_back(p10);  // "partition1_redis0"
        partition1.push_back(p11);  // "partition1_redis1"
        partition1.push_back(p12);  // "partition1_redis2"
        m_redisPartitions.push_back(partition0);
        m_redisPartitions.push_back(partition1);
        // m_currPartitionConn[0]=0;
        // m_currPartitionConn[1]=0;
    }

    bool hset(uint64_t gid)
    {
        int pIndex = getPartitionByGroup(gid);
        if (pIndex == -1) {
            return false;
        }
    
        int rIndex = 0;
        {
            std::shared_lock<std::shared_timed_mutex> l(m_currPartitionConnMutex);
            auto itr = m_currPartitionConn.find(pIndex);
            if (itr != m_currPartitionConn.end()) {
                rIndex = itr->second;
            }
        }
    

        std::string ptrRedisServer;

        for (unsigned int i = 0; i < m_redisPartitions[pIndex].size(); i++ ){
            if (i!= 0) {
                std::unique_lock<std::shared_timed_mutex> l(m_currPartitionConnMutex);
                m_currPartitionConn[pIndex] = rIndex;
            }
            ptrRedisServer = m_redisPartitions[pIndex][rIndex++];
            if (unsigned(rIndex) == m_redisPartitions[pIndex].size()){
                rIndex = 0;
            }
            if (ptrRedisServer == "") {
                std::cerr << "[REDIS MANAGER HSET] failed to get available redis connection." << std::endl;
                continue;
            }
            bool isSuccess = true;
            if (ptrRedisServer == "error") {
                isSuccess = false;
            }else {
                std::cout << ptrRedisServer << ": hset success " << std::endl;
            }
            if (isSuccess) {
                return true;
            }
        }

        return false;
    }

    void run() {
        std::string ptrRedisServer;
        for (auto& p : m_currPartitionConn) {
            for (int i = 0; i <= p.second; i++) {
                ptrRedisServer = m_redisPartitions[p.first][i];

                if (ptrRedisServer == "") {
                    std::cerr << "redis manager run failed to get available redis connection: " << ptrRedisServer << std::endl;
                    continue;
                }

                bool isSuccess = true;
                if (ptrRedisServer == "error") {
                    isSuccess = false;
                }else {
                    std::cout << ptrRedisServer << ": set group_redis_active 15 " << std::endl;
                }
                if (isSuccess && i < p.second) {
                    std::unique_lock<std::shared_timed_mutex> l(m_currPartitionConnMutex);
                    m_currPartitionConn[p.first] = i;
                    std::cout << "switch to " << m_redisPartitions[p.first][i] << std::endl;
                    break;
                }
            }
        }
    }

    std::unordered_map<int, int >& getCurrPartition() {
        return m_currPartitionConn;
    }

    void setRedisServer(int pIndex, int rIndex, std::string serverName) {
        m_redisPartitions[pIndex][rIndex] = serverName;
    }

private:
    int getPartitionByGroup(uint64_t gid) {
        if (m_redisPartitions.empty()){
            return -1;
        }
        return gid % m_redisPartitions.size();
    }

private:
    std::vector<std::vector<std::string> > m_redisPartitions;
    std::unordered_map<int, int > m_currPartitionConn;
    std::shared_timed_mutex m_currPartitionConnMutex;

};

class MackGroupRedisDbActive : public FiberTimer::Task {
public:
    MackGroupRedisDbActive(MockRedisDbManager* redisDbManager)
    :m_redisDbManager(redisDbManager)
    {
    }

    static constexpr int groupToRedisDbCheckInterval = 5000; // ms


private:
    void run() {
        int64_t start = nowInMilli();

        m_redisDbManager->run();

        m_execTime = nowInMilli() - start;
    }
    void cancel() {

    }
    int64_t lastExecTimeInMilli() {
        return m_execTime;
    }

private:
    MockRedisDbManager* m_redisDbManager;
    int64_t m_execTime;
    
};



TEST_CASE("group_redis_active")
{
    MockRedisDbManager::Instance()->setRedisDbConfig("partition0_redis0","partition0_redis1","partition0_redis2",
                                                     "partition1_redis0","partition1_redis1","partition1_redis2");

    auto groupRedisDbActive = std::make_shared<MackGroupRedisDbActive>(MockRedisDbManager::Instance());
    auto fiberTimer = std::make_shared<bcm::FiberTimer>();
    fiberTimer->schedule(groupRedisDbActive, MackGroupRedisDbActive::groupToRedisDbCheckInterval, true);

    // 正常
    MockRedisDbManager::Instance()->hset(1);
    MockRedisDbManager::Instance()->hset(2);

    // partition0_redis0 connect error
    sleep(2);
    MockRedisDbManager::Instance()->setRedisServer(0,0,"");
    MockRedisDbManager::Instance()->hset(1);
    MockRedisDbManager::Instance()->hset(2);
    // 检查 pIndex rIndex
    auto currMap = MockRedisDbManager::Instance()->getCurrPartition();
    REQUIRE(currMap[0] == 1);
    REQUIRE(currMap[1] == 0);

    // set group_msg_active partition0_redis1
    sleep(5);
    currMap = MockRedisDbManager::Instance()->getCurrPartition();
    REQUIRE(currMap[0] == 1);
    REQUIRE(currMap[1] == 0);


    // partition0_redis0 connect 恢复
    MockRedisDbManager::Instance()->setRedisServer(0,0,"partition0_redis0");

    // 检查 pIndex rIndex 是否恢复
    sleep(5);
    currMap = MockRedisDbManager::Instance()->getCurrPartition();
    REQUIRE(currMap[0] == 0);
    REQUIRE(currMap[1] == 0);

    MockRedisDbManager::Instance()->hset(1);
    MockRedisDbManager::Instance()->hset(2);

    return;
}