# Matter ESP32 DiscoBall Example

This example is meant to represent a minimal-sized application.

Please
[setup ESP-IDF and CHIP Environment](../../../docs/guides/esp32/setup_idf_chip.md)
and refer
[building and commissioning](../../../docs/guides/esp32/build_app_and_commission.md)
guides to get started.

---

-   [Cluster control](#cluster-control)
-   [Optimization](#optimization)

---

### Cluster control

#### discoball

```bash
Usage:
  ./out/debug/chip-tool discoball start-request 100 ${NODE_ID} 1 --timedInteractioTimeoutMs 5000
```

## Optimization

Optimization related to WiFi, Bluetooth, asserts, etc, are the part of this
example by default. To enable this option set is_debug=false from command-line.

```
# Reconfigure the project for additional optimizations
rm -rf sdkconfig build/
idf.py -Dis_debug=false reconfigure

# Set additional configurations if required
idf.py menuconfig

# Build, flash, and monitor the device
idf.py -p /dev/tty.SLAB_USBtoUART build flash monitor
```
