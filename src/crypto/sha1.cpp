#include "sha1.h"
#include <boost/uuid/detail/sha1.hpp>

namespace bcm {

std::string SHA1::digest(const std::string& msg)
{
    boost::uuids::detail::sha1 sha;
    uint32_t digest[5] = {0};
    sha.process_bytes(msg.data(), msg.size());
    sha.get_digest(digest);
    for (auto i = 0; i < 5; ++i) {
        digest[i] = htobe32(digest[i]);
    }
    return std::string(reinterpret_cast<const char*>(digest), sizeof(digest));
}

} // namespace bcm
