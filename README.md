# zigbee2wb
Convert zigbee2mqtt mqtt notation to wirenboard notation

## How to build

- git clone git@github.com:avp-avp/zigbee2wb.git
- git submodule init  
- git submodule update
- autoreconf -fvi
- ./configure
- make

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
