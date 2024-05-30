#!/bin/bash
# A script that runs all integration tests in one go

set -euxo pipefail
cd ${SI_DIR:-"."}
pwd

rm -rf tmp-* || true

python3 .ci/test_sonata_sanity.py

set +x
echo "[`date`] Integration Tests Finished"
