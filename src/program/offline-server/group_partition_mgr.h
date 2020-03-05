#pragma once

#include <vector>
#include <map>
#include <set>
#include <string>
#include <shared_mutex>

namespace bcm {

struct GroupMessageSeqInfo
{
    uint32_t timestamp;
    uint64_t lastMid;
};


class GroupPartitionMgr {
public:
    GroupPartitionMgr() {}
    virtual ~GroupPartitionMgr() {}

    static GroupPartitionMgr& Instance()
    {
        static GroupPartitionMgr g_instance;
        return g_instance;
    }

    bool updateMid(uint64_t gid, uint32_t tm, uint64_t lastMid)
    {
        std::unique_lock<std::shared_timed_mutex> l(m_groupMutex);
        if (m_groupMsgSeq.find(gid) != m_groupMsgSeq.end()) {
            m_groupMsgSeq[gid].timestamp = tm;
            m_groupMsgSeq[gid].lastMid = lastMid;
        } else {
            GroupMessageSeqInfo gm;
            gm.timestamp = tm;
            gm.lastMid = lastMid;
            m_groupMsgSeq[gid] = gm;
        }
        return true;
    }

    bool getGroupPushInfo(uint64_t gid, GroupMessageSeqInfo&  gm)
    {
        std::unique_lock<std::shared_timed_mutex> l(m_groupMutex);
        if (m_groupMsgSeq.find(gid) != m_groupMsgSeq.end()) {
            gm = m_groupMsgSeq[gid];
        } else {
            return false;
        }
        return true;
    }

    bool isExistGroup(uint64_t gid)
    {
        std::unique_lock<std::shared_timed_mutex> l(m_groupMutex);
        if (m_groupMsgSeq.find(gid) == m_groupMsgSeq.end()) {
            return false;
        }
        return true;
    }
    
private:
    mutable std::shared_timed_mutex m_groupMutex;

    std::map<uint64_t /* gid */, GroupMessageSeqInfo>  m_groupMsgSeq;
};

} // namespace bcm