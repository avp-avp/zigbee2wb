#include "WbDevice.h"
#include "JsonUtils.h"

CWbControl::CWbControl()
	:type(CWBControl::Text), readonly(true), hasMin(false), hasMax(false),
	min(0), max(0), order(0), changed(false)
{
}

const char *CWbDevice::DriverName()
{
	return PACKAGE_NAME;
}

CWbDevice::CWbDevice(const string &id, const string &title)
	:m_Id(id), m_Title(title), m_Driver(DriverName())
{
}

string CWbDevice::MakeId(const string &name)
{
	string id = name;
	for (size_t i = 0; i < id.size(); i++) {
		// Topic separators and wildcards would break the topic structure.
		if (id[i] == '/' || id[i] == '+' || id[i] == '#')
			id[i] = '_';
	}
	return id;
}

CWbControl &CWbDevice::addControl(const string &name, CWBControl::ControlType type, bool readonly)
{
	CControls::iterator i = m_Controls.find(name);
	if (i != m_Controls.end())
		return i->second;

	CWbControl &control = m_Controls[name];
	control.name = name;
	control.type = type;
	control.readonly = readonly;
	control.order = (int)m_Controls.size();
	return control;
}

bool CWbDevice::hasControl(const string &name) const
{
	return m_Controls.find(name) != m_Controls.end();
}

const CWbControl *CWbDevice::findControl(const string &name) const
{
	CControls::const_iterator i = m_Controls.find(name);
	return i == m_Controls.end() ? NULL : &i->second;
}

CWbControl &CWbDevice::control(const string &name)
{
	CControls::iterator i = m_Controls.find(name);
	if (i == m_Controls.end())
		throw CHaException(CHaException::ErrBadParam, "Device %s has no control %s", m_Id.c_str(), name.c_str());

	return i->second;
}

void CWbDevice::set(const string &name, const string &value)
{
	CWbControl &target = control(name);
	target.value = value;
	target.changed = true;
}

const string &CWbDevice::value(const string &name) const
{
	return const_cast<CWbDevice*>(this)->control(name).value;
}

string CWbDevice::controlTopic(const string &name) const
{
	return "/devices/" + m_Id + "/controls/" + name;
}

static Json::Value Title(const string &text)
{
	Json::Value title(Json::objectValue);
	title["en"] = text;
	return title;
}

void CWbDevice::describe(CMqttMessages &messages)
{
	string base = "/devices/" + m_Id;

	Json::Value deviceMeta(Json::objectValue);
	deviceMeta["driver"] = m_Driver;
	deviceMeta["title"] = Title(m_Title);
	messages.push_back(CMqttMessage(base + "/meta", ToJson(deviceMeta), true));
	// Subtopics duplicate the JSON for software that follows the old conventions.
	messages.push_back(CMqttMessage(base + "/meta/name", m_Title, true));
	messages.push_back(CMqttMessage(base + "/meta/driver", m_Driver, true));

	for_each(CControls, m_Controls, i) {
		CWbControl &control = i->second;
		string topic = controlTopic(control.name);
		string type = CWBControl::getTypeName(control.type);
		bool hasUnits = control.type == CWBControl::Generic && !control.units.empty();

		Json::Value meta(Json::objectValue);
		meta["type"] = type;
		meta["readonly"] = control.readonly;
		meta["order"] = control.order;
		if (hasUnits) meta["units"] = control.units;
		if (control.hasMin) meta["min"] = control.min;
		if (control.hasMax) meta["max"] = control.max;

		messages.push_back(CMqttMessage(topic + "/meta", ToJson(meta), true));
		messages.push_back(CMqttMessage(topic + "/meta/type", type, true));
		messages.push_back(CMqttMessage(topic + "/meta/readonly", control.readonly ? "1" : "0", true));
		messages.push_back(CMqttMessage(topic + "/meta/order", itoa(control.order), true));
		if (hasUnits) messages.push_back(CMqttMessage(topic + "/meta/units", control.units, true));
		if (control.hasMin) messages.push_back(CMqttMessage(topic + "/meta/min", itoa(control.min), true));
		if (control.hasMax) messages.push_back(CMqttMessage(topic + "/meta/max", itoa(control.max), true));

		if (!control.isPushButton() && !control.value.empty()) {
			messages.push_back(CMqttMessage(topic, control.value, true));
			control.changed = false;
		}
	}
}

void CWbDevice::collectChanges(CMqttMessages &messages)
{
	for_each(CControls, m_Controls, i) {
		CWbControl &control = i->second;
		if (!control.changed)
			continue;

		messages.push_back(CMqttMessage(controlTopic(control.name), control.value, !control.isPushButton()));
		control.changed = false;
	}
}
