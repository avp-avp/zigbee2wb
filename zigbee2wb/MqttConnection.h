// MQTT client: a thin adapter between libmosquittopp and CBridge.
#pragma once

//#include <algorithm>

#include "zigbee2wb.h"
#include "Bridge.h"

#if __has_include(<mosquitto/libmosquittopp.h>)
	#include <mosquitto/libmosquittopp.h>  // Mosquitto 2.1+
#elif __has_include(<mosquittopp.h>)
	#include <mosquittopp.h>               // Mosquitto 2.0
#else
	#error "Mosquitto C++ development headers are not installed"
#endif

class CMqttConnection
	:public mosqpp::mosquittopp, private IMqttTransport
{
	CLog *m_Log;
	CBridge m_Bridge;
	bool m_bStop;

public:
	CMqttConnection(const CSettings &settings, CLog *log);
	~CMqttConnection();
	bool isStopped() { return m_bStop; }

private:
	// mosquittopp callbacks
	virtual void on_connect(int rc);
	virtual void on_disconnect(int rc);
	virtual void on_message(const struct mosquitto_message *message);
	virtual void on_log(int level, const char *str);
	virtual void on_error();

	// IMqttTransport
	virtual void publish(const string &topic, const string &payload, bool retain);
	virtual void subscribe(const string &topic);
	virtual void unsubscribe(const string &topic);
};
