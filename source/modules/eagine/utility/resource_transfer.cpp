/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.utility:resource_transfer;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.main_ctx;
import eagine.core.resource;
import eagine.msgbus.core;
import eagine.msgbus.services;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export using resource_data_server_node_base =
  service_composition<require_services<
    subscriber,
    shutdown_target,
    resource_server,
    pingable,
    common_info_providers>>;
//------------------------------------------------------------------------------
/// @brief Basic resource server message bus service.
/// @ingroup msgbus
/// @see resource_data_consumer_node
export class resource_data_server_node
  : public main_ctx_object
  , public resource_data_server_node_base {
    using base = resource_data_server_node_base;

    void _init();

public:
    /// @brief Initializing constructor.
    resource_data_server_node(endpoint& bus);

    /// @brief Initializing constructor with explicit driver reference.
    resource_data_server_node(endpoint& bus, resource_server_driver& drvr);

    /// @brief Updates the server info about average message age from base node.
    auto update_message_age() -> resource_data_server_node&;

    /// @brief Indicates if the server received a shutdown request.
    auto is_done() const noexcept -> bool {
        return _done;
    }

private:
    void _handle_shutdown(
      const result_context&,
      const shutdown_request&) noexcept;

    bool _done{false};
};
//------------------------------------------------------------------------------
export using resource_data_consumer_node_base =
  service_composition<require_services<subscriber, resource_manipulator, pinger>>;
//------------------------------------------------------------------------------
export struct resource_data_consumer_node_config {
    application_config_value<std::chrono::seconds> server_check_interval;
    application_config_value<std::chrono::seconds> server_response_timeout;
    application_config_value<std::chrono::seconds> resource_search_interval;
    application_config_value<std::chrono::seconds> resource_stream_timeout;
    int _dummy;

    resource_data_consumer_node_config(application_config& c);
};
//------------------------------------------------------------------------------
/// @brief Structure containing parameters for a resource request.
/// @see resource_loader
/// @see resource_request_result
/// @ingroup msgbus
export struct resource_request_params {
    /// @brief The locator of the requested resource.
    url locator;

    /// @brief Maximum time (timeout) for the resource request.
    std::optional<std::chrono::seconds> max_time{};

    /// @brief The priority of the resource request.
    std::optional<msgbus::message_priority> priority{};
};
//------------------------------------------------------------------------------
/// @brief Message bus service consuming resource data blocks.
/// @ingroup msgbus
export class resource_data_consumer_node
  : public main_ctx_object
  , public resource_data_consumer_node_base
  , public blob_stream_signals {
    using base = resource_data_consumer_node_base;

    void _init();

public:
    /// @brief Initializing constructor.
    resource_data_consumer_node(endpoint& bus);

    /// @brief Return a reference to the internal buffer pool.
    auto buffers() noexcept -> memory::buffer_pool& {
        return _buffers;
    }

    /// @brief Does some work and updates internal state (should be called periodically).
    auto update_and_process_all() noexcept -> work_done override;

    /// @brief Indicates if embedded resource with the specified id is available.
    auto has_embedded_resource(identifier res_id) noexcept -> bool {
        return _embedded_loader.has_resource(res_id);
    }

    /// @brief Returns a URL for embedded resource with the specified id.
    auto embedded_resource_locator(
      const string_view scheme,
      identifier res_id) noexcept -> url;

    /// @brief Returns a new unique id for a resource request.
    /// @see query_resource
    /// @see stream_resource
    /// @see fetch_resource_chunks
    auto get_request_id() noexcept -> identifier_t;

    /// @brief Queries a resource with the specified URL and target I/O object.
    /// @see stream_resource
    /// @see fetch_resource_chunks
    void query_resource(
      const resource_request_params& params,
      shared_holder<target_blob_io> io,
      const bool all_in_one) {
        _query_resource(get_request_id(), params, std::move(io), all_in_one);
    }

    /// @brief Requests a resource stream with the specified URL.
    /// @see fetch_resource_chunks
    ///
    /// This function uses a streaming target data I/O that
    /// the blob_stream_data_appended signal is repeatedly emitted as
    /// consecutive blocks of the resource data arrive in the order from the
    /// start to the end of the resource BLOB.
    ///
    /// Returns a pair of unique resource request identifier and the URL.
    auto stream_resource(const resource_request_params& params)
      -> std::pair<identifier_t, const url&>;

    /// @brief Requests a resource as a collection of chunks with the specified URL.
    /// @see stream_resource
    ///
    /// This function uses a chunking target data I/O that
    /// the blob_stream_data_appended signal is emitted once as all equal-sized
    /// chunks of the resource data is loaded.
    ///
    /// Returns a pair of unique resource request identifier and the URL.
    auto fetch_resource_chunks(
      const resource_request_params& params,
      const span_size_t chunk_size) -> std::pair<identifier_t, const url&>;

    /// @brief Requests a resource as a collection of chunks with the specified URL.
    /// @see stream_resource
    ///
    /// Returns a pair of unique resource request identifier and the URL.
    auto fetch_resource_chunks(const resource_request_params& params)
      -> std::pair<identifier_t, const url&> {
        return fetch_resource_chunks(params, 4096);
    }

    /// @brief Cancels a resource request with the specified identifier.
    auto cancel_resource_stream(identifier_t request_id) noexcept -> bool;

    /// @brief Indicates if a resource request with the specified id is still pending.
    /// @see has_pending_resources
    auto has_pending_resource(identifier_t request_id) const noexcept -> bool;

    /// @bried Indicates if there are any resource requests pending.
    /// @see has_pending_resource
    auto has_pending_resources() const noexcept -> bool;

private:
    struct _server_info {
        timeout should_check{};
        timeout not_responding{};
    };

    struct _embedded_resource_info {
        resource_data_consumer_node& _parent;
        const identifier_t _request_id{0};
        span_size_t _unpack_offset{0};
        const url _locator{};
        block_stream_decompression _unpacker;
        blob_info _binfo{};
        std::vector<memory::buffer> _chunks;
        bool _is_all_in_one{false};

        _embedded_resource_info(
          resource_data_consumer_node& parent,
          identifier_t request_id,
          url locator,
          const embedded_resource& resource);

        auto _unpack_data(memory::const_block data) noexcept -> bool;
        auto _finish_data() noexcept -> bool;

        struct request_id_equal {
            span_size_t request_id;

            request_id_equal(span_size_t id) noexcept
              : request_id{id} {}

            auto operator()(auto& entry) const noexcept -> bool {
                return entry->_request_id == request_id;
            }
        };

        auto unpack_next() noexcept -> bool;
    };

    struct _streamed_resource_info {
        url locator{};
        endpoint_id_t source_server_id{};
        shared_holder<target_blob_io> resource_io{};
        timeout should_search{};
        timeout blob_timeout{};
        message_sequence_t blob_stream_id{0};
        message_priority blob_priority{message_priority::normal};
    };

    auto _query_resource(
      identifier_t res_id,
      const resource_request_params& params,
      shared_holder<target_blob_io> io,
      const bool all_in_one) -> std::pair<identifier_t, const url&>;

    void _handle_server_appeared(endpoint_id_t) noexcept;
    void _handle_server_lost(endpoint_id_t) noexcept;
    void _handle_resource_found(endpoint_id_t, const url&) noexcept;
    void _handle_missing(endpoint_id_t, const url&) noexcept;
    void _handle_stream_done(identifier_t) noexcept;
    void _handle_stream_cancelled(identifier_t) noexcept;
    void _handle_stream_data(const blob_stream_chunk&) noexcept;
    void _handle_ping_response(
      const result_context&,
      const ping_response&) noexcept;
    void _handle_ping_timeout(const ping_timeout&) noexcept;

    resource_data_consumer_node_config _config;

    identifier_t _res_id_seq{0};
    memory::buffer_pool _buffers;

    embedded_resource_loader _embedded_loader;
    std::map<endpoint_id_t, _server_info> _current_servers;
    std::map<identifier_t, _streamed_resource_info> _streamed_resources;
    std::vector<unique_holder<_embedded_resource_info>> _embedded_resources;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
