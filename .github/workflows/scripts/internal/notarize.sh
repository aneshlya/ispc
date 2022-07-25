#!/bin/bash

# This script is copy-pasted from RenderKit devops actions:
# https://github.com/intel-innersource/libraries.devops.renderkit.actions/blob/devel/notarize-macos/notarize.sh

set -euo pipefail

tempdir="$(mktemp -d)"
function cleanup {
  rm -rf "$tempdir"
}
trap cleanup EXIT

echo "$NOTARYTOOL_API_KEY" | tr ';' '\n' > $tempdir/authkey.p8

## From notarytool docs:
## --wait: Only return from notarytool once the Apple notary service has responded with a status of "Accepted", "Invalid", "Rejected",
##         or if a fatal error has occurred during submission.  This option replaces the need for polling from a script.
read id status < <(xcrun notarytool submit \
                --wait -f json \
                --issuer "$NOTARYTOOL_API_KEY_ISSUER_UUID" \
                --key-id "$NOTARYTOOL_API_KEY_ID" \
                --key $tempdir/authkey.p8 $@ \
                | jq -r '"\(.id) \(.status)"')

if [[ $status == Accepted ]]; then
  xcrun notarytool info \
      --issuer "$NOTARYTOOL_API_KEY_ISSUER_UUID" \
      --key-id "$NOTARYTOOL_API_KEY_ID" \
      --key $tempdir/authkey.p8 $id
  exit 0
else
  xcrun notarytool log \
      --issuer "$NOTARYTOOL_API_KEY_ISSUER_UUID" \
      --key-id "$NOTARYTOOL_API_KEY_ID" \
      --key $tempdir/authkey.p8 $id
  exit 1
fi
