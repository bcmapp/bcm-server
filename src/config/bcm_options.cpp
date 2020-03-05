#include <exception>
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include "bcm_options.h"


namespace bcm {

namespace po = boost::program_options;

void BcmOptions::parseCmd(int argc, char* argv[])
{
    try {
        std::string configFile = "config.json";
        po::options_description cmdOpt("Usage");
        boost::program_options::variables_map opts;
        cmdOpt.add_options()
            ("help,h", "show this help/usage message")
            ("config,c", po::value<std::string>(&configFile), "path to config file");
        po::store(po::parse_command_line(argc, argv, cmdOpt), opts);
        po::notify(opts);

        if (opts.count("help") != 0 || opts.count("config") == 0) {
            std::cout << cmdOpt << std::endl;
            exit(0);
        }

        if (configFile.empty()) {
            std::cerr << "no config file:" << std::endl;
            exit(-1);
        }

        getInstance()->readConfig(configFile);
    } catch (std::exception& e) {
        std::cerr << "parse program option exception:" << e.what() << std::endl;
        exit(-1);
    }
}

void BcmOptions::readConfig(std::string configFile)
{
    try {
        std::cout << "read config file: " << configFile << std::endl;

        std::ifstream fin(configFile, std::ios::in);
        std::string content;
        char buf[4096] = {0};
        while (fin.good()) {
            fin.read(buf, 4096);
            content.append(buf, static_cast<unsigned long>(fin.gcount()));
        }

        nlohmann::json j = nlohmann::json::parse(content);
        m_config = j.get<BcmConfig>();

    } catch (std::exception& e) {
        std::cerr << "read config exception:" << e.what() << std::endl;
        exit(-1);
    }
}

} // namespace bcm
