/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_SERVICE_HPP
#define EAGINE_MSGBUS_SERVICE_HPP

#include "invoker.hpp"
#include "serialize.hpp"
#include "service_interface.hpp"
#include "skeleton.hpp"
#include "subscriber.hpp"
#include <eagine/bool_aggregate.hpp>
#include <eagine/protected_member.hpp>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Helper mixin class for message bus services composed of several parts.
/// @ingroup msgbus
template <typename Base = subscriber>
class service_composition
  : public Base
  , public connection_user
  , public service_interface {

    using This = service_composition;

public:
    /// @brief Construction from a reference to an endpoint.
    service_composition(endpoint& bus)
      : Base{bus} {
        _init();
    }

    /// @brief Move constructible.
    // NOLINTNEXTLINE(hicpp-noexcept-move,performance-noexcept-move-constructor)
    service_composition(service_composition&& that)
      : Base{static_cast<Base&&>(that)} {
        _init();
    }

    /// @brief Not copy constructible.
    service_composition(const service_composition&) = delete;

    /// @brief Not move assignable.
    auto operator=(service_composition&&) = delete;

    /// @brief Not copy assignable.
    auto operator=(const service_composition&) = delete;

    ~service_composition() noexcept override {
        this->retract_subscriptions();
        this->finish();
    }

    /// @brief Adds a connection to the associated endpoint.
    auto add_connection(std::unique_ptr<connection> conn) noexcept
      -> bool final {
        return this->bus_node().add_connection(std::move(conn));
    }

    /// @brief Updates the associated endpoint and processes all incoming messages.
    auto update_and_process_all() noexcept -> work_done final {
        some_true something_done{};
        something_done(this->update());
        something_done(this->process_all());
        return something_done;
    }

protected:
    void add_methods() {
        Base::add_methods();
        Base::add_method(
          this,
          EAGINE_MSG_MAP(eagiMsgBus, qrySubscrp, This, _handle_sup_query));
        Base::add_method(
          this,
          EAGINE_MSG_MAP(eagiMsgBus, qrySubscrb, This, _handle_sub_query));
    }

private:
    auto _handle_sup_query(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        this->respond_to_subscription_query(message.source_id);
        return true;
    }

    auto _handle_sub_query(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        message_id sub_msg_id{};
        if(default_deserialize_message_type(sub_msg_id, message.content())) {
            this->respond_to_subscription_query(message.source_id, sub_msg_id);
        }
        return true;
    }

    void _init() {
        this->add_methods();
        this->init();
        this->announce_subscriptions();
    }
};
//------------------------------------------------------------------------------
template <typename Base = subscriber>
class service_node
  : public main_ctx_object
  , private protected_member<endpoint>
  , public service_composition<Base> {
public:
    service_node(const identifier id, main_ctx_parent parent)
      : main_ctx_object{id, parent}
      , protected_member<endpoint>{id, parent}
      , service_composition<Base>{this->get_the_member()} {}
};
//------------------------------------------------------------------------------
template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_callback_invoker = callback_invoker<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_invoker = invoker<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_skeleton = skeleton<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_function_skeleton = function_skeleton<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_lazy_skeleton = lazy_skeleton<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_async_skeleton = async_skeleton<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_SERVICE_HPP
