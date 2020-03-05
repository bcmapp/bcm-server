#include "bloom/bloom_filters.h"
#include "crypto/base64.h"
#include <iostream>
#include <string>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    po::options_description cmdOpt("Usage");
    po::variables_map opts;
    std::string content;
    std::vector<std::string> ids;
    std::vector<std::string> tids;
    uint32_t length = 0;
    uint32_t algo = 0;
    std::vector<uint32_t> positons;
    std::vector<bool> values;


    cmdOpt.add_options()
        ("help,h", "show this help/usage message")
        ("length,l", po::value<uint32_t>(&length), "length of filters(required)")
        ("algo,a", po::value<uint32_t>(&algo), "algo number of filters(required)")
        ("content,c", po::value<std::string>(&content), "bloom filters content to init with(base64 encoded)(optional)")
        ("idList,i", po::value<std::vector<std::string>>(&ids)->multitoken(), "id list(muti)(optional)")
        ("testIdList,t", po::value<std::vector<std::string>>(&tids)->multitoken(), "to test id list(muti)(optional)")
        ("positons,u", po::value<std::vector<uint32_t>>(&positons)->multitoken(), "positons to update(muti)(optional)")
        ("values,v", po::value<std::vector<bool>>(&values)->multitoken(), "values corresponding to positions(muti)(optional)")
        ("print,p", "print bloom filters in base64_encode(optional)");

    po::store(po::parse_command_line(argc, argv, cmdOpt), opts);
    po::notify(opts);

    if (opts.count("help")) {
        std::cout << cmdOpt << std::endl;
        exit(0);
    }

    if (!opts.count("length") || !opts.count("algo")) {
        std::cout << "require length and algo" << std::endl;
        exit(-1);
    }

    bcm::BloomFilters filters(algo, length);

    if (opts.count("content")) {
        std::cout << "content: " << content << std::endl;
        bool ret = filters.fromFiltersContent(bcm::Base64::decode(content));
        if (!ret) {
            std::cout << "parse error, content length not equals filter length:"
                 << length << ": " << bcm::Base64::decode(content).length()*8 << std::endl;
            exit(-1);
        }
    }

    if (opts.count("idList")) {
        for (const auto& id : ids) {
            filters.insert(id);
            std::cout << "insert id: " << id << std::endl;
        }
    }

    if (opts.count("positons") && opts.count("values")) {
        if (positons.size() != values.size()) {
            std::cout << "positons length not equals values length:"
                 << positons.size() << ": " << values.size() << std::endl;
        }
        std::map<uint32_t, bool> upData;
        for (auto i = 0u; i < positons.size(); ++i) {
            upData[positons[i]] = values[i];
        }
        filters.update(upData);
    }

    if (opts.count("testIdList")) {
        for (const auto& id : tids) {
            std::cout << "test if filters contains id: " << id << ": " << filters.contains(id) << std::endl;
        }
    }

    if (opts.count("print")) {
        std::cout << "filters content: " << bcm::Base64::encode(filters.getFiltersContent()) << std::endl;
    }
    return 0;
}

