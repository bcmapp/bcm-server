#ifndef NDEBUG

#pragma once
#include <map>
#include "http/http_router.h"

namespace bcm {

class MagicStatusCodeFilter : public HttpRouter::Filter {
public:
    MagicStatusCodeFilter() {}
    virtual ~MagicStatusCodeFilter() {}
    bool onFilter(const http::request<http::string_body>& header, uint32_t& status, std::string& reason) override;

private:
    std::map<std::string, unsigned> m_Codes;
};

}

#endif