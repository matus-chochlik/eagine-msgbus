///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
#if EAGINE_MSGBUS_MODULE
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import <algorithm>;
import <chrono>;
import <cmath>;
import <cstdint>;
import <map>;
import <thread>;
import <vector>;
#else
#include <eagine/identifier_ctr.hpp>
#include <eagine/main_ctx.hpp>
#include <eagine/main_fwd.hpp>
#include <eagine/math/functions.hpp>
#include <eagine/message_bus.hpp>
#include <eagine/msgbus/service.hpp>
#include <eagine/msgbus/service/application_info.hpp>
#include <eagine/msgbus/service/discovery.hpp>
#include <eagine/msgbus/service/endpoint_info.hpp>
#include <eagine/msgbus/service/host_info.hpp>
#include <eagine/msgbus/service/ping_pong.hpp>
#include <eagine/msgbus/service_requirements.hpp>
#include <eagine/signal_switch.hpp>
#include <eagine/timeout.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>
#endif

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
struct ping_state {
    host_id_t host_id{0};
    std::string hostname;
    std::chrono::microseconds min_time{std::chrono::microseconds::max()};
    std::chrono::microseconds max_time{std::chrono::microseconds::zero()};
    std::chrono::microseconds sum_time{std::chrono::microseconds::zero()};
    std::chrono::steady_clock::time_point start{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point finish{
      std::chrono::steady_clock::now()};
    std::intmax_t sent{0};
    std::intmax_t responded{0};
    std::intmax_t timeouted{0};
    resetting_timeout should_check_info{std::chrono::seconds(5), nothing};
    bool is_active{false};

    auto avg_time() const noexcept -> std::chrono::microseconds {
        if(responded) {
            return sum_time / responded;
        }
        return {};
    }

    auto time_interval() const noexcept {
        return std::chrono::duration_cast<std::chrono::duration<float>>(
          finish - start);
    }

    auto total_count() const noexcept {
        return float(responded) + float(timeouted);
    }

    auto respond_rate() const noexcept {
        return math::ratio(float(responded), total_count());
    }

    auto responds_per_second() const noexcept {
        return math::ratio(float(responded), time_interval().count());
    }
};
//------------------------------------------------------------------------------
using pinger_base = require_services<
  subscriber,
  pinger,
  host_info_consumer,
  host_info_provider,
  application_info_provider,
  endpoint_info_provider,
  subscriber_discovery>;

class pinger_node : public service_node<pinger_base> {
    using base = service_node<pinger_base>;

public:
    pinger_node(
      main_ctx_parent parent,
      const valid_if_positive<std::intmax_t>& max,
      const valid_if_positive<std::intmax_t>& limit)
      : base{"MsgBusPing", parent}
      , _limit{extract_or(limit, 1000)}
      , _max{extract_or(max, 100000)} {
        this->object_description("Pinger", "Message bus ping");

        bus_node().id_assigned.connect(
          make_callable_ref<&pinger_node::on_id_assigned>(this));
        bus_node().connection_lost.connect(
          make_callable_ref<&pinger_node::on_connection_lost>(this));
        bus_node().connection_established.connect(
          make_callable_ref<&pinger_node::on_connection_established>(this));

        subscribed.connect(
          make_callable_ref<&pinger_node::on_subscribed>(this));
        unsubscribed.connect(
          make_callable_ref<&pinger_node::on_unsubscribed>(this));
        not_subscribed.connect(
          make_callable_ref<&pinger_node::on_not_subscribed>(this));
        ping_responded.connect(
          make_callable_ref<&pinger_node::on_ping_response>(this));
        ping_timeouted.connect(
          make_callable_ref<&pinger_node::on_ping_timeout>(this));
        host_id_received.connect(
          make_callable_ref<&pinger_node::on_host_id_received>(this));
        hostname_received.connect(
          make_callable_ref<&pinger_node::on_hostname_received>(this));

        auto& info = provided_endpoint_info();
        info.display_name = "pinger";
        info.description = "node pinging all other nodes";

        setup_connectors(main_context(), *this);
    }

    void on_id_assigned(const identifier_t endpoint_id) noexcept {
        log_info("new id ${id} assigned").arg("id", endpoint_id);
        _can_ping = true;
    }

    void on_connection_established(const bool usable) noexcept {
        log_info("connection established");
        _can_ping = usable;
    }

    void on_connection_lost() noexcept {
        log_info("connection lost");
        _can_ping = false;
    }

    void on_subscribed(
      const subscriber_info& info,
      const message_id sub_msg) noexcept {
        if(sub_msg == this->ping_msg_id()) {
            auto& stats = _targets[info.endpoint_id];
            if(!stats.is_active) {
                stats.is_active = true;
                log_info("new pingable ${id} appeared")
                  .arg("id", info.endpoint_id);
            }
        }
    }

    void on_unsubscribed(
      const subscriber_info& info,
      const message_id sub_msg) noexcept {
        if(sub_msg == this->ping_msg_id()) {
            auto& state = _targets[info.endpoint_id];
            if(state.is_active) {
                state.is_active = false;
                log_info("pingable ${id} disappeared")
                  .arg("id", info.endpoint_id);
            }
        }
    }

    void on_not_subscribed(
      const subscriber_info& info,
      const message_id sub_msg) noexcept {
        if(sub_msg == this->ping_msg_id()) {
            auto& state = _targets[info.endpoint_id];
            state.is_active = false;
            log_info("target ${id} is not pingable").arg("id", info.endpoint_id);
        }
    }

    void on_host_id_received(
      const result_context& res_ctx,
      const valid_if_positive<host_id_t>& host_id) noexcept {
        if(host_id) {
            if(res_ctx.source_id() != this->bus_node().get_id()) {
                auto& state = _targets[res_ctx.source_id()];
                state.host_id = extract(host_id);
            }
        }
    }

    void on_hostname_received(
      const result_context& res_ctx,
      const valid_if_not_empty<std::string>& hostname) noexcept {
        if(hostname) {
            auto& state = _targets[res_ctx.source_id()];
            state.hostname = extract(hostname);
        }
    }

    void on_ping_response(
      const identifier_t pinger_id,
      const message_sequence_t,
      const std::chrono::microseconds age,
      const verification_bits) noexcept {
        auto& state = _targets[pinger_id];
        state.responded++;
        state.min_time = std::min(state.min_time, age);
        state.max_time = std::max(state.max_time, age);
        state.sum_time += age;
        state.finish = std::chrono::steady_clock::now();
        if((++_rcvd % _mod) == 0) [[unlikely]] {
            const auto now{std::chrono::steady_clock::now()};
            const std::chrono::duration<float> interval{now - prev_log};

            if(interval > decltype(interval)::zero()) [[likely]] {
                const auto msgs_per_sec{float(_mod) / interval.count()};

                log_chart_sample("msgsPerSec", msgs_per_sec);
                log_info("received ${rcvd} pongs")
                  .arg("rcvd", _rcvd)
                  .arg("interval", interval)
                  .arg("msgsPerSec", msgs_per_sec)
                  .arg(
                    "done",
                    "Progress",
                    0.F,
                    static_cast<float>(_rcvd),
                    static_cast<float>(_max));
            }
            prev_log = now;
        }
    }

    void on_ping_timeout(
      const identifier_t pinger_id,
      const message_sequence_t,
      const std::chrono::microseconds) noexcept {
        auto& state = _targets[pinger_id];
        state.timeouted++;
        if((++_tout % _mod) == 0) [[unlikely]] {
            log_info("${tout} pongs expired").arg("tout", _tout);
        }
    }

    auto is_done() const noexcept -> bool {
        return !(((_rcvd + _tout + _mod) < _max) || this->has_pending_pings());
    }

    auto do_ping() -> work_done {
        some_true something_done{};
        if(_should_query_pingable) [[unlikely]] {
            log_info("searching for pingable nodes");
            query_pingables();
        }
        if(!_targets.empty()) {
            for(auto& [pingable_id, entry] : _targets) {
                if(_rcvd < _max) {
                    if(entry.is_active) {
                        const auto balance =
                          entry.sent - entry.responded - entry.timeouted;
                        const auto limit =
                          _limit / span_size(_targets.size() + 1);
                        if(balance <= limit) {
                            this->ping(pingable_id, std::chrono::seconds(15));
                            entry.sent++;
                            if((++_sent % _mod) == 0) [[unlikely]] {
                                log_info("sent ${sent} pings")
                                  .arg("sent", _sent);
                            }

                            if(entry.should_check_info) [[unlikely]] {
                                if(!entry.host_id) {
                                    this->query_host_id(pingable_id);
                                }
                                if(entry.hostname.empty()) {
                                    this->query_hostname(pingable_id);
                                }
                            }
                            something_done();
                        }
                    }
                } else {
                    break;
                }
            }
        }
        return something_done;
    }

    auto update() -> work_done {
        some_true something_done{base::update()};
        if(_can_ping) [[likely]] {
            something_done(do_ping());
        }
        something_done(base::process_all() > 0);
        return something_done;
    }

    void log_stats() {
        const string_view not_avail{"N/A"};
        for(auto& [id, info] : _targets) {

            log_stat("pingable ${id} stats:")
              .arg("id", id)
              .arg("hostId", info.host_id)
              .arg("hostname", info.hostname)
              .arg("minTime", info.min_time)
              .arg("maxTime", info.max_time)
              .arg("avgTime", info.avg_time())
              .arg("responded", info.responded)
              .arg("timeouted", info.timeouted)
              .arg("duration", info.time_interval())
              .arg("rspdRate", "Ratio", info.respond_rate(), not_avail)
              .arg(
                "rspdPerSec",
                "RatePerSec",
                info.responds_per_second(),
                not_avail);
        }
    }

private:
    resetting_timeout _should_query_pingable{
      adjusted_duration(std::chrono::seconds{3}),
      nothing};
    std::chrono::steady_clock::time_point prev_log{
      std::chrono::steady_clock::now()};
    std::map<identifier_t, ping_state> _targets{};
    std::intmax_t _limit{1000};
    std::intmax_t _mod{10000};
    std::intmax_t _max{100000};
    std::intmax_t _sent{0};
    std::intmax_t _rcvd{0};
    std::intmax_t _tout{0};
    bool _can_ping{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    const signal_switch interrupted;
    enable_message_bus(ctx);
    ctx.preinitialize();

    valid_if_positive<std::intmax_t> ping_count{};
    if(const auto arg{ctx.args().find("--ping-count")}) {
        arg.next().parse(ping_count, ctx.log().error_stream());
    }

    valid_if_positive<std::intmax_t> limit_count{};
    if(const auto arg{ctx.args().find("--limit-count")}) {
        arg.next().parse(limit_count, ctx.log().error_stream());
    }

    msgbus::pinger_node the_pinger{ctx, ping_count, limit_count};

    resetting_timeout do_chart_stats{std::chrono::seconds{15}, nothing};

    while(!the_pinger.is_done() || interrupted) {
        the_pinger.process_all();
        if(!the_pinger.update()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
            if(do_chart_stats) {
                the_pinger.log_chart_sample(
                  "shortLoad", ctx.system().short_average_load());
                the_pinger.log_chart_sample(
                  "longLoad", ctx.system().long_average_load());
                if(const auto temp_k{ctx.system().cpu_temperature()}) {
                    the_pinger.log_chart_sample(
                      "cpuTempC", extract(temp_k).to<units::degree_celsius>());
                }
            }
        }
    }
    the_pinger.log_stats();

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "PingExe";
    return eagine::main_impl(argc, argv, options, &eagine::main);
}
