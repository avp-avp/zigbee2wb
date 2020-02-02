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
- ../devenv chroot autoreconf -fvi
- ../devenv chroot ./configure
- ../devenv chroot make
