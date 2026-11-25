#!/usr/bin/env bash
# Download the chosen CC0 assets, check SHA-256, unpack into web3d/assets/. Needs curl and python3.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=web3d/assets; mkdir -p "$OUT" /tmp/tribes-assets
OGA=https://opengameart.org/sites/default/files
# name | url | sha256   (all CC0 1.0; see docs/ASSETS.md)
while IFS='|' read -r name url sha; do
  f=/tmp/tribes-assets/$name.zip
  [ -f "$f" ] || curl -fsSL -o "$f" "$url"
  echo "$sha  $f" | sha256sum -c - >/dev/null || { echo "HASH MISMATCH: $name" >&2; exit 1; }
  python3 -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])" "$f" "$OUT/$name"
  echo "ok $name"
done <<LIST
characters|$OGA/ultimate_animated_character_pack_by_quaternius.zip|0295062cbc72da52ee804f39ac38694901a40458011f9b2638fc7ef13ed509da
farm_animals|$OGA/Farm%20Animals%20by%20%40Quaternius.zip|9ff42b7e2445eaf259e4734c08172637db92e4b59a6145909ac56d946152c39b
nature|$OGA/Nature%20pack%20vol.3.zip|7bf32888a521e93c94d82fc2cebda482de52c014cde9d406dc62da1575f9070d
village|$OGA/medieval_village_pack_-_dec_2020.zip|c38d8632c3c883043809a5db9b2f47b7033e298ab4bd073bb5c29ad323df60fc
crops|$OGA/nature_crops_pack_by_quaternius.zip|48de7f1e092750a3832501e79d1463abeec8218c76090fc4b81fa8dfccacdb52
sand_texture|https://ambientcg.com/get?file=Ground061_1K-JPG.zip|12cfdf98ec086fc6ed1242d9f3cedd7c4f82cd44721ed06cba13ece97ab1915c
LIST
