#pragma once

#include <sys/time.h>
#include <time.h>

#include <map>
#include <string>
#include <sstream>
#include <iostream>

#include <mutex>
#include <shared_mutex>

#include "utils/log.h"

#define DEFAULTNUMBEROFREPLICAS 200

namespace bcm
{

class FnvHashFunction
{
public:
     static uint32_t hash(const char *pKey, size_t ulen)
     {
         register uint32_t uMagic = 16777619;
         register uint32_t uHash = 0x811C9DC5;//2166136261L;
    
         while(ulen--)
         {
             uHash = (uHash ^ (*(unsigned char *)pKey)) * uMagic;
             pKey++;
         }
    
         uHash += uHash << 13;
         uHash ^= uHash >> 7;
         uHash += uHash << 3;
         uHash ^= uHash >> 17;
         uHash += uHash << 5;
    
         return uHash;
     }
    
    static uint32_t hash(uint64_t i64)
    {
        return hash((const char *) &i64, sizeof(i64));
    }
};

class UserConsistentFnvHash
{
public:
    UserConsistentFnvHash(uint32_t numberOfReplicas = DEFAULTNUMBEROFREPLICAS)
    {
        this->numberOfReplicas = numberOfReplicas;
    }

    void AddServer(const std::string& strServerIpport)
    {
        char hashbuf[50] = {0};
        //memcpy(hashbuf, strServerIpport.data(), strServerIpport.length());
        snprintf(hashbuf, sizeof(hashbuf), "%s", strServerIpport.c_str());
        
        std::unique_lock<std::shared_timed_mutex> l(m_mutexCircle);
        for (uint32_t i = 0; i < numberOfReplicas; i++) 
        {
            uint32_t strLen = strServerIpport.length();
            if (strLen > sizeof(hashbuf)-sizeof(i)) {
                strLen = sizeof(hashbuf)-sizeof(i);
            }
            memcpy(hashbuf + strLen, &i, sizeof(i));
            uint32_t uValue = FnvHashFunction::hash(hashbuf, sizeof(hashbuf));

            std::map<uint32_t, std::string>::iterator iter = server_circle.find(uValue);
            if (iter != server_circle.end())
            {
                LOGW << "server circle duplicate! server:" << strServerIpport << " index:" << i;
            }
            server_circle[uValue] = strServerIpport;
        }
    }

    void RemoveServer(std::string& strServerIpport)
    {
        char hashbuf[50]={0};
        memcpy(hashbuf, strServerIpport.data(), strServerIpport.length());

        std::unique_lock<std::shared_timed_mutex> l(m_mutexCircle);
        for (uint32_t i = 0; i < numberOfReplicas; i++)
        {
            memcpy(hashbuf + strServerIpport.length(), &i, sizeof(i));
            uint32_t uValue = FnvHashFunction::hash(hashbuf, sizeof(hashbuf));
            server_circle.erase(uValue);
        }
    }

    std::string GetServer(uint64_t dwGroupid)
    {
        static const std::string kEmptyStr;
        
        std::shared_lock<std::shared_timed_mutex> l(m_mutexCircle);
        if (server_circle.empty())
        {
            return kEmptyStr;
        }

        uint32_t hash = FnvHashFunction::hash(dwGroupid);

        std::map<uint32_t, std::string>::const_iterator it = server_circle.lower_bound(hash);
        if (it == server_circle.end())
        {
            return server_circle.begin()->second;
        }

        return it->second;
    }
    
    std::string GetServer(const std::string& sHashKey)
    {
        static const std::string kEmptyStr;
        
        std::shared_lock<std::shared_timed_mutex> l(m_mutexCircle);
        if (server_circle.empty())
        {
            return kEmptyStr;
        }
        
        uint32_t hash = FnvHashFunction::hash(sHashKey.data(), sHashKey.size());
        
        std::map<uint32_t, std::string>::const_iterator it = server_circle.lower_bound(hash);
        if (it == server_circle.end())
        {
            return server_circle.begin()->second;
        }
        
        return it->second;
    }

    void Clear()
    {
        std::unique_lock<std::shared_timed_mutex> l(m_mutexCircle);
        server_circle.clear();
    }

private:
     uint32_t numberOfReplicas;
     FnvHashFunction hashFunction;
     std::shared_timed_mutex m_mutexCircle;
     std::map<uint32_t, std::string> server_circle;
};

}
