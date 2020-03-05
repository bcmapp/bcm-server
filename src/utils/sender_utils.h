#pragma once

#include <string>
#include <proto/dao/client_version.pb.h>
#include <proto/dao/device.pb.h>
#include <config/encrypt_sender.h>
#include <proto/message/message_protocol.pb.h>
#include <signal/signal_protocol.h>

namespace bcm {

class SenderUtils {

public:

    static int encryptSender(const std::string& sender,
                             const std::string& ecdhDestPubKey,
                             uint32_t& version,
                             std::string& iv,
                             std::string& ephemeralPubkey,
                             std::string& encrypted);

    static bool isClientVersionSupportEncryptSender(const Device& device,
                                                    const EncryptSenderConfig& encryptSenderConfig);

    static std::string getSourceInPushService(const Device& destinationDevice,
                                              const Envelope& envelope,
                                              const EncryptSenderConfig& encryptSenderConfig);

private:
    template <class T>
    class Guard {
    public:
        typedef void (*FunDestroy)(T*);
        Guard(T* p, FunDestroy fun) : value(p), destroy(fun)
        {

        }

        ~Guard()
        {
            if (value) {
                release();
            }
        }

        Guard() = delete;
        Guard(const Guard& p) = delete;
        Guard<T>& operator=(const Guard& p) = delete;

        T* get() { return value; }
        void reset(T* v)
        {
            if (value) {
                release();
            }
            value = v;
        }
    public:
        void release()
        {
            if (destroy) {
                destroy(value);
                value = nullptr;
            }
        }
    private:
        T* value;
        FunDestroy destroy;
    };

    static thread_local Guard<signal_context> signalContext;
};
}
