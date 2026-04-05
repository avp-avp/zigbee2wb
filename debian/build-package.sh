#!/bin/sh
set -eu

PACKAGE_NAME="$(dpkg-parsechangelog -S Source)"
PACKAGE_VERSION="$(dpkg-parsechangelog -S Version)"
OUTPUT_DIR="/work/dist"

mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DIR"/"${PACKAGE_NAME}"_"${PACKAGE_VERSION}"_*.deb \
      "$OUTPUT_DIR"/"${PACKAGE_NAME}"_"${PACKAGE_VERSION}"_*.changes \
      "$OUTPUT_DIR"/"${PACKAGE_NAME}"_"${PACKAGE_VERSION}"_*.buildinfo

dpkg-buildpackage -us -uc -b

cp /"${PACKAGE_NAME}"_"${PACKAGE_VERSION}"_*.deb "$OUTPUT_DIR"/
cp /"${PACKAGE_NAME}"_"${PACKAGE_VERSION}"_*.changes "$OUTPUT_DIR"/
cp /"${PACKAGE_NAME}"_"${PACKAGE_VERSION}"_*.buildinfo "$OUTPUT_DIR"/
