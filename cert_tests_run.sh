#!/bin/bash
(time scripts/tests/run_test_suite.py \
        --target "${@:-all}" \
        --target-skip-glob '{tv*,TV*,*Group*,DL_*,OTA*}' --chip-tool ./out/debug/standalone/chip-tool \
        run \
        --iterations 1 \
        --tv-app ./out/debug/standalone/chip-tv-app \
        --lock-app ./out/debug/standalone/lock-app \
        --all-clusters-app ./out/debug/standalone/chip-all-clusters-app \
        && echo "OK" || echo "FAIL")
killall chip-all-clusters-app
