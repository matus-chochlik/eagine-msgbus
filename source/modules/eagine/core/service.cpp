/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:service;

import eagine.core.types;
import eagine.core.identifier;
import eagine.core.serialization;
import eagine.core.utility;
import eagine.core.main_ctx;
import :types;
import :message;
import :interface;
import :invoker;
import :skeleton;
import :endpoint;
import :subscriber;
import :handler_map;
import std;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Helper mixin class for message bus services composed of several parts.
/// @ingroup msgbus
export template <typename Base = subscriber>
class service_composition
  : public Base
  , public connection_user
  , public service_interface {

    using This = service_composition;

public:
    /// @brief Construction from a reference to an endpoint.
    template <typename... Args>
    service_composition(endpoint& bus, Args&&... args)
      : Base{bus, std::forward<Args>(args)...} {
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

    auto process_queues() noexcept
      -> pointee_generator<const subscriber_message_queue*> final {
        return Base::process_queues();
    }

    auto give_decoded() noexcept
      -> tuple_generator<const result_context&, decode_result_t<Base>> {
        for(auto& queue : process_queues()) {
            for(auto& message : queue.give_messages()) {
                co_yield {
                  result_context{queue.context(), message},
                  this->decode(queue.context(), message)};
            }
        }
    }

    /// @brief Updates the associated endpoint.
    auto update_only() noexcept -> work_done override {
        return this->update();
    }

    /// @brief Updates the associated endpoint and processes all incoming messages.
    auto update_and_process_all() noexcept -> work_done override {
        some_true something_done{};
        something_done(this->update());
        something_done(this->process_all());
        return something_done;
    }

    /// @brief Indicates if the underlying endpoint as an assigned id.
    /// @see get_id
    auto has_id() const noexcept -> bool final {
        return this->bus_node().has_id();
    }

    /// @brief Returns the underlying endpoint identifier if one is assigned.
    /// @see has_id
    auto get_id() const noexcept -> std::optional<identifier_t> {
        const auto id{this->bus_node().get_id()};
        if(is_valid_endpoint_id(id)) {
            return {id};
        }
        return {};
    }

protected:
    void add_methods() noexcept {
        Base::add_methods();
        Base::add_method(
          this, msgbus_map<"qrySubscrp", &This::_handle_sup_query>());
        Base::add_method(
          this, msgbus_map<"qrySubscrb", &This::_handle_sub_query>());
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

    void _init() noexcept {
        this->add_methods();
        this->init();
        this->announce_subscriptions();
    }
};
//------------------------------------------------------------------------------
template <typename T>
concept composed_service = requires(T x) {
    []<typename Base>(service_composition<Base>&) {
    }(x);
};
//------------------------------------------------------------------------------
export template <typename Base = subscriber>
class service_node
  : public main_ctx_object
  , private protected_member<endpoint>
  , public service_composition<Base> {
public:
    service_node(const identifier id, main_ctx_parent parent) noexcept
      : main_ctx_object{id, parent}
      , protected_member<endpoint>{id, parent}
      , service_composition<Base>{this->get_the_member()} {}
};
//------------------------------------------------------------------------------
export template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_callback_invoker = callback_invoker<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
export template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_invoker = invoker<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
export template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_skeleton = skeleton<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
export template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_function_skeleton = function_skeleton<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
export template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_lazy_skeleton = lazy_skeleton<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
export template <typename Signature, std::size_t MaxDataSize = 8192 - 128>
using default_async_skeleton = async_skeleton<
  Signature,
  default_serializer_backend,
  default_deserializer_backend,
  block_data_sink,
  block_data_source,
  MaxDataSize>;
//------------------------------------------------------------------------------
export template <
  class Base,
  template <typename...>
  class Required,
  typename... ReqiredArgs>
struct has_base_service : std::false_type {};

export template <
  class BaseOfBase,
  template <typename...>
  class BaseService,
  typename... BaseArgs,
  template <typename...>
  class Required,
  typename... RequiredArgs>
struct has_base_service<
  BaseService<BaseOfBase, BaseArgs...>,
  Required,
  RequiredArgs...> : has_base_service<BaseOfBase, Required, RequiredArgs...> {};

export template <
  class BaseOfBase,
  template <typename...>
  class Required,
  typename... RequiredArgs>
struct has_base_service<
  Required<BaseOfBase, RequiredArgs...>,
  Required,
  RequiredArgs...> : std::true_type {};

export template <class Base, template <typename...> class Required, typename... Args>
constexpr const bool has_base_service_v =
  has_base_service<Base, Required, Args...>::value;
//------------------------------------------------------------------------------
export template <class Base, template <typename...> class Required, typename... Args>
using require_service = std::conditional_t<
  has_base_service_v<Base, Required, Args...>,
  Base,
  Required<Base, Args...>>;
//------------------------------------------------------------------------------
export template <class Base, template <class> class... Requirements>
struct get_required_services;

export template <class Base, template <class> class... Requirements>
using require_services =
  typename get_required_services<Base, Requirements...>::type;

export template <class Base>
struct get_required_services<Base> : std::type_identity<Base> {};

export template <
  class Base,
  template <class>
  class Required,
  template <class>
  class... Requirements>
struct get_required_services<Base, Required, Requirements...>
  : std::type_identity<
      require_service<require_services<Base, Requirements...>, Required>> {};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

