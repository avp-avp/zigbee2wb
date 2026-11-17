# zigbee2wb
Convert zigbee2mqtt mqtt notation to wirenboard notation

## How to build

- git clone git@github.com:avp-avp/zigbee2wb.git
- git submodule init  
- git submodule update
- autoreconf -fvi
- ./configure
- make

## Refreshing the device list

The `zigbee` device has a `refresh` button. Pressing it (or publishing `1` to
`/devices/zigbee/controls/refresh/on`) removes from the broker every device whose driver is
`zigbee2wb` - including the ones left by previous runs or deleted from zigbee2mqtt - and then
creates the devices anew from what zigbee2mqtt reports. Devices of other drivers are not touched.

Controls reappear as the devices report their values, the same way as after a start.

## How to run unit tests

- make check

The tests use [doctest](https://github.com/doctest/doctest) (bundled as `tests/doctest.h`)
and do not need an MQTT broker or mosquitto. To run a single suite or see every test case:

- ./tests/unit_tests --test-suite=Bridge
- ./tests/unit_tests --success

## Source layout

Everything except the MQTT client is built as the `libzigbee2wbcore` convenience library,
which is linked both into the service and into the unit tests.

| Module (`zigbee2wb/`) | Responsibility |
|---|---|
| `zigbee2wb.cpp` | `main`: command line, config file, logging |
| `MqttConnection` | thin adapter between libmosquittopp and `CBridge` |
| `Bridge` | routes incoming messages, owns the devices, publishes Wiren Board topics |
| `ZigbeeDevice` | one zigbee2mqtt device as a Wiren Board device: controls, value conversion, `set` payloads |
| `WbDevice` | a device in the [Wiren Board MQTT conventions](https://github.com/wirenboard/conventions): controls, metadata, MQTT messages |
| `Exposes` | parsing of zigbee2mqtt [`exposes`](https://www.zigbee2mqtt.io/guide/usage/exposes.html), choice of the Wiren Board control type |
| `Settings` | `zigbee2wb.json`: MQTT options, converters, per-model templates |
| `Topics` | MQTT topic parsing and building |
| `ValueConverters` | value dictionaries (`ON` <-> `1`), mapping of zigbee2mqtt units to Wiren Board `units` |
| `JsonUtils` | exception-safe jsoncpp helpers |

Numeric values are published as the `value` type with `units` (the dedicated types such as
`temperature` are deprecated by the conventions); settable numbers with a maximum become `range`,
binary values `switch`, everything else `text`. Metadata is published both as JSON in `.../meta`
and as legacy `.../meta/<name>` subtopics.

Tests live in `tests/test_<module>.cpp`; `tests/data/bridge_devices.json` is a sample
`bridge/devices` payload.

## How to build Debian package

- install build dependencies: `sudo apt-get install build-essential debhelper dh-autoreconf autoconf automake libtool libjsoncpp-dev libmosquitto-dev libmosquittopp-dev pkg-config`
- git submodule init
- git submodule update
- dpkg-buildpackage -us -uc -b

The resulting `.deb` package will be created in the parent directory of the repository.

### Build Debian package with `wbdev`

- git submodule init
- git submodule update
- ../wbdev ./debian/build-package.sh

The generated artifacts will be copied to the `dist/` directory in the repository.

## How to build for wirenboard

You need Wirenboard development env from https://github.com/contactless/wirenboard 

Run bould command with devenv chroot
- export WBDEV_TARGET=wb6
- ../wbdev chroot autoreconf -fvi
- ../wbdev chroot ./configure
- ../wbdev chroot make

## How to run on wirenboard

- scp zigbee2wb.service root@wirenboard:/etc/systemd/system/
- scp zigbee2wb/zigbee2wb root@wirenboard:/usr/bin/zigbee2wb
- scp zigbee2wb.json root@wirenboard:/etc/zigbee2wb.conf
- ssh root@wirenboard "service zigbee2wb start"
