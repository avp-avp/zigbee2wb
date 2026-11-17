# Unit tests, run with `make check`.
AM_CPPFLAGS = -DTEST_DATA_DIR=\"$(abs_srcdir)/data\"

check_PROGRAMS = unit_tests
TESTS = $(check_PROGRAMS)

unit_tests_SOURCES = test_main.cpp TestHelpers.h doctest.h \
	test_json_utils.cpp \
	test_value_converters.cpp \
	test_exposes.cpp \
	test_settings.cpp \
	test_topics.cpp \
	test_wb_device.cpp \
	test_zigbee_device.cpp \
	test_bridge.cpp \
	test_refresh.cpp

unit_tests_LDADD = $(top_builddir)/zigbee2wb/libzigbee2wbcore.la \
	$(top_builddir)/libs/libwb/libwb.la \
	$(top_builddir)/libs/libutils/libutils.la \
	-lpthread

EXTRA_DIST = data/bridge_devices.json
