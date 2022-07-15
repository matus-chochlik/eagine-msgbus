/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.core;

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
        something_done(_service->update_and_process_all());
    }
    return something_done;
}
//------------------------------------------------------------------------------
registry::registry(main_ctx_parent parent) noexcept
  : main_ctx_object{"MsgBusRgtr", parent}
  , _acceptor{std::make_shared<direct_acceptor>(*this)}
  , _router{*this} {
    _router.add_acceptor(_acceptor);

    if(const auto setup{locate<message_bus_setup>()}) {
        extract(setup).setup_connectors(_router);
    }
}
//------------------------------------------------------------------------------
auto registry::_add_entry(const identifier log_id) noexcept
  -> registered_entry& {
    auto new_ept{std::make_unique<endpoint>(main_ctx_object{log_id, *this})};
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
auto registry::update() noexcept -> work_done {
    return _router.update(8);
}
//------------------------------------------------------------------------------
auto registry::update_all() noexcept -> work_done {
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
} // namespace eagine::msgbus
