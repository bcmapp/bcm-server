#include "fnv.h"

namespace bcm {

uint32_t FNV::hash(const char *pKey, size_t ulen)
{
    register uint32_t uMagic = 16777619;
    register uint32_t uHash = 0x811C9DC5; //2166136261L;

    while (ulen--)
    {
        uHash = (uHash ^ (*(unsigned char *)pKey)) * uMagic;
        pKey++;
    }

    return uHash;
}

}; // namespace bcm