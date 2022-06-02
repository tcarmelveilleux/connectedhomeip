#!/bin/bash
(scripts/examples/gn_build_example.sh examples/all-clusters-app/linux out/debug/standalone chip_config_network_layer_ble=false && scripts/examples/gn_build_example.sh examples/chip-tool out/debug/standalone/ && \
        ./cert_tests_run.sh $@)
