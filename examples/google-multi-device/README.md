### Board setup

- Silabs BRD2703A (MG24 explorer kit)
- Pinout:
  - PC00: Red LED
  - PC03: Yellow LED
  - PC08: Green LED
  - PC02: Red Button
  - PC01: Yellow Button
  - PB01: Green Button
  - PA00: Latch 1
  - PB04: Latch 2
  - PB05: Latch 3
  - PB00: Alternative discriminator/DAC select: floating/high: 2994, low: 681
  - PD04: Proximity sensor
  - PD05: Debug pin

### Build command

- `./scripts/examples/gn_silabs_example.sh ./examples/google-multi-device/silabs/ ./out/google-multi-device-efr BRD2703A sl_matter_version=3 sl_matter_version_str='"1.0.3"' chip_build_example_creds=false`
