///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
#include <eagine/identifier_ctr.hpp>
#include <eagine/main_ctx.hpp>
#include <eagine/main_fwd.hpp>
#include <eagine/math/functions.hpp>
#include <eagine/message_bus.hpp>
#include <eagine/message_bus/service.hpp>
#include <eagine/message_bus/service/application_info.hpp>
#include <eagine/message_bus/service/discovery.hpp>
#include <eagine/message_bus/service/endpoint_info.hpp>
#include <eagine/message_bus/service/host_info.hpp>
#include <eagine/message_bus/service/ping_pong.hpp>
#include <eagine/message_bus/service_requirements.hpp>
#include <eagine/signal_switch.hpp>
#include <eagine/timeout.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

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
      : base{EAGINE_ID(MsgBusPing), parent}
      , _limit{extract_or(limit, 1000)}
      , _max{extract_or(max, 100000)} {
        this->object_description("Pinger", "Message bus ping");

        bus_node().id_assigned.connect(
          EAGINE_THIS_MEM_FUNC_REF(on_id_assigned));
        bus_node().connection_lost.connect(
          EAGINE_THIS_MEM_FUNC_REF(on_connection_lost));
        bus_node().connection_established.connect(
          EAGINE_THIS_MEM_FUNC_REF(on_connection_established));

        subscribed.connect(EAGINE_THIS_MEM_FUNC_REF(on_subscribed));
        unsubscribed.connect(EAGINE_THIS_MEM_FUNC_REF(on_unsubscribed));
        not_subscribed.connect(EAGINE_THIS_MEM_FUNC_REF(on_not_subscribed));
        ping_responded.connect(EAGINE_THIS_MEM_FUNC_REF(on_ping_response));
        ping_timeouted.connect(EAGINE_THIS_MEM_FUNC_REF(on_ping_timeout));
        host_id_received.connect(EAGINE_THIS_MEM_FUNC_REF(on_host_id_received));
        hostname_received.connect(
          EAGINE_THIS_MEM_FUNC_REF(on_hostname_received));

        auto& info = provided_endpoint_info();
        info.display_name = "pinger";
        info.description = "node pinging all other nodes";

        bus().setup_connectors(*this);
    }

    void on_id_assigned(identifier_t endpoint_id) {
        log_info("new id ${id} assigned").arg(EAGINE_ID(id), endpoint_id);
        _can_ping = true;
    }

    void on_connection_established(bool usable) {
        log_info("connection established");
        _can_ping = usable;
    }

    void on_connection_lost() {
        log_info("connection lost");
        _can_ping = false;
    }

    void on_subscribed(const subscriber_info& info, message_id sub_msg) {
        if(sub_msg == this->ping_msg_id()) {
            auto& stats = _targets[info.endpoint_id];
            if(!stats.is_active) {
                stats.is_active = true;
                log_info("new pingable ${id} appeared")
                  .arg(EAGINE_ID(id), info.endpoint_id);
            }
        }
    }

    void on_unsubscribed(const subscriber_info& info, message_id sub_msg) {
        if(sub_msg == this->ping_msg_id()) {
            auto& state = _targets[info.endpoint_id];
            if(state.is_active) {
                state.is_active = false;
                log_info("pingable ${id} disappeared")
                  .arg(EAGINE_ID(id), info.endpoint_id);
            }
        }
    }

    void on_not_subscribed(const subscriber_info& info, message_id sub_msg) {
        if(sub_msg == this->ping_msg_id()) {
            auto& state = _targets[info.endpoint_id];
            state.is_active = false;
            log_info("target ${id} is not pingable")
              .arg(EAGINE_ID(id), info.endpoint_id);
        }
    }

    void on_host_id_received(
      const result_context& res_ctx,
      const valid_if_positive<host_id_t>& host_id) {
        if(host_id) {
            if(host_id != this->bus_node().get_id()) {
                auto& state = _targets[res_ctx.source_id()];
                state.host_id = extract(host_id);
            }
        }
    }

    void on_hostname_received(
      const result_context& res_ctx,
      const valid_if_not_empty<std::string>& hostname) {
        if(hostname) {
            auto& state = _targets[res_ctx.source_id()];
            state.hostname = extract(hostname);
        }
    }

    void on_ping_response(
      identifier_t pinger_id,
      message_sequence_t,
      std::chrono::microseconds age,
      verification_bits) {
        auto& state = _targets[pinger_id];
        state.responded++;
        state.min_time = std::min(state.min_time, age);
        state.max_time = std::max(state.max_time, age);
        state.sum_time += age;
        state.finish = std::chrono::steady_clock::now();
        if(EAGINE_UNLIKELY((++_rcvd % _mod) == 0)) {
            const auto now{std::chrono::steady_clock::now()};
            const std::chrono::duration<float> interval{now - prev_log};

            if(EAGINE_LIKELY(interval > decltype(interval)::zero())) {
                const auto msgs_per_sec{float(_mod) / interval.count()};

                log_chart_sample(EAGINE_ID(msgsPerSec), msgs_per_sec);
                log_info("received ${rcvd} pongs")
                  .arg(EAGINE_ID(rcvd), _rcvd)
                  .arg(EAGINE_ID(interval), interval)
                  .arg(EAGINE_ID(msgsPerSec), msgs_per_sec)
                  .arg(EAGINE_ID(done), EAGINE_ID(Progress), 0, _rcvd, _max);
            }
            prev_log = now;
        }
    }

    void on_ping_timeout(
      identifier_t pinger_id,
      message_sequence_t,
      std::chrono::microseconds) {
        auto& state = _targets[pinger_id];
        state.timeouted++;
        if(EAGINE_UNLIKELY((++_tout % _mod) == 0)) {
            log_info("${tout} pongs expired").arg(EAGINE_ID(tout), _tout);
        }
    }

    auto is_done() const noexcept -> bool {
        return !(((_rcvd + _tout + _mod) < _max) || this->has_pending_pings());
    }

    auto do_ping() -> work_done {
        some_true something_done{};
        if(EAGINE_UNLIKELY(_should_query_pingable)) {
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
                            if(EAGINE_UNLIKELY((++_sent % _mod) == 0)) {
                                log_info("sent ${sent} pings")
                                  .arg(EAGINE_ID(sent), _sent);
                            }

                            if(EAGINE_UNLIKELY(entry.should_check_info)) {
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
        if(EAGINE_LIKELY(_can_ping)) {
            something_done(do_ping());
        }
        something_done(base::process_all() > 0);
        return something_done;
    }

    void log_stats() {
        const string_view not_avail{"N/A"};
        for(auto& [id, info] : _targets) {

            log_stat("pingable ${id} stats:")
              .arg(EAGINE_ID(id), id)
              .arg(EAGINE_ID(hostId), info.host_id)
              .arg(EAGINE_ID(hostname), info.hostname)
              .arg(EAGINE_ID(minTime), info.min_time)
              .arg(EAGINE_ID(maxTime), info.max_time)
              .arg(EAGINE_ID(avgTime), info.avg_time())
              .arg(EAGINE_ID(responded), info.responded)
              .arg(EAGINE_ID(timeouted), info.timeouted)
              .arg(EAGINE_ID(duration), info.time_interval())
              .arg(
                EAGINE_ID(rspdRate),
                EAGINE_ID(Ratio),
                info.respond_rate(),
                not_avail)
              .arg(
                EAGINE_ID(rspdPerSec),
                EAGINE_ID(RatePerSec),
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
    signal_switch interrupted;
    enable_message_bus(ctx);
    ctx.preinitialize();

    valid_if_positive<std::intmax_t> ping_count{};
    if(auto arg{ctx.args().find("--ping-count")}) {
        arg.next().parse(ping_count, ctx.log().error_stream());
    }

    valid_if_positive<std::intmax_t> limit_count{};
    if(auto arg{ctx.args().find("--limit-count")}) {
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
                  EAGINE_ID(shortLoad), ctx.system().short_average_load());
                the_pinger.log_chart_sample(
                  EAGINE_ID(longLoad), ctx.system().long_average_load());
                if(auto temp_k{ctx.system().cpu_temperature()}) {
                    the_pinger.log_chart_sample(
                      EAGINE_ID(cpuTempC),
                      extract(temp_k).to<units::degree_celsius>());
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
    options.app_id = EAGINE_ID(PingExe);
    return eagine::main_impl(argc, argv, options);
}
