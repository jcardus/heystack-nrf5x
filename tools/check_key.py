import base64, re, glob, os

# Find the first .keys file in output/
keys_files = glob.glob("output/*.keys")
if not keys_files:
    raise RuntimeError("No .keys file found in output/ directory")
keys_file = keys_files[0]

print("Using keys file:", keys_file)

with open(keys_file, "r") as f:
    text = f.read()

# Extract the Advertisement key from the .keys file
match = re.search(r"Advertisement key:\s*([A-Za-z0-9+/=]+)", text)
if not match:
    raise RuntimeError("Advertisement key not found in .keys file")

adv_b64 = match.group(1)
adv_key = base64.b64decode(adv_b64)
if len(adv_key) != 28:
    raise RuntimeError(f"Expected 28-byte adv key, got {len(adv_key)}")

# --- Derive MAC address (first 6 bytes) ---
bt_addr = [
    (adv_key[0] | 0b11000000),  # force random static
    adv_key[1],
    adv_key[2],
    adv_key[3],
    adv_key[4],
    adv_key[5],
]
mac_str = ":".join(f"{b:02X}" for b in reversed(bt_addr))

# --- Derive Manufacturer Data (next 22 bytes) ---
mfg_data = adv_key[6:28]
mfg_str = " ".join(f"{b:02X}" for b in mfg_data)

print("Advertisement key (Base64):", adv_b64)
print("Expected MAC address:", mac_str)
print("Expected Manufacturer Data (22 bytes):", mfg_str)
