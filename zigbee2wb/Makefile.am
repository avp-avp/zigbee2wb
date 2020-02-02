AM_CPPFLAGS = -I../include/ -g 
bin_PROGRAMS = zigbee2wb
zigbee2wb_SOURCES = zigbee2wb.cpp MqttConnection.cpp
zigbee2wb_LDADD =  $(top_builddir)/libs/libwb/libwb.la  \
	$(top_builddir)/libs/libutils/libutils.la \
	-lmosquitto  -lmosquittopp -lpthread
