# qos-utils

qos-utils is a collection of command-line utilities for configuration of
quality-of-service and time-sensitive-networking on microchip switches.

These utilities adds features which at the point of designing/implementing
were not present in the upstream kernel (some have been supported since, others
are still not supported). To use these tools, additional kernel patches are
needed, or a pre-patched Microchip kernel is needed.

We have a strong interest in working with the kernel community to eventually
add upstream support for the features present in this repository.

## Utilities

| Utility  | Description                                        | Standard      |
| -------- | -------------------------------------------------- | --------------|
| fp       | Configuration of Frame Preemption                  | IEEE 802.1Qbu |
| frer     | Configuration of Frame Replication and Elimination | IEEE 802.1CB  |
| psfp     | Configuration of Per-Stream Filtering and Policing | IEEE 802.1Qci |
| qos      | Configuration of Quality Of Service                | IEEE 802.1p   |


## How to build

Install build-time dependencies (see CMakeLists.txt)

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ sudo make install


