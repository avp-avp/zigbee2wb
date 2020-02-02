# zigbee2wb
Convert zigbee2mqtt mqtt notation to wirenboard notation

## How to build

- git submodule init  
- git submodule update
- autoreconf -fvi
- ./configure
- make

## How to build for wirenboard

You need Wirenboard development env from https://github.com/contactless/wirenboard 

Run bould command with devenv chroot
- export WBDEV_TARGET=wb6
- ../devenv chroot autoreconf -fvi
- ../devenv chroot ./configure
- ../devenv chroot make

## How to run on wirenboard

- scp zigbee2wb.service root@wirenboard:/etc/systemd/system/
- scp zigbee2wb/zigbee2wb root@wirenboard:/usr/bin/zigbee2wb
- scp zigbee2wb.json root@wirenboard:/etc/zigbee2wb.conf
- ssh root@wirenboard "service zigbee2wb start"
