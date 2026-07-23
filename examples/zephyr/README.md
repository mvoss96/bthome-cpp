# Using bthome-cpp with Zephyr / nRF Connect SDK

The repository ships Zephyr module metadata (`zephyr/module.yml`), so it can
be pulled straight into a west workspace.

## West manifest integration

Add the repository to your manifest:

```yaml
manifest:
    projects:
        - name: bthome-cpp
            url: https://github.com/mvoss96/bthome-cpp
            revision: main
            path: modules/lib/bthome-cpp
```

Then initialize/update your workspace:

```bash
west init -m <your-manifest-repo-url> zephyr-workspace
cd zephyr-workspace
west update
```

Alternatively, clone the repository into your workspace manually and include
`src` in your target include directories.

## Building the examples

Plain Zephyr example ([src/main.cpp](src/main.cpp)):

```bash
west build -b nrf52840dk/nrf52840 examples/zephyr -p always
```

Encrypted example with the PSA Crypto backend
([../zephyr_encrypted](../zephyr_encrypted)) — also the right starting point
for the nRF Connect SDK, where PSA (Oberon/CryptoCell) is the preferred
crypto path:

```bash
west build -b nrf52840dk/nrf52840 examples/zephyr_encrypted -p always
```

For the nRF52840 Dongle use board `nrf52840dongle/nrf52840`.

## Portability note

Zephyr's minimal C++ library ships no C++ wrapper headers for the C library
(only `cstddef`/`cstdint`/`new`); bthome-cpp deliberately uses `<string.h>`
with unqualified calls, so no special configuration is needed beyond
`CONFIG_CPP=y` and `CONFIG_STD_CPP17=y` (see [prj.conf](prj.conf)).
