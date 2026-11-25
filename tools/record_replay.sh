#!/bin/sh
# usage: tools/record_replay.sh PORT OUT.jsonl [FRAMES] [SECONDS]  -> JSON lines for web3d/world.html?replay=...
P=${1:-8080}; O=${2:-replay.jsonl}; N=${3:-5}; S=${4:-2}
printf '{"type":"map","data":%s}\n' "$(curl -s localhost:$P/api/map | tr -d '\n')" > "$O"
i=0; while [ $i -lt $N ]; do printf '{"type":"people","data":%s}\n' "$(curl -s localhost:$P/api/people | tr -d '\n')" >> "$O"; i=$((i+1)); sleep $S; done
