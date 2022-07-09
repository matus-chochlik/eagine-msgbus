/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus:context;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.main_ctx;
import eagine.sslplus;
import :types;
import :message;
import <array>;
import <map>;
import <random>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct context_remote_node {
    std::array<byte, 256> nonce{};
    memory::buffer cert_pem;
    sslplus::owned_x509 cert{};
    sslplus::owned_pkey pubkey{};
    bool verified_key{false};
};
//------------------------------------------------------------------------------
/// @brief Class holding common message bus utility objects.
/// @ingroup msgbus
export class context : main_ctx_object {
public:
    context(main_ctx_parent parent) noexcept;

    /// @brief Not move constructible.
    context(context&&) = delete;

    /// @brief Not copy constructible.
    context(const context&) = delete;

    /// @brief Not move assignable.
    auto operator=(context&&) = delete;

    /// @brief Not copy assignable.
    auto operator=(const context&) = delete;

    ~context() noexcept;

    /// @brief Returns a reference to the SSL API wrapper.
    auto ssl() noexcept -> sslplus::ssl_api& {
        return _ssl;
    }

    /// @brief Returns the next sequence number value for the specified message type.
    auto next_sequence_no(const message_id) noexcept -> message_sequence_t;

    /// @brief Verifies the specified x509 certificate against CA certificate.
    auto verify_certificate(const sslplus::x509 cert) noexcept -> bool;

    /// @brief Checks if x509 certificate has the specified node kind DN entry.
    auto verify_certificate_node_kind(
      const sslplus::x509 cert,
      const node_kind kind) noexcept -> bool;

    /// @brief Sets this bus node certificate encoded in PEM format.
    /// @see get_own_certificate_pem
    /// @see add_ca_certificate_pem
    /// @see add_remote_certificate_pem
    /// @see add_router_certificate_pem
    auto add_own_certificate_pem(const memory::const_block) noexcept -> bool;

    /// @brief Sets a CA certificate encoded in PEM format.
    /// @see get_ca_certificate_pem
    /// @see add_own_certificate_pem
    /// @see add_remote_certificate_pem
    /// @see add_router_certificate_pem
    auto add_ca_certificate_pem(const memory::const_block) noexcept -> bool;

    /// @brief Sets remote bus node certificate encoded in PEM format.
    /// @see get_remote_certificate_pem
    /// @see add_ca_certificate_pem
    /// @see add_remote_certificate_pem
    /// @see add_router_certificate_pem
    auto add_remote_certificate_pem(
      const identifier_t node_id,
      memory::const_block) noexcept -> bool;

    /// @brief Sets the router certificate encoded in PEM format.
    /// @see get_router_certificate_pem
    /// @see add_own_certificate_pem
    /// @see add_ca_certificate_pem
    /// @see add_remote_certificate_pem
    auto add_router_certificate_pem(const memory::const_block blk) noexcept
      -> bool {
        return add_remote_certificate_pem(0, blk);
    }

    /// @brief Gets this bus node certificate encoded in PEM format.
    /// @see add_own_certificate_pem
    /// @see get_ca_certificate_pem
    /// @see get_remote_certificate_pem
    /// @see get_router_certificate_pem
    auto get_own_certificate_pem() const noexcept -> memory::const_block {
        return view(_own_cert_pem);
    }

    /// @brief Gets the CA certificate encoded in PEM format.
    /// @see add_ca_certificate_pem
    /// @see get_own_certificate_pem
    /// @see get_remote_certificate_pem
    /// @see get_router_certificate_pem
    auto get_ca_certificate_pem() const noexcept -> memory::const_block {
        return view(_ca_cert_pem);
    }

    /// @brief Gets remote bus node certificate encoded in PEM format.
    /// @see add_remote_certificate_pem
    /// @see get_ca_certificate_pem
    /// @see get_remote_certificate_pem
    /// @see get_router_certificate_pem
    auto get_remote_certificate_pem(const identifier_t) const noexcept
      -> memory::const_block;

    /// @brief Gets the router certificate encoded in PEM format.
    /// @see add_router_certificate_pem
    /// @see get_own_certificate_pem
    /// @see get_ca_certificate_pem
    /// @see get_remote_certificate_pem
    auto get_router_certificate_pem() const noexcept -> memory::const_block {
        return get_remote_certificate_pem(0);
    }

    auto get_remote_nonce(const identifier_t) const noexcept
      -> memory::const_block;

    /// @brief Indicates if the private key of a remote node was verified.
    auto verified_remote_key(const identifier_t) const noexcept -> bool;

    /// @brief Returns the default message digest type.
    auto default_message_digest() noexcept
      -> decltype(ssl().message_digest_sha256());

    auto message_digest_sign_init(
      const sslplus::message_digest mdc,
      const sslplus::message_digest_type mdt) noexcept
      -> decltype(ssl().message_digest_sign_init.fail());

    auto message_digest_verify_init(
      const sslplus::message_digest mdc,
      const sslplus::message_digest_type mdt,
      const identifier_t node_id) noexcept
      -> decltype(ssl().message_digest_verify_init.fail());

    /// @brief Signs the specified memory block and returns the signature.
    auto get_own_signature(const memory::const_block) noexcept
      -> memory::const_block;

    auto verify_remote_signature(
      const memory::const_block data,
      const memory::const_block sig,
      const identifier_t,
      const bool = false) noexcept -> verification_bits;

    /// @brief Verifies the signature on a data block from a remote node.
    auto verify_remote_signature(
      const memory::const_block sig,
      const identifier_t) noexcept -> bool;

private:
    //
    std::mt19937_64 _rand_engine{std::random_device{}()};
    flat_map<message_id, message_sequence_t> _msg_id_seq{};
    //
    memory::buffer _scratch_space{};
    memory::buffer _own_cert_pem{};
    memory::buffer _ca_cert_pem{};
    //
    sslplus::ssl_api _ssl{};
    sslplus::owned_engine _ssl_engine{};
    sslplus::owned_x509_store _ssl_store{};
    sslplus::owned_x509 _own_cert{};
    sslplus::owned_x509 _ca_cert{};
    sslplus::owned_pkey _own_pkey{};
    //
    std::map<identifier_t, context_remote_node> _remotes{};
};
//------------------------------------------------------------------------------
export using shared_context = std::shared_ptr<context>;
//------------------------------------------------------------------------------
export [[nodiscard]] auto make_context(main_ctx_parent)
  -> std::shared_ptr<context>;
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

