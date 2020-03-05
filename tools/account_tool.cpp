#include <iostream>
#include <string>
#include <boost/program_options.hpp>
#include <bitcoincrypto/Base58Check.hpp>
#include <bitcoincrypto/Sha256.hpp>
#include <bitcoincrypto/Ripemd160.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <signal/curve.h>
#include <signal/signal_protocol.h>
#include <boost/uuid/detail/sha1.hpp>
#include "signal_openssl_provider.h"

namespace po = boost::program_options;
using namespace boost::beast::detail;

Sha256Hash getChallengeHash(const std::string& uid, uint32_t difficulty, uint32_t serverNonce, uint32_t clientNonce)
{
    static constexpr uint8_t prefix[] = {'B', 'C', 'M'};
    size_t dataSize = sizeof(prefix) + uid.size() + 3 * sizeof(uint32_t);
    auto data = std::make_unique<uint8_t[]>(dataSize);
    auto* pos = data.get();

    memcpy(pos, prefix, sizeof(prefix));
    pos += sizeof(prefix);

    memcpy(pos, reinterpret_cast<const void*>(uid.data()), uid.size());
    pos += uid.size();

    uint32_t swaped = htobe32(serverNonce);
    memcpy(pos, reinterpret_cast<const void*>(&swaped), sizeof(uint32_t));
    pos += sizeof(uint32_t);

    swaped = htobe32(difficulty);
    memcpy(pos, reinterpret_cast<const void*>(&swaped), sizeof(uint32_t));
    pos += sizeof(uint32_t);

    swaped = htobe32(clientNonce);
    memcpy(pos, reinterpret_cast<const void*>(&swaped), sizeof(uint32_t));

    return Sha256::getDoubleHash(data.get(), dataSize);

}

bool verifyChallenge(const std::string& uid, uint32_t difficulty, uint32_t serverNonce, uint32_t clientNonce)
{
    if (difficulty < 1) {
        return true;
    }

    if (difficulty > 32) {
        return false;
    }

    Sha256Hash hash = getChallengeHash(uid, difficulty, serverNonce, clientNonce);

    uint32_t comparing;
    memcpy(reinterpret_cast<void*>(&comparing), reinterpret_cast<const void*>(hash.value), sizeof(uint32_t));
    comparing = be32toh(comparing);

    uint32_t target = static_cast<uint32_t>(pow(2, 32 - difficulty));

    return comparing < target;
}

void doAccountChallenge(const std::string& uid, uint32_t difficulty, uint32_t serverNonce)
{
    uint32_t clientNonce = 0;
    for (uint32_t i = 0; i < UINT32_MAX; ++i) {
        if (verifyChallenge(uid, difficulty, serverNonce, i)) {
            clientNonce = i;
            break;
        }
    }

    std::cout << "Account Challenge Finish: " << std::endl;
    std::cout << "\tuid:            " << uid << std::endl;
    std::cout << "\tdifficulty:     " << difficulty << std::endl;
    std::cout << "\tserver nonce:   " << serverNonce << std::endl;
    std::cout << "\tclient nonce:   " << clientNonce << std::endl;
    std::cout << std::endl;
}

std::string publicKeyToUid(const std::string& publicKey)
{
    std::string decoded = base64_decode(publicKey);
    Sha256Hash sha256 = Sha256::getHash(reinterpret_cast<const uint8_t*>(decoded.data()), decoded.size());
    uint8_t hash160[Ripemd160::HASH_LEN];
    Ripemd160::getHash(sha256.value, (size_t) Sha256Hash::HASH_LEN, hash160);

    char uid[36];
    Base58Check::pubkeyHashToBase58Check(hash160, 0, uid);
    return std::string(uid);
}

void generateKeys()
{
    signal_context* context = nullptr;
    signal_context_create(&context, nullptr);
    signal_context_set_crypto_provider(context, &openssl_provider);

    ec_key_pair* pair = nullptr;
    curve_generate_key_pair(context, &pair);

    signal_buffer* pubkey = nullptr;
    signal_buffer* prikey = nullptr;
    ec_public_key_serialize(&pubkey, ec_key_pair_get_public(pair));
    ec_private_key_serialize(&prikey, ec_key_pair_get_private(pair));

    std::string encodedPubkeyWithDJBType = base64_encode(signal_buffer_data(pubkey), signal_buffer_len(pubkey));
    std::string encodedPubkey = base64_encode(signal_buffer_data(pubkey) + 1, signal_buffer_len(pubkey) - 1);
    std::string encodedPrikey = base64_encode(signal_buffer_data(prikey), signal_buffer_len(prikey));
    std::string uid = publicKeyToUid(encodedPubkey);

    uint64_t token = 0;
    openssl_random_generator(reinterpret_cast<uint8_t*>(&token), sizeof(token), nullptr);
    std::string authToken = std::to_string(token);
    signal_buffer* signature = nullptr;
    curve_calculate_signature(context, &signature, ec_key_pair_get_private(pair),
                              reinterpret_cast<const uint8_t*>(authToken.data()), authToken.size());
    std::string encodedSignature = base64_encode(signal_buffer_data(signature), signal_buffer_len(signature));

    std::cout << "Account Generate Finish: " << std::endl;
    std::cout << "\tuid:                    " << uid << std::endl;
    std::cout << "\tpublic key:             " << encodedPubkey << std::endl;
    std::cout << "\tpublic key(DJB):        " << encodedPubkeyWithDJBType << std::endl;
    std::cout << "\tprivate key:            " << encodedPrikey << std::endl;
    std::cout << "\tauthenticate token:     " << authToken << std::endl;
    std::cout << "\tauthenticate signature: " << encodedSignature << std::endl;
    std::cout << std::endl;
}

void calculateSignature(const std::string& text, const std::string& key)
{
    signal_context* context = nullptr;
    signal_context_create(&context, nullptr);
    signal_context_set_crypto_provider(context, &openssl_provider);

    ec_private_key* privateKey = nullptr;
    std::string decKey = base64_decode(key);
    curve_decode_private_point(&privateKey, reinterpret_cast<const uint8_t*>(decKey.data()), decKey.size(), context);

    signal_buffer* signature = nullptr;
    curve_calculate_signature(context, &signature, privateKey,
                              reinterpret_cast<const uint8_t*>(text.data()), text.size());
    std::string encodedSignature = base64_encode(signal_buffer_data(signature), signal_buffer_len(signature));

    std::cout << "Signature Calculate Finish: " << std::endl;
    std::cout << "\ttext:      " << text << std::endl;
    std::cout << "\tsignature: " << encodedSignature << std::endl;
    std::cout << std::endl;
}

void getEncodedContactToken(const std::string& number)
{
    boost::uuids::detail::sha1 sha;
    uint32_t digest[5] = {0};
    sha.process_bytes(number.data(), number.size());
    sha.get_digest(digest);

    int swaped = 0;
    char hash[20] = {0};
    for (int i = 0; i < 5; ++i) {
        swaped = htobe32(digest[i]);
        memcpy(hash + i * sizeof(uint32_t), &swaped, sizeof(uint32_t));
    }
    auto encodedToken = base64_encode(reinterpret_cast<uint8_t*>(hash), 10);

    std::cout << "Generate Contact Token Finish: " << std::endl;
    std::cout << "\tnumber:      " << number << std::endl;
    std::cout << "\ttoken:       " << encodedToken << std::endl;
    std::cout << std::endl;
}

int main(int argc, char** argv)
{
    po::options_description cmdOpt("Usage");
    boost::program_options::variables_map opts;

    bool doAccount = true;
    bool doChallenge = false;
    bool doSignature = false;
    bool doToken = false;
    std::string challengeUid;
    uint32_t challengeDifficulty = 0;
    uint32_t challengeNonce = 0;
    std::string signText;
    std::string signPrivateKey;
    std::string number;

    cmdOpt.add_options()
        ("help,h", "show this help/usage message")
        ("account,a", "generate new account (default)")
        ("challenge,c", "do account challenge")
        ("uid,u", po::value<std::string>(&challengeUid), "challenging uid")
        ("difficulty,d", po::value<uint32_t>(&challengeDifficulty), "challenging difficulty")
        ("nonce,n", po::value<uint32_t>(&challengeNonce), "challenging server nonce")
        ("sign,s", po::value<std::string>(&signText)->value_name("text"), "calculate signature for text")
        ("private,p", po::value<std::string>(&signPrivateKey)->value_name("key"), "private key for calculating signature")
        ("token,t", po::value<std::string>(&number)->value_name("number"), "generate token for number");
    po::store(po::parse_command_line(argc, argv, cmdOpt), opts);
    po::notify(opts);

    if (opts.count("help")) {
        std::cout << cmdOpt << std::endl;
        exit(0);
    }

    doChallenge = (opts.count("challenge") != 0);
    if (doChallenge) {
        if (opts.count("uid") == 0) {
            std::cerr << "need 'uid' to do challenge" << std::endl;
            exit(-1);
        }
        if (opts.count("difficulty") == 0) {
            std::cerr << "need 'difficulty' to do challenge" << std::endl;
            exit(-1);
        }
        if (opts.count("nonce") == 0) {
            std::cerr << "need 'nonce' to do challenge" << std::endl;
            exit(-1);
        }
    }

    doSignature = !signText.empty();
    if (doSignature) {
        if (signPrivateKey.empty()) {
            std::cerr << "need 'private' to calculate signature" << std::endl;
            exit(-1);
        }
    }

    doToken = !number.empty();

    if (doChallenge || doSignature || doToken) {
        doAccount = false;
    }
    if (opts.count("account") != 0) {
        doAccount = true;
    }

    if (doAccount) {
        generateKeys();
    }
    if (doChallenge) {
        doAccountChallenge(challengeUid, challengeDifficulty, challengeNonce);
    }
    if (doSignature) {
        calculateSignature(signText, signPrivateKey);
    }
    if (doToken) {
        getEncodedContactToken(number);
    }

}

