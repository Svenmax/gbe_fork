/* gbe_ed25519.h -- Minimal standalone Ed25519 (RFC 8032) for Goldberg Emulator
 *
 * Public domain.  Based on the SUPERCOP ref10 / TweetNaCl algorithm with a
 * self-contained SHA-512 to avoid any external dependency beyond <stdint.h>
 * and <string.h>.
 *
 * Only the functions required by Goldberg are exposed:
 *   gbe_ed25519_sign(signature, message, message_len, private_key)
 *
 * The private key is the 32-byte seed (RFC 8032 §5.1.5).
 * The signature is a 64-byte Ed25519 signature.
 */

#ifndef GBE_ED25519_H
#define GBE_ED25519_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sign `message` (of `message_len` bytes) with the 32-byte Ed25519
 * private-key seed `private_key`.  Writes 64 bytes into `signature`.
 * Returns 0 on success, non-zero on failure. */
int gbe_ed25519_sign(uint8_t *signature,
                     const uint8_t *message, size_t message_len,
                     const uint8_t *private_key);

#ifdef __cplusplus
}
#endif

#endif /* GBE_ED25519_H */
