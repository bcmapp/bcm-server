#include "../include/concurrent_queue.h"
#include "test_common.h"
#include <vector>
#include <string>
#include <thread>
#include <unistd.h>
#include <atomic>
#include <iostream>

using namespace bcm::metrics;

TEST_CASE("testOneThreadWithoutSize")
//void testOneThreadWithoutSize()
{
    ConcurrentQueue<int> q;
    bool enqResult = q.tryEnqueue(1);
    REQUIRE(enqResult == true);
    q.tryEnqueue(2);
    q.enqueue(3);
    int result;
    REQUIRE(q.tryPop(result) == true);
    REQUIRE(result == 1);
    REQUIRE(q.tryPop(result) == true);
    REQUIRE(result == 2);
    REQUIRE(q.tryPop(result) == true);
    REQUIRE(result == 3);
    REQUIRE(q.tryPop(result) == false);

    ConcurrentQueue<std::string> qStr;
    std::string a = "aaa";
    qStr.tryEnqueue(a);
    qStr.tryEnqueue("bbb");
    std::string c = "ccc";
    qStr.tryEnqueue(std::move(c));
    std::string d = "ddd";
    qStr.enqueue(d);
    std::string e = "eee";
    qStr.enqueue(e);

    std::string resultStr;
    REQUIRE(qStr.tryPop(resultStr) == true);
    REQUIRE(resultStr == "aaa");
    REQUIRE(qStr.tryPop(resultStr) == true);
    REQUIRE(resultStr == "bbb");
    REQUIRE(qStr.tryPop(resultStr) == true);
    REQUIRE(resultStr == "ccc");
    REQUIRE(qStr.tryPop(resultStr) == true);
    REQUIRE(resultStr == "ddd");
    REQUIRE(qStr.tryPop(resultStr) == true);
    REQUIRE(resultStr == "eee");
    REQUIRE(qStr.tryPop(resultStr) == false);
}

TEST_CASE("testOneThreadWithSize")
//void testOneThreadWithSize()
{
    ConcurrentQueue<std::string> q(2);
    q.tryEnqueue("aaa");
    q.tryEnqueue("bbb");
    bool enqResult = q.tryEnqueue("ccc");
    REQUIRE(enqResult == false);
    q.enqueue("ddd");
    std::string resultStr;
    REQUIRE(q.tryPop(resultStr) == true);
    REQUIRE(resultStr == "aaa");
    REQUIRE(q.tryPop(resultStr) == true);
    REQUIRE(resultStr == "bbb");
    REQUIRE(q.tryPop(resultStr) == true);
    REQUIRE(resultStr == "ddd");
    REQUIRE(q.tryPop(resultStr) == false);
}

TEST_CASE("testMultiThread")
//void testMultiThread()
{
    std::atomic<uint64_t> received(0);
    std::vector<std::thread> senderThreads;
    std::vector<std::thread> receiveThreads;
    ConcurrentQueue<std::string> q(1000000);

    for(int i = 0; i < 5; ++i){
        senderThreads.push_back(std::thread([&]() {
            std::string index = std::to_string(i);
            for (int j=0;j<10000;j++) {
                q.tryEnqueue(index + std::to_string(j));
            }
        }));
    }
    for(auto& t : senderThreads) {
        t.detach();
    }

    for(int i = 0; i < 2; ++i){
        std::string result;
        receiveThreads.push_back(std::thread([&]() {
            while(true) {
                q.blockingPop(result);
                received.fetch_add(1);
            }
        }));
    }

    for(auto& t : receiveThreads) {
        t.detach();
    }

    sleep(10);
    REQUIRE(received.load() == 50000);
}

//TEST_CASE("loadTest")
void loadTest()
{
    std::atomic<uint64_t> received(0);
    std::vector<std::thread> senderThreads;
    std::vector<std::thread> receiveThreads;
    ConcurrentQueue<std::string> q(1000000);
    bool running = true;

    for(int i = 0; i < 5; ++i){
        senderThreads.push_back(std::thread([&]() {
            std::string index = std::to_string(i);
            uint64_t j = 0;
            while(running){
                q.tryEnqueue(index + std::to_string(++j));
            }
        }));
    }
    for(auto& t : senderThreads) {
        t.detach();
    }

    for(int i = 0; i < 5; ++i){
        std::string result;
        receiveThreads.push_back(std::thread([&]() {
            while(running) {
                q.blockingPop(result);
                received.fetch_add(1);
            }
        }));
    }

    for(auto& t : receiveThreads) {
        t.detach();
    }

    sleep(1200);
    running = false;
    for(auto& t : receiveThreads) {
        pthread_cancel(t.native_handle());
    }
    sleep(2);
    std::cout << "total: " << received.load() << " tps:" << (received.load() / 1200) << std::endl;
}
