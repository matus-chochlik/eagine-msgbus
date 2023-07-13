/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.core;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.main_ctx;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
auto registered_entry::update_service() noexcept -> work_done {
    some_true something_done;
    if(_service) [[likely]] {
        something_done(_service->update_only());
    }
    return something_done;
}
//------------------------------------------------------------------------------
auto registered_entry::update_and_process_service() noexcept -> work_done {
    some_true something_done;
    if(_service) [[likely]] {
        something_done(_service->update_and_process_all());
    }
    return something_done;
}
//------------------------------------------------------------------------------
registry::registry(main_ctx_parent parent) noexcept
  : main_ctx_object{"MsgBusRgtr", parent}
  , _acceptor{make_direct_acceptor(*this)}
  , _router{*this} {
    _router.add_acceptor(_acceptor);

    locate<message_bus_setup>().and_then(
      [this](auto& setup) { setup.setup_connectors(_router); });
}
//------------------------------------------------------------------------------
auto registry::_add_entry(const identifier log_id) noexcept
  -> registered_entry& {
    unique_holder<endpoint> new_ept{
      default_selector, main_ctx_object{log_id, *this}};
    new_ept->add_connection(_acceptor->make_connection());

    _entries.emplace_back();
    auto& entry = _entries.back();

    entry._endpoint = std::move(new_ept);

    return entry;
}
//------------------------------------------------------------------------------
void registry::remove(service_interface& service) noexcept {
    std::erase_if(_entries, [&service](auto& entry) {
        return entry._service.get() == &service;
    });
}
//------------------------------------------------------------------------------
auto registry::update_self() noexcept -> work_done {
    return _router.update(8);
}
//------------------------------------------------------------------------------
auto registry::update_only() noexcept -> work_done {
    some_true something_done{};

    something_done(_router.do_work());

    for(auto& entry : _entries) {
        something_done(entry.update_service());
    }

    something_done(_router.do_work());
    something_done(_router.do_maintenance());

    return something_done;
}
//------------------------------------------------------------------------------
auto registry::update_and_process() noexcept -> work_done {
    some_true something_done{};

    something_done(_router.do_work());

    for(auto& entry : _entries) {
        something_done(entry.update_and_process_service());
    }

    something_done(_router.do_work());
    something_done(_router.do_maintenance());

    return something_done;
}
//------------------------------------------------------------------------------
auto registry::wait_for_ids(const std::chrono::milliseconds t) noexcept
  -> bool {
    timeout get_id_time{t};
    const auto missing_ids{[this]() {
        for(const auto& entry : _entries) {
            assert(entry._service);
            if(not entry._service->has_id()) {
                return true;
            }
        }
        return false;
    }};
    while(missing_ids()) {
        if(get_id_time.is_expired()) [[unlikely]] {
            return false;
        }
        update_and_process();
    }
    return true;
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
