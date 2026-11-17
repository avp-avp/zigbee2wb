// A device in the Wiren Board MQTT conventions
// (https://github.com/wirenboard/conventions): its controls, their metadata
// and the MQTT messages that describe them.
#pragma once
#include "zigbee2wb.h"

struct CMqttMessage
{
	string topic, payload;
	bool retain;

	CMqttMessage(const string &Topic, const string &Payload, bool Retain)
		:topic(Topic), payload(Payload), retain(Retain) {}
};

typedef vector<CMqttMessage> CMqttMessages;

struct CWbControl
{
	string name;
	CWBControl::ControlType type;
	string units;        // published only for the "value" type
	bool readonly;
	bool hasMin, hasMax;
	int min, max;
	int order;           // position in the user interface, starts from 1
	string value;
	bool changed;        // the value was set after it was published last time

	CWbControl();
	// Push buttons are stateless: a press is "1", published without the retained flag.
	bool isPushButton() const { return type == CWBControl::PushButton; }
	void setMin(int value) { min = value; hasMin = true; }
	void setMax(int value) { max = value; hasMax = true; }
};

class CWbDevice
{
	typedef map<string, CWbControl> CControls;

	string m_Id, m_Title, m_Driver;
	CControls m_Controls;

public:
	// The title is what the user sees; the id is used in topics.
	CWbDevice(const string &id, const string &title);

	// Makes a topic-safe device id out of an arbitrary name.
	static string MakeId(const string &name);

	const string &id() const { return m_Id; }
	const string &title() const { return m_Title; }
	// Name of this service, published as the driver of every device it creates.
	static const char *DriverName();

	// Returns the existing control when there is one with this name.
	CWbControl &addControl(const string &name, CWBControl::ControlType type, bool readonly);
	bool hasControl(const string &name) const;
	// NULL when there is no such control.
	const CWbControl *findControl(const string &name) const;
	size_t controlCount() const { return m_Controls.size(); }

	// Both throw CHaException(ErrBadParam) for an unknown control.
	void set(const string &name, const string &value);
	const string &value(const string &name) const;

	// Metadata of the device and of all its controls plus the current values.
	void describe(CMqttMessages &messages);
	// Values set since they were published last time.
	void collectChanges(CMqttMessages &messages);

private:
	string controlTopic(const string &name) const;
	CWbControl &control(const string &name);
};
