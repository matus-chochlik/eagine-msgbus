/// @example eagine/msgbus/007_ping.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.sslplus;
import eagine.msgbus;
import std;

namespace eagine {
namespace msgbus {
//------------------------------------------------------------------------------
struct ping_stats {
    host_id_t host_id{0};
    std::string hostname;
    std::chrono::microseconds min_time{std::chrono::microseconds::max()};
    std::chrono::microseconds max_time{std::chrono::microseconds::zero()};
    std::chrono::microseconds sum_time{std::chrono::microseconds::zero()};
    std::chrono::steady_clock::time_point start{
      std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point finish{
      std::chrono::steady_clock::now()};
    std::intmax_t responded{0};
    std::intmax_t timeouted{0};
    resetting_timeout should_check_info{std::chrono::seconds(5), nothing};

    auto avg_time() const noexcept {
        return sum_time / responded;
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
using ping_base = service_composition<require_services<
  subscriber,
  pinger,
  host_info_consumer,
  subscriber_discovery,
  shutdown_invoker>>;

class ping_example
  : public main_ctx_object
  , public ping_base {
    using base = ping_base;

public:
    ping_example(
      endpoint& bus,
      const valid_if_positive<std::intmax_t>& rep,
      const valid_if_positive<std::intmax_t>& mod,
      const valid_if_positive<std::intmax_t>& max)
      : main_ctx_object{"PingExampl", bus}
      , base{bus}
      , _rep{rep.value_or(1)}
      , _mod{mod.value_or(10000)}
      , _max{max.value_or(100000)} {
        object_description("Pinger", "Ping example");

        connect<&ping_example::on_id_assigned>(this, bus.id_assigned);
        connect<&ping_example::on_connection_lost>(this, bus.connection_lost);
        connect<&ping_example::on_connection_established>(
          this, bus.connection_established);

        connect<&ping_example::on_subscribed>(this, subscribed);
        connect<&ping_example::on_unsubscribed>(this, unsubscribed);
        connect<&ping_example::on_not_subscribed>(this, not_subscribed);
        connect<&ping_example::on_ping_response>(this, ping_responded);
        connect<&ping_example::on_ping_timeout>(this, ping_timeouted);
        connect<&ping_example::on_host_id_received>(this, host_id_received);
        connect<&ping_example::on_hostname_received>(this, hostname_received);
    }

    void on_id_assigned(const identifier_t endpoint_id) noexcept {
        log_info("new id ${id} assigned").arg("id", endpoint_id);
        _do_ping = true;
    }

    void on_connection_established(const bool usable) noexcept {
        log_info("connection established").tag("newConn");
        _do_ping = usable;
    }

    void on_connection_lost() noexcept {
        log_info("connection lost").tag("connLost");
        _do_ping = false;
    }

    void on_subscribed(
      const result_context&,
      const subscriber_subscribed& sub) noexcept {
        if(sub.message_type == this->ping_msg_id()) {
            if(_targets.try_emplace(sub.source.endpoint_id, ping_stats{})
                 .second) {
                log_info("new pingable ${id} appeared")
                  .tag("newPngable")
                  .arg("id", sub.source.endpoint_id);
            }
        }
    }

    void on_unsubscribed(
      const result_context&,
      const subscriber_unsubscribed& sub) noexcept {
        if(sub.message_type == this->ping_msg_id()) {
            log_info("pingable ${id} disappeared")
              .arg("id", sub.source.endpoint_id);
        }
    }

    void on_not_subscribed(
      const result_context&,
      const subscriber_not_subscribed& sub) noexcept {
        if(sub.message_type == this->ping_msg_id()) {
            log_info("target ${id} is not pingable")
              .arg("id", sub.source.endpoint_id);
        }
    }

    void on_host_id_received(
      const result_context& res_ctx,
      const valid_if_positive<host_id_t>& host_id) noexcept {
        host_id.and_then(_1.assign_to(_targets[res_ctx.source_id()].host_id));
    }

    void on_hostname_received(
      const result_context& res_ctx,
      const valid_if_not_empty<std::string>& hostname) noexcept {
        hostname.and_then(_1.assign_to(_targets[res_ctx.source_id()].hostname));
    }

    void on_ping_response(
      const result_context&,
      const ping_response& pong) noexcept {
        auto& stats = _targets[pong.pingable_id];
        stats.responded++;
        stats.min_time = std::min(stats.min_time, pong.age);
        stats.max_time = std::max(stats.max_time, pong.age);
        stats.sum_time += pong.age;
        stats.finish = std::chrono::steady_clock::now();
        if((++_rcvd % _mod) == 0) [[unlikely]] {
            const auto now{std::chrono::steady_clock::now()};
            const std::chrono::duration<float> interval{now - prev_log};

            if(interval > decltype(interval)::zero()) [[likely]] {
                const auto msgs_per_sec{float(_mod) / interval.count()};

                log_chart_sample("msgsPerSec", msgs_per_sec);
                log_info("received ${rcvd} pongs")
                  .tag("rcvdPongs")
                  .arg("rcvd", _rcvd)
                  .arg("interval", interval)
                  .arg("msgsPerSec", msgs_per_sec)
                  .arg(
                    "done",
                    "MainPrgrss",
                    0.F,
                    static_cast<float>(_rcvd),
                    static_cast<float>(_max));
            }
            prev_log = now;
        }
    }

    void on_ping_timeout(const ping_timeout& fail) noexcept {
        auto& stats = _targets[fail.pingable_id];
        stats.timeouted++;
        if((++_tout % _mod) == 0) [[unlikely]] {
            log_info("${tout} pongs timeouted").arg("tout", _tout);
        }
    }

    auto is_done() const noexcept -> bool {
        return not(
          ((_rcvd + _tout + _mod) < _max) or this->has_pending_pings());
    }

    auto do_update() -> work_done {
        some_true something_done{};
        if(not _targets.empty()) {
            const auto lim{
              _rcvd +
              static_cast<std::intmax_t>(
                static_cast<float>(_mod) *
                (1.F + std::log(static_cast<float>(1 + _targets.size()))))};
            for(auto& [pingable_id, entry] : _targets) {
                if((_rcvd < _max) and (_sent < lim)) {
                    this->ping(
                      pingable_id,
                      adjusted_duration(std::chrono::seconds(3 + _rep)));
                    if((++_sent % _mod) == 0) [[unlikely]] {
                        log_info("sent ${sent} pings")
                          .tag("sentPings")
                          .arg("sent", _sent);
                    }

                    if(entry.should_check_info) [[unlikely]] {
                        if(not entry.host_id) {
                            this->query_host_id(pingable_id);
                        }
                        if(entry.hostname.empty()) {
                            this->query_hostname(pingable_id);
                        }
                    }
                    something_done();
                } else {
                    break;
                }
            }
        }
        return something_done;
    }

    auto update() -> work_done {
        some_true something_done{};
        something_done(base::update());
        if(_do_ping) {
            if(_should_query_pingable) [[unlikely]] {
                log_info("searching for pingables").tag("search");
                query_pingables();
            }
            for([[maybe_unused]] const auto i : integer_range(_rep)) {
                something_done(do_update());
            }
        }
        something_done(base::process_all());
        return something_done;
    }

    void shutdown() {
        log_info("sending shutdown requests to ${count} targets")
          .arg("count", _targets.size());
        for(auto& entry : _targets) {
            this->shutdown_one(std::get<0>(entry));
        }
        base::update();
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
    resetting_timeout _should_query_pingable{std::chrono::seconds(2), nothing};
    std::chrono::steady_clock::time_point prev_log{
      std::chrono::steady_clock::now()};
    std::map<identifier_t, ping_stats> _targets{};
    std::intmax_t _rep;
    std::intmax_t _mod;
    std::intmax_t _max;
    std::intmax_t _sent{0};
    std::intmax_t _rcvd{0};
    std::intmax_t _tout{0};
    bool _do_ping{false};
};
//------------------------------------------------------------------------------
} // namespace msgbus

auto main(main_ctx& ctx) -> int {
    const auto& log{ctx.log()};
    log.active_state("pinging");
    log.declare_state("pinging", "pingStart", "pingFinish");

    enable_message_bus(ctx);
    ctx.preinitialize();

    msgbus::endpoint bus{"PingEndpt", ctx};

    valid_if_positive<std::intmax_t> ping_repeat{};
    if(auto arg{ctx.args().find("--ping-repeat")}) {
        assign_if_fits(arg.next(), ping_repeat);
    }

    valid_if_positive<std::intmax_t> ping_batch{};
    if(auto arg{ctx.args().find("--ping-batch")}) {
        assign_if_fits(arg.next(), ping_batch);
    }

    valid_if_positive<std::intmax_t> ping_count{};
    if(auto arg{ctx.args().find("--ping-count")}) {
        assign_if_fits(arg.next(), ping_count);
    }

    msgbus::ping_example the_pinger{bus, ping_repeat, ping_batch, ping_count};
    msgbus::setup_connectors(ctx, the_pinger);

    resetting_timeout do_chart_stats{std::chrono::seconds(15), nothing};

    log.change("starting").tag("pingStart");
    while(not the_pinger.is_done()) {
        the_pinger.process_all();
        if(not the_pinger.update()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if(do_chart_stats) {
                the_pinger.log_chart_sample(
                  "shortLoad", ctx.system().short_average_load());
                the_pinger.log_chart_sample(
                  "longLoad", ctx.system().long_average_load());
                if(auto temp_k{ctx.system().cpu_temperature()}) {
                    the_pinger.log_chart_sample(
                      "cpuTempC", temp_k->to<units::degree_celsius>());
                }
            }
        }
    }
    log.change("finished").tag("pingFinish");
    the_pinger.shutdown();
    the_pinger.log_stats();

    return 0;
}
//------------------------------------------------------------------------------
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    eagine::main_ctx_options options;
    options.app_id = "PingExe";
    return eagine::main_impl(argc, argv, options, eagine::main);
}
