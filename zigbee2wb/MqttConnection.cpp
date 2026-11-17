#include <algorithm>

#include "MqttConnection.h"

CMqttConnection::CMqttConnection(const CSettings &settings, CLog *log)
	:mosquittopp("Zigbee2WB"), m_Log(log), m_Bridge(settings, this, log), m_bStop(false)
{
	int rc = connect(settings.mqttHost.c_str());
	if (rc)
		m_Log->Printf(0, "mqtt connect to %s failed: %s", settings.mqttHost.c_str(), mosqpp::strerror(rc));

	loop_start();
}

CMqttConnection::~CMqttConnection()
{
	loop_stop(true);
}

void CMqttConnection::on_connect(int rc)
{
	m_Log->Printf(1, "mqtt::on_connect(%d)", rc);
	m_Bridge.onConnect();
}

void CMqttConnection::on_disconnect(int rc)
{
	m_Log->Printf(1, "mqtt::on_disconnect(%d)", rc);
}

void CMqttConnection::on_message(const struct mosquitto_message *message)
{
	string payload;
	if (message->payloadlen && message->payload)
		payload.assign((const char*)message->payload, message->payloadlen);

	m_Bridge.onMessage(message->topic, payload, message->retain);
}

void CMqttConnection::on_log(int level, const char *str)
{
	m_Log->Printf(max(10, level), "mqtt::on_log(%d, %s)", level, str);
}

void CMqttConnection::on_error()
{
	m_Log->Printf(1, "mqtt::on_error()");
}

void CMqttConnection::publish(const string &topic, const string &payload, bool retain)
{
	mosqpp::mosquittopp::publish(NULL, topic.c_str(), payload.length(), payload.c_str(), 1, retain);
}

void CMqttConnection::subscribe(const string &topic)
{
	mosqpp::mosquittopp::subscribe(NULL, topic.c_str());
}

void CMqttConnection::unsubscribe(const string &topic)
{
	mosqpp::mosquittopp::unsubscribe(NULL, topic.c_str());
}
