//! Password-protected credential storage. The on-disk envelope authenticates
//! its format, salt and nonce; KDF costs are fixed, never supplied by the file.
use argon2::{Algorithm, Argon2, Params, Version};
use base64::{engine::general_purpose::STANDARD, Engine};
use chacha20poly1305::{
    aead::{Aead, KeyInit, Payload},
    XChaCha20Poly1305, XNonce,
};
use zeroize::Zeroizing;

const MAGIC: &[u8; 8] = b"SWFTVLT1";
const HEADER: usize = 8 + 16 + 24;
pub(crate) const MAX_PLAINTEXT: usize = 1024 * 1024;
const MAX_ENCODED: usize = 2 * MAX_PLAINTEXT;

pub(crate) struct Vault {
    key: Zeroizing<[u8; 32]>,
    salt: [u8; 16],
}

impl Vault {
    pub(crate) fn open(password: &str, encoded: &str) -> Result<(Self, Zeroizing<Vec<u8>>), String> {
        if password.is_empty() || password.len() > 1024 {
            return Err("Invalid vault password length".into());
        }
        if encoded.len() > MAX_ENCODED {
            return Err("Credential vault is too large".into());
        }
        let bytes = if encoded.is_empty() {
            Vec::new()
        } else {
            STANDARD.decode(encoded).map_err(|_| "Invalid credential vault")?
        };
        let mut salt = [0; 16];
        if bytes.is_empty() && encoded.is_empty() {
            getrandom::getrandom(&mut salt).map_err(|_| "Secure random generator failed")?;
        } else {
            if bytes.len() < HEADER + 16 || &bytes[..8] != MAGIC {
                return Err("Invalid credential vault format".into());
            }
            salt.copy_from_slice(&bytes[8..24]);
        }
        let mut key = Zeroizing::new([0; 32]);
        let params = Params::new(65536, 3, 1, Some(32)).map_err(|_| "Invalid vault KDF parameters")?;
        Argon2::new(Algorithm::Argon2id, Version::V0x13, params)
            .hash_password_into(password.as_bytes(), &salt, key.as_mut())
            .map_err(|_| "Could not derive the vault key")?;
        let vault = Self { key, salt };
        if bytes.is_empty() {
            return Ok((vault, Zeroizing::new(b"{}".to_vec())));
        }
        let cipher = XChaCha20Poly1305::new((&*vault.key).into());
        let plaintext = cipher.decrypt(
            XNonce::from_slice(&bytes[24..HEADER]),
            Payload { msg: &bytes[HEADER..], aad: &bytes[..HEADER] },
        ).map_err(|_| "Wrong password or damaged credential vault")?;
        if plaintext.len() > MAX_PLAINTEXT {
            return Err("Credential vault is too large".into());
        }
        Ok((vault, Zeroizing::new(plaintext)))
    }

    pub(crate) fn seal(&self, plaintext: &[u8]) -> Result<String, String> {
        if plaintext.len() > MAX_PLAINTEXT {
            return Err("Credential vault is too large".into());
        }
        let mut header = [0; HEADER];
        header[..8].copy_from_slice(MAGIC);
        header[8..24].copy_from_slice(&self.salt);
        getrandom::getrandom(&mut header[24..]).map_err(|_| "Secure random generator failed")?;
        let cipher = XChaCha20Poly1305::new((&*self.key).into());
        let ciphertext = cipher.encrypt(
            XNonce::from_slice(&header[24..]), Payload { msg: plaintext, aad: &header },
        ).map_err(|_| "Could not encrypt the credential vault")?;
        let mut bytes = header.to_vec();
        bytes.extend_from_slice(&ciphertext);
        Ok(STANDARD.encode(bytes))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip_and_wrong_password() {
        let (vault, _) = Vault::open("fixture password only", "").unwrap();
        let tokens = b"{\"refreshToken\":\"fixture-secret\"}";
        let encoded = vault.seal(tokens).unwrap();
        assert!(!encoded.contains("fixture-secret"));
        let (_, restored) = Vault::open("fixture password only", &encoded).unwrap();
        assert_eq!(&*restored, tokens);
        assert!(Vault::open("wrong password", &encoded).is_err());
        // The same payload never reuses a nonce.
        assert_ne!(encoded, vault.seal(tokens).unwrap());
    }

    #[test]
    fn altered_ciphertext_and_headers_are_rejected() {
        let (vault, _) = Vault::open("fixture password only", "").unwrap();
        let encoded = vault.seal(b"{}").unwrap();
        let bytes = STANDARD.decode(encoded).unwrap();
        for offset in [0, 8, 24, HEADER] {
            let mut bad = bytes.clone();
            bad[offset] ^= 1;
            assert!(Vault::open("fixture password only", &STANDARD.encode(bad)).is_err());
        }
        assert!(Vault::open("fixture password only", "not base64").is_err());
        assert!(Vault::open("fixture password only", &STANDARD.encode(&bytes[..HEADER])).is_err());
    }
}
