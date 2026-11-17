AM_CXXFLAGS = -DWB2IH

# Everything except the MQTT client lives in a convenience library,
# so the unit tests in ../tests can link it without mosquitto.
noinst_LTLIBRARIES = libzigbee2wbcore.la
libzigbee2wbcore_la_SOURCES = zigbee2wb.h \
	JsonUtils.cpp JsonUtils.h \
	ValueConverters.cpp ValueConverters.h \
	Exposes.cpp Exposes.h \
	Settings.cpp Settings.h \
	Topics.cpp Topics.h \
	WbDevice.cpp WbDevice.h \
	ZigbeeDevice.cpp ZigbeeDevice.h \
	Bridge.cpp Bridge.h

bin_PROGRAMS = zigbee2wb
zigbee2wb_SOURCES = zigbee2wb.cpp MqttConnection.cpp MqttConnection.h
zigbee2wb_LDADD = libzigbee2wbcore.la \
	$(top_builddir)/libs/libwb/libwb.la  \
	$(top_builddir)/libs/libutils/libutils.la \
	-lmosquitto  -lmosquittopp -lpthread
