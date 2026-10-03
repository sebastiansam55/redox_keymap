#!/usr/bin/env python3
import getpass
import sys

def rc4(key: bytes, data: bytes) -> bytes:
    S = list(range(256))
    j = 0
    for i in range(256):
        j = (j + S[i] + key[i % len(key)]) % 256
        S[i], S[j] = S[j], S[i]
    
    out = bytearray()
    i = j = 0
    for char in data:
        i = (i + 1) % 256
        j = (j + S[i]) % 256
        S[i], S[j] = S[j], S[i]
        out.append(char ^ S[(S[i] + S[j]) % 256])
    return bytes(out)

def main():
    import re
    print("TOTP Secret Encryptor for QMK")
    print("-----------------------------")
    account_name = input("Enter a short account name (e.g. GITHUB, AWS): ").strip().upper()
    account_name = re.sub(r'[^A-Z0-9_]', '_', account_name)
    if not account_name:
        account_name = "ACCOUNT"
        
    secret = input("Enter your Base32 TOTP secret: ").strip()
    if not secret:
        print("Secret cannot be empty.")
        sys.exit(1)
        
    pin = getpass.getpass("Enter your decryption PIN/password: ")
    if not pin:
        print("PIN cannot be empty.")
        sys.exit(1)
        
    pin_confirm = getpass.getpass("Confirm PIN: ")
    if pin != pin_confirm:
        print("PINs do not match.")
        sys.exit(1)
        
    encrypted = rc4(pin.encode('utf-8'), secret.encode('utf-8'))
    
    array_str = ", ".join(f"0x{b:02x}" for b in encrypted)
    var_name = f"ENCRYPTED_TOTP_{account_name}"
    new_array_def = f"const uint8_t {var_name}[] = {{ {array_str} }};\n"
    new_entry = f"    {{ {var_name}, sizeof({var_name}) }}, // Slot for {account_name}\n"

    # If the user redirects output, print the raw C code.
    # Otherwise, try to update private/totp.inc in-place safely.
    if not sys.stdout.isatty():
        print(new_array_def)
        print("/* Add the following to your TOTP_SECRETS array: */")
        print(new_entry)
        return

    import os
    totp_inc_path = os.path.join(os.path.dirname(__file__), '..', 'private', 'totp.inc')
    
    if os.path.exists(totp_inc_path):
        with open(totp_inc_path, 'r') as f:
            content = f.read()
            
        if "const totp_secret_t TOTP_SECRETS[] = {" in content:
            # Inject new array definition before the struct array
            content = content.replace("const totp_secret_t TOTP_SECRETS[] = {", new_array_def + "\nconst totp_secret_t TOTP_SECRETS[] = {")
            # Inject new entry into the struct array
            content = content.replace("};\nconst uint8_t NUM_TOTP_SECRETS", new_entry + "};\nconst uint8_t NUM_TOTP_SECRETS")
            
            with open(totp_inc_path, 'w') as f:
                f.write(content)
                
            print(f"\nSuccess! Automatically updated {totp_inc_path} with {account_name}.")
            return

    # If file doesn't exist or is malformed, create it from scratch
    os.makedirs(os.path.dirname(totp_inc_path), exist_ok=True)
    with open(totp_inc_path, 'w') as f:
        f.write(new_array_def + "\n")
        f.write("const totp_secret_t TOTP_SECRETS[] = {\n")
        f.write(new_entry)
        f.write("};\nconst uint8_t NUM_TOTP_SECRETS = sizeof(TOTP_SECRETS) / sizeof(TOTP_SECRETS[0]);\n")
        
    print(f"\nSuccess! Created {totp_inc_path} with {account_name}.")

if __name__ == "__main__":
    main()
