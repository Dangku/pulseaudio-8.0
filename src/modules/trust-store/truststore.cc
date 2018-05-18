#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <memory>
#include <core/dbus/bus.h>
#include <core/dbus/asio/executor.h>
#include <core/trust/dbus_agent.h>
#include <core/trust/agent.h>

#include <pulse/cdecl.h>

PA_C_DECL_BEGIN
#include <pulsecore/core-util.h>
#include <pulse/xmalloc.h>
#include <pulsecore/log.h>
#include <pulsecore/thread.h>

#include "truststore.h"
PA_C_DECL_END

class TrustStore {
public:
    std::shared_ptr<core::trust::Agent> agent;
    std::shared_ptr<core::dbus::Bus> bus;
    pa_thread *thread;
};

static void thread_func(void *data) {
    class TrustStore *ts = (class TrustStore *) data;

    ts->bus->run();
}

pa_trust_store* pa_trust_store_new() {
    try {
        auto bus = std::make_shared<core::dbus::Bus>(core::dbus::WellKnownBus::session);
        bus->install_executor(core::dbus::asio::make_executor(bus));

        auto agent = core::trust::dbus::create_multi_user_agent_for_bus_connection(bus, "PulseAudio");
        auto ts = new TrustStore();
        ts->agent = agent;
        ts->bus = bus;
        ts->thread = pa_thread_new("trust-store-bus", thread_func, ts);

        return (pa_trust_store *) ts;
    } catch(const std::exception &e) {
        pa_log_error("Could not create Ubuntu touch trust store connection: %s",
            e.what());
    } catch(...) {
        pa_log_error("Could not create Ubuntu touch trust store connection");
        return NULL;
    }
}

void pa_trust_store_free(pa_trust_store *t) {
    pa_assert(t != NULL);
    auto ts = (TrustStore*) t;
    if (ts->thread) {
        ts->bus->stop();
        pa_thread_free(ts->thread);
    }
    delete ts;
}

bool pa_trust_store_check(pa_trust_store *t, const char *appname,
                          uid_t uid, pid_t pid, const char *description) {
    auto ts = (TrustStore*) t;
    try {

        core::trust::Agent::RequestParameters params {
            core::trust::Uid{uid},
            core::trust::Pid{pid},
            appname,
            core::trust::Feature{0},
            description
        };
        pa_log_debug("Asking Ubuntu touch trust store for permission (app: %s)",
            params.application.id.c_str());
        auto answer = ts->agent->authenticate_request_with_parameters(params);
        if (answer == core::trust::Request::Answer::granted) {
            pa_log_debug("Request granted.");
            return true;
        }
        pa_log_info("Request denied.");
    } catch(const std::exception &e) {
        pa_log_error("Could not ask Ubuntu touch trust store for permission: %s",
            e.what());
    } catch(...) {
        pa_log_error("Could not ask Ubuntu touch trust store for permission");
    }
    return false;
}
