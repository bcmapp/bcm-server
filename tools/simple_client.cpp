#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/program_options.hpp>

using namespace boost;
using tcp = boost::asio::ip::tcp;
namespace ssl = boost::asio::ssl;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;
namespace po = boost::program_options;

void loadCertificate(ssl::context& ctx)
{

    const std::string cert =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIDeDCCAmACCQCtE2zPkXgN3TANBgkqhkiG9w0BAQsFADCBgjELMAkGA1UEBhMC\n"
        "Q04xCzAJBgNVBAgMAmdkMQswCQYDVQQHDAJHWjELMAkGA1UECgwCWVkxFTATBgNV\n"
        "BAsMDHd3dy50ZXN0LmNvbTEVMBMGA1UEAwwMd3d3LnRlc3QuY29tMR4wHAYJKoZI\n"
        "hvcNAQkBFg93dWdhb2p1bkB5eS5jb20wHhcNMTgwOTA2MDkxMDM3WhcNMjgwOTAz\n"
        "MDkxMDM3WjB5MQswCQYDVQQGEwJDTjELMAkGA1UECAwCR0QxCzAJBgNVBAcMAkda\n"
        "MQswCQYDVQQKDAJZWTEMMAoGA1UECwwDTEFCMRUwEwYDVQQDDAx3d3cudGVzdC5j\n"
        "b20xHjAcBgkqhkiG9w0BCQEWD3d1Z2FvanVuQHl5LmNvbTCCASIwDQYJKoZIhvcN\n"
        "AQEBBQADggEPADCCAQoCggEBALyFx3HXUsVvYSOpRQapbJUrx+L08BXfyxIJaElt\n"
        "JzhpUzNf1B7FU/FGteKtisZu4abZYT9neeJs7DfR4ytO8z8fe7GF7VmyOkJLgCWP\n"
        "RJvwqHG2Ex5jyk1EvmdWNM/HELJRCCV9z4hm9PC6neyzaX3A3h/exPdGxcmJFvnm\n"
        "KY/QZNG2xf1OLQbdeqGrHTNmdFZY94ljEakFzjLTgYbmmELAPUtp9eosQUqz4F8i\n"
        "Txn2aFtXKYiDxCcFIAEfa/W5nyAtiOizl1cCLrubABxjdx24YvQw+DRknplWwJyf\n"
        "esLgXCuBonmKnasgwNHXEpwDrdxtDt2MmAmQ1T/FGIIN8SkCAwEAATANBgkqhkiG\n"
        "9w0BAQsFAAOCAQEAWseF8XRNkMoRwysPbK7tYYxeMFNd8T3IpTZmiN5bNqYQ4GXh\n"
        "XKXy04nk9eV5uLw5IUboJgBF9NrxjmyNVVWTLcDopLVFTMMHZud3SzUGbcOIT9wl\n"
        "I1wVaYBRCxgUerqx1+Lg0TA3fPFU5oBuIPP+cTqvEzxDdqtZ+0Xeu3w3ZvbgYY79\n"
        "xTBwbU9DcAQF5JrB6kGC9J/SIXP+5rRhJDaXB3AdJWino6kWMyvavhW+0/97paHR\n"
        "Dhle9aLk70zMSgrZw/0rPQ1QUjEm2CbjA6qE3TujKS9YGWZVo/Qf5kMXxGNs/+Ls\n"
        "vqQnapGDcX6F08NN8HZUitpGl4rjKKJTFIn3WA==\n"
        "-----END CERTIFICATE-----\n";

    system::error_code ec;
    ctx.add_certificate_authority(
        boost::asio::buffer(cert.data(), cert.size()), ec);
    if(ec) {
        std::cerr << "Load Certificate Error: " << ec.message() << std::endl;
        exit(-1);
    }
}

int main(int argc, char** argv)
{
    po::options_description cmdOpt("Usage");
    boost::program_options::variables_map opts;

    std::string host;
    std::string port;
    std::string uri;
    std::string uid;
    std::string token;
    std::string deviceId;
    std::string requestId;

    cmdOpt.add_options()
        ("help", "show this help/usage message")
        ("host,h", po::value<std::string>(&host), "service host")
        ("port,p", po::value<std::string>(&port), "service port")
        ("requestId,r", po::value<std::string>(&requestId), "request id")
        ("uri,i", po::value<std::string>(&uri), "service uri")
        ("uid,u", po::value<std::string>(&uid), "login uid")
        ("token,t", po::value<std::string>(&token), "login token")
        ("device,d", po::value<std::string>(&deviceId), "login device");
    po::store(po::parse_command_line(argc, argv, cmdOpt), opts);
    po::notify(opts);

    bool bHelp = false;

    bool isDeviceRequest = false;

    if (!requestId.empty()) {
        isDeviceRequest = true;
    }

    if (host.empty()) {
        bHelp = true;
        std::cerr << "need host to connect" << std::endl;
    }
    if (port.empty()) {
        bHelp = true;
        std::cerr << "need port to connect" << std::endl;
    }
    if (uri.empty()) {
        bHelp = true;
        std::cerr << "need uri to connect" << std::endl;
    }
    if (uid.empty() && !isDeviceRequest) {
        bHelp = true;
        std::cerr << "need uid to login" << std::endl;
    }
    if (token.empty() && !isDeviceRequest ) {
        bHelp = true;
        std::cerr << "need token to login" << std::endl;
    }
    if (deviceId.empty()) {
        deviceId = "1";
    }

    if (opts.count("help") || bHelp) {
        std::cout << cmdOpt << std::endl;
        exit(0);
    }

    std::string target;
    if (isDeviceRequest) {
        target = uri + "?requestId=" + requestId;
    } else {
        target = uri + "?login=" + uid + "&password=" + token;
    }

    try
    {
        boost::asio::io_context ioc;
        ssl::context ctx{ssl::context::sslv23_client};
        loadCertificate(ctx);

        tcp::resolver resolver{ioc};
        websocket::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};

        auto const results = resolver.resolve(host, port);
        boost::asio::connect(ws.next_layer().next_layer(), results.begin(), results.end());
        ws.next_layer().handshake(ssl::stream_base::client);

        http::response<http::string_body> res;
        ws.handshake(res, host, target);

        boost::beast::multi_buffer b;
        for(;;) {
            system::error_code ec;
            ws.read(b, ec);
            if (ec) {
                std::cerr << "Error: " << ec.message() << std::endl;
                break;
            }
            std::cout << "read: " << boost::beast::buffers(b.data()) << std::endl;
        }

    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
