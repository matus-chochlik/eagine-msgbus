/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.core;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.utility;
import eagine.core.identifier;
import eagine.core.reflection;
import eagine.core.main_ctx;
import eagine.core.c_api;
import eagine.sslplus;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
context::context(main_ctx_parent parent) noexcept
  : main_ctx_object{"MsgBusCtxt", parent} {

    if(ok make_result{_ssl.new_x509_store()}) {
        _ssl_store = std::move(make_result.get());
    } else {
        log_error("failed to create certificate store: ${reason}")
          .arg("reason", (not make_result).message());
    }
}
//------------------------------------------------------------------------------
context::~context() noexcept {
    for(auto& remote : _remotes) {
        auto& info = std::get<1>(remote);
        _ssl.delete_pkey(std::move(info.pubkey));
        _ssl.delete_x509(std::move(info.cert));
    }

    if(_own_pkey) {
        _ssl.delete_pkey(std::move(_own_pkey));
    }

    if(_ca_cert) {
        _ssl.delete_x509(std::move(_ca_cert));
    }

    if(_own_cert) {
        _ssl.delete_x509(std::move(_own_cert));
    }

    if(_ssl_store) {
        _ssl.delete_x509_store(std::move(_ssl_store));
    }
}
//------------------------------------------------------------------------------
auto context::next_sequence_no(const message_id msg_id) noexcept
  -> message_sequence_t {

    const auto [pos, newone] = _msg_id_seq.try_emplace(msg_id);

    if(newone) {
        std::get<1>(*pos) = 0U;
        log_debug("creating sequence for message type ${message}")
          .arg("message", msg_id);
    }
    return std::get<1>(*pos)++;
}
//------------------------------------------------------------------------------
auto context::verify_certificate(const sslplus::x509 cert) noexcept -> bool {
    if(ok vrfy_ctx{_ssl.new_x509_store_ctx()}) {
        auto del_vrfy{_ssl.delete_x509_store_ctx.raii(vrfy_ctx)};

        if(_ssl.init_x509_store_ctx(vrfy_ctx, _ssl_store, cert)) {
            if(const ok verify_res{_ssl.x509_verify_certificate(vrfy_ctx)}) {
                return true;
            } else {
                log_debug("failed to verify x509 certificate")
                  .arg("reason", (not verify_res).message());
            }
        } else {
            log_debug("failed to init x509 certificate store context");
        }
    } else {
        log_error("failed to create x509 certificate store")
          .arg("reason", (not vrfy_ctx).message());
    }
    return false;
}
//------------------------------------------------------------------------------
auto context::verify_certificate_node_kind(
  const sslplus::x509 cert,
  const node_kind kind) noexcept -> bool {
    return _ssl.certificate_subject_name_has_entry_value(
      cert,
      "eagiMsgBusNodeKind",
      "1.3.6.1.4.1.55765.3.2",
      enumerator_name(kind));
}
//------------------------------------------------------------------------------
auto context::add_own_certificate_pem(const memory::const_block blk) noexcept
  -> bool {
    if(blk) {
        if(ok cert{_ssl.parse_x509(blk, {})}) {
            if(_own_cert) {
                _ssl.delete_x509(std::move(_own_cert));
            }
            _own_cert = std::move(cert.get());
            memory::copy_into(blk, _own_cert_pem);
            return verify_certificate(_own_cert);
        } else {
            log_error("failed to parse own x509 certificate from pem")
              .arg("reason", (not cert).message())
              .arg("pem", blk);
        }
    }
    return false;
}
//------------------------------------------------------------------------------
auto context::add_ca_certificate_pem(const memory::const_block blk) noexcept
  -> bool {
    if(blk) {
        if(ok cert{_ssl.parse_x509(blk, {})}) {
            if(_ssl.add_cert_into_x509_store(_ssl_store, cert)) {
                if(_ca_cert) {
                    _ssl.delete_x509(std::move(_ca_cert));
                }
                _ca_cert = std::move(cert.get());
                memory::copy_into(blk, _ca_cert_pem);
                return not _own_cert or verify_certificate(_own_cert);
            } else {
                log_error("failed to add x509 CA certificate to store")
                  .arg("reason", (not cert).message())
                  .arg("pem", blk);
            }
        } else {
            log_error("failed to parse CA x509 certificate from pem")
              .arg("reason", (not cert).message())
              .arg("pem", blk);
        }
    }
    return false;
}
//------------------------------------------------------------------------------
auto context::add_remote_certificate_pem(
  const identifier_t node_id,
  const memory::const_block blk) noexcept -> bool {
    if(blk) {
        if(ok cert{_ssl.parse_x509(blk, {})}) {
            auto& info = _remotes[node_id];
            if(info.cert) {
                _ssl.delete_x509(std::move(info.cert));
            }
            if(info.pubkey) {
                _ssl.delete_pkey(std::move(info.pubkey));
            }
            info.cert = std::move(cert.get());
            memory::copy_into(blk, info.cert_pem);
            if(verify_certificate(info.cert)) {
                if(ok pubkey{_ssl.get_x509_pubkey(info.cert)}) {
                    info.pubkey = std::move(pubkey.get());
                    fill_with_random_bytes(cover(info.nonce), _rand_engine);
                    return true;
                } else {
                    log_error("failed to get remote node x509 public key")
                      .arg("nodeId", node_id)
                      .arg("reason", (not pubkey).message())
                      .arg("pem", blk);
                }
            } else {
                log_debug("failed to verify remote node certificate")
                  .arg("nodeId", node_id);
            }
        } else {
            log_error("failed to parse remote node x509 certificate from pem")
              .arg("nodeId", node_id)
              .arg("reason", (not cert).message())
              .arg("pem", blk);
        }
    } else {
        log_error("received empty x509 certificate pem")
          .arg("nodeId", node_id)
          .arg("pem", blk);
    }
    return false;
}
//------------------------------------------------------------------------------
auto context::get_remote_certificate_pem(
  const identifier_t node_id) const noexcept -> memory::const_block {
    auto pos = _remotes.find(node_id);
    if(pos != _remotes.end()) {
        return view(std::get<1>(*pos).cert_pem);
    }
    return {};
}
//------------------------------------------------------------------------------
auto context::get_remote_nonce(const identifier_t node_id) const noexcept
  -> memory::const_block {
    const auto pos = _remotes.find(node_id);
    if(pos != _remotes.end()) {
        return view(std::get<1>(*pos).nonce);
    }
    return {};
}
//------------------------------------------------------------------------------
auto context::verified_remote_key(const identifier_t node_id) const noexcept
  -> bool {
    const auto pos = _remotes.find(node_id);
    if(pos != _remotes.end()) {
        return std::get<1>(*pos).verified_key;
    }
    return false;
}
//------------------------------------------------------------------------------
auto context::default_message_digest() noexcept
  -> decltype(_ssl.message_digest_sha256()) {
    return _ssl.message_digest_sha256();
}
//------------------------------------------------------------------------------
auto context::message_digest_sign_init(
  const sslplus::message_digest mdc,
  const sslplus::message_digest_type mdt) noexcept
  -> decltype(_ssl.message_digest_sign_init.fail()) {
    if(_own_pkey) {
        return _ssl.message_digest_sign_init(mdc, mdt, _ssl_engine, _own_pkey);
    }
    return _ssl.message_digest_sign_init.fail();
}
//------------------------------------------------------------------------------
auto context::message_digest_verify_init(
  const sslplus::message_digest mdc,
  const sslplus::message_digest_type mdt,
  const identifier_t node_id) noexcept
  -> decltype(_ssl.message_digest_verify_init.fail()) {
    auto pos = _remotes.find(node_id);
    if(pos != _remotes.end()) {
        auto& info = std::get<1>(*pos);
        if(info.pubkey) {
            return _ssl.message_digest_verify_init(
              mdc, mdt, _ssl_engine, info.pubkey);
        }
    } else {
        log_debug("could not find remote node ${endpoint} for verification")
          .arg("endpoint", node_id);
    }
    return _ssl.message_digest_verify_init.fail();
}
//------------------------------------------------------------------------------
auto context::get_own_signature(const memory::const_block nonce) noexcept
  -> memory::const_block {
    if(ok md_type{default_message_digest()}) {
        if(ok md_ctx{_ssl.new_message_digest()}) {
            auto cleanup{_ssl.delete_message_digest.raii(md_ctx)};

            if(message_digest_sign_init(md_ctx, md_type)) [[likely]] {
                if(_ssl.message_digest_sign_update(md_ctx, nonce)) [[likely]] {
                    const auto req_size{
                      _ssl.message_digest_sign_final.required_size(md_ctx)};

                    _scratch_space.ensure(extract_or(req_size, 0));
                    auto free{cover(_scratch_space)};

                    if(ok sig{_ssl.message_digest_sign_final(md_ctx, free)}) {
                        return sig.get();
                    } else {
                        log_debug("failed to finish ssl signature")
                          .arg("freeSize", free.size())
                          .arg("reason", (not sig).message());
                    }
                } else {
                    log_debug("failed to update ssl signature");
                }
            } else {
                log_debug("failed to init ssl sign context");
            }
        } else {
            log_debug("failed to create ssl message digest")
              .arg("reason", (not md_ctx).message());
        }
    } else {
        log_debug("failed to get ssl message digest type")
          .arg("reason", (not md_type).message());
    }
    return {};
}
//------------------------------------------------------------------------------
auto context::verify_remote_signature(
  const memory::const_block content,
  const memory::const_block signature,
  const identifier_t node_id,
  const bool verified_key) noexcept -> verification_bits {
    verification_bits result{};

    if(content and signature) {
        if(ok md_type{default_message_digest()}) {
            if(ok md_ctx{_ssl.new_message_digest()}) {
                auto cleanup{_ssl.delete_message_digest.raii(md_ctx)};

                if(message_digest_verify_init(md_ctx, md_type, node_id))
                  [[likely]] {
                    if(_ssl.message_digest_verify_update(md_ctx, content))
                      [[likely]] {
                        if(_ssl.message_digest_verify_final(
                             md_ctx, signature)) {

                            if(verified_key or verified_remote_key(node_id)) {
                                result |= verification_bit::source_private_key;
                            }

                            result |= verification_bit::source_certificate;
                            result |= verification_bit::message_content;
                        } else {
                            log_debug("failed to finish ssl verification");
                        }
                    } else {
                        log_debug("failed to update ssl verify context");
                    }
                } else {
                    log_debug("failed to init ssl verify context");
                }
            } else {
                log_debug("failed to create ssl message digest")
                  .arg("reason", (not md_ctx).message());
            }
        } else {
            log_debug("failed to get ssl message digest type")
              .arg("reason", (not md_type).message());
        }
    }
    return result;
}
//------------------------------------------------------------------------------
auto context::verify_remote_signature(
  const memory::const_block sig,
  const identifier_t node_id) noexcept -> bool {
    const auto pos = _remotes.find(node_id);
    if(pos != _remotes.end()) {
        auto& remote{std::get<1>(*pos)};
        const auto result{
          verify_remote_signature(view(remote.nonce), sig, node_id, true)};
        if(result.has(verification_bit::message_content)) {
            remote.verified_key = true;
            return true;
        }
    }
    return false;
}
//------------------------------------------------------------------------------
auto make_context(main_ctx_parent parent) -> std::shared_ptr<context> {
    return std::make_shared<context>(parent);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
