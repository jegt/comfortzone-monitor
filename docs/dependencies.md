# Pinned dependencies

The monitor uses the qix67 Comfortzone parser as an external dependency. Its source is downloaded during setup rather than copied into this repository.

## Setup and build

```sh
python3 tools/device.py setup
python3 tools/device.py build
```

Setup needs Python 3 and network access, but no device credentials. `dependencies.json` records the upstream repository, full Git revision, archive URL and SHA-256. The installer downloads that archive, verifies its checksum and installs it under `.dependencies/comfortzone_heatpump/`, retaining the upstream license and attribution. The cache is ignored by Git.

Build, USB upload and OTA upload verify the cache before compiling. A missing dependency is downloaded automatically. An existing valid cache works offline; the Arduino core and FastCRC must also already be installed. HTTP status/capture operations do not download dependencies.

## Current versions

| Dependency | Pin | Installation |
|---|---|---|
| qix67/comfortzone_heatpump | `c0a896db8163d721e373c983bc651e7863e8c28b` | `python3 tools/device.py setup` |
| FastCRC | `1.44.0` | `arduino-cli lib install 'FastCRC@1.44.0'` |
| Espressif Arduino core | `3.3.11` | Arduino CLI core manager, as shown in the main README |

The downloaded parser source matches the previously included revision without patches. The monitor's v1.1 field decoding and control engine remain in `comfortzone_wifi/`.

## Cache changes and updates

The cache records checksums for the installed files. Changed, missing or extra files cause verification to fail, rather than silently building a different dependency. To explicitly replace the cache with the locked revision:

```sh
python3 tools/setup_dependencies.py --force
```

The existing cache is replaced only after the download and extracted files pass validation. A failed download/checksum check leaves the existing cache intact. Archive paths and file types are checked before installation.

To deliberately update upstream, select a full Git revision, update the archive URL and SHA-256 in `dependencies.json`, then refresh the cache and run the tests/build. A floating branch or latest release is not used automatically. Firmware compatibility still needs validation when changing a parser dependency.

The upstream project is by Eric PREVOTEAU and distributed under GPLv3. See its [repository](https://github.com/qix67/comfortzone_heatpump) and [license at the pinned revision](https://github.com/qix67/comfortzone_heatpump/blob/c0a896db8163d721e373c983bc651e7863e8c28b/LICENSE). This monitor remains GPL-3.0-or-later under the root LICENSE.
