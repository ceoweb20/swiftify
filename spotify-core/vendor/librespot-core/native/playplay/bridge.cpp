// C ABI over the unplayplay key derivation, so the Rust side needs no C++ types.
// The vendored algorithm is a port of Spotify's PlayPlay key obfuscation; see
// vendor/librespot-core/src/playplay.rs for how it is used.
#include <cstdint>
#include <cstring>

#include "unplayplay.hpp"

extern "C" {
// out_key = bind(decrypt(obfuscated_key), file_id). all buffers are raw bytes;
// key is 16 bytes, file_id is 20 bytes.
void playplay_derive_key(const uint8_t* obfuscated_key, const uint8_t* file_id, uint8_t* out_key) {
    unplayplay::Key enc{};
    std::memcpy(enc.data(), obfuscated_key, enc.size());
    unplayplay::FileId fid{};
    std::memcpy(fid.data(), file_id, fid.size());

    unplayplay::Key out = unplayplay::bind_key(unplayplay::decrypt_key(enc), fid);
    std::memcpy(out_key, out.data(), out.size());
}

// the license request token this derivation pairs with (16 bytes)
void playplay_token(uint8_t* out_token) {
    unplayplay::Token token = unplayplay::get_token();
    std::memcpy(out_token, token.data(), token.size());
}
}
