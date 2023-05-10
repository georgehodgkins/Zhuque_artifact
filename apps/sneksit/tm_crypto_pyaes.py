#!/usr/bin/env python
"""
Pure-Python Implementation of the AES block-cipher.

Benchmark AES in CTR mode using the pyaes module.
"""

from sneksit import do_bench

import pyaes

# 23,000 bytes
CLEARTEXT = b"This is a test. What could possibly go wrong? " * 10000

# 128-bit key (16 bytes)
KEY = b'\xa1\xf6%\x8c\x87}_\xcd\x89dHE8\xbf\xc9,'


def enc_then_dec():
    aes = pyaes.AESModeOfOperationCTR(KEY)
    ciphertext = aes.encrypt(CLEARTEXT)

    # need to reset IV for decryption
    aes = pyaes.AESModeOfOperationCTR(KEY)
    plaintext = aes.decrypt(ciphertext)

    # explicitly destroy the pyaes object
    aes = None
  
    if plaintext != CLEARTEXT:
        raise Exception("decrypt error!")



if __name__ == "__main__":
    do_bench('crypto', enc_then_dec, 5)
