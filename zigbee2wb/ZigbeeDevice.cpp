#include <algorithm>

#include "ZigbeeDevice.h"
#include "JsonUtils.h"

const char *CZigbeeDevice::LAST_SEEN = "lastSeen";
const char *CZigbeeDevice::RAW = "raw";

CZigbeeDevice::CZigbeeDevice(const string &name, const CExposeMap &exposes,
	const CModelTemplate *modelTemplate, const string_vector &actionFields)
	:m_Name(name), m_WbDevice(CWbDevice::MakeId(name), name), m_Exposes(exposes),
	m_ActionFields(actionFields), m_JsonControl(false)
{
	if (modelTemplate) {
		m_Rules = modelTemplate->controls;
		m_JsonControl = modelTemplate->jsonControl;
	}

	m_WbDevice.addControl(LAST_SEEN, CWBControl::Text, true);
	if (m_JsonControl)
		m_WbDevice.addControl(RAW, CWBControl::Text, true);

	for_each_const(CExposeMap, m_Exposes, i) {
		const CExpose &expose = i->second;

		if (expose.isOnOff()) {
			// The on/off payloads announced by the device win over the config.
			CControlRule &rule = m_Rules[expose.name];
			rule.type = CWBControl::Switch;
			rule.valueMap = CValueMap();
			rule.valueMap.add(expose.valueOn, "1");
			rule.valueMap.add(expose.valueOff, "0");
		}
		else if (expose.type == "enum" && isActionField(expose.name)) {
			// Every possible action becomes a push button.
			for_each_const(string_vector, expose.values, value)
				m_WbDevice.addControl(*value, CWBControl::PushButton, true);
		}
	}

	// Set-only values never show up in the state, so they would never get a control.
	for_each_const(CExposeMap, m_Exposes, i) {
		if (i->second.isSettable() && !i->second.isPublished())
			createControl(i->first);
	}
}

const CControlRule *CZigbeeDevice::findRule(const string &control) const
{
	CControlRules::const_iterator i = m_Rules.find(control);
	return i == m_Rules.end() ? NULL : &i->second;
}

const CExpose *CZigbeeDevice::findExpose(const string &name) const
{
	CExposeMap::const_iterator i = m_Exposes.find(name);
	return i == m_Exposes.end() ? NULL : &i->second;
}

bool CZigbeeDevice::isActionField(const string &name) const
{
	return find(m_ActionFields.begin(), m_ActionFields.end(), name) != m_ActionFields.end();
}

bool CZigbeeDevice::createControl(const string &name)
{
	const CExpose *expose = findExpose(name);
	if (!expose)
		return false;

	const CControlRule *rule = findRule(name);
	CWbControl &control = m_WbDevice.addControl(name,
		rule ? rule->type : GuessControlType(*expose), !expose->isSettable());

	if (rule && rule->max > 0)
		control.setMax(rule->max);
	else if (expose->valueMax)
		control.setMax(expose->valueMax);

	if (expose->valueMin)
		control.setMin(expose->valueMin);

	CUnit unit = GetUnit(expose->unit, name);
	control.units = unit.wbUnits;
	if (unit.converter)
		m_UnitConverters[name] = unit.converter;

	return true;
}

string CZigbeeDevice::toWb(const string &control, const string &value) const
{
	string result = value;
	const CControlRule *rule = findRule(control);
	if (rule && rule->valueMap.toWb(value, result))
		return result;

	CUnitConverters::const_iterator converter = m_UnitConverters.find(control);
	if (converter != m_UnitConverters.end())
		return converter->second(value);

	return value;
}

// zigbee2mqtt expects values of the same JSON type it publishes:
// numbers for numeric exposes, the announced payloads for binary ones.
Json::Value CZigbeeDevice::toZigbee(const string &control, const string &value) const
{
	string text = value;
	const CControlRule *rule = findRule(control);
	if (rule)
		rule->valueMap.toZigbee(value, text);

	const CExpose *expose = findExpose(control);
	if (expose && expose->isOnOff()) {
		if (text == expose->valueOn)  return expose->jsonOn;
		if (text == expose->valueOff) return expose->jsonOff;
	}

	if (expose && expose->type == "numeric" && !text.empty()) {
		char *end = NULL;
		double number = strtod(text.c_str(), &end);
		if (*end == 0) {
			if (number == (double)(Json::Int64)number)
				return Json::Value((Json::Int64)number);
			return Json::Value(number);
		}
	}

	return Json::Value(text);
}

bool CZigbeeDevice::buildSetPayload(const string &control, const string &value, string &payload) const
{
	const CWbControl *target = m_WbDevice.findControl(control);
	if (!target || target->readonly)
		return false;

	Json::Value request(Json::objectValue);
	request[control] = toZigbee(control, value);
	payload = ToJson(request);
	return true;
}

bool CZigbeeDevice::applyState(const Json::Value &state, const string &rawPayload, long long nowMs)
{
	bool controlsAdded = false;

	if (m_JsonControl)
		m_WbDevice.set(RAW, rawPayload);

	Json::Value::Members names;
	if (state.isObject())
		names = state.getMemberNames();

	for_each_const(Json::Value::Members, names, i) {
		const string &name = *i;
		const Json::Value &json = state[name];

		if (json.isArray() || json.isObject())
			continue;

		if (!m_WbDevice.hasControl(name)) {
			if (!createControl(name))
				continue;
			controlsAdded = true;
		}

		string value = toWb(name, JsonString(json));
		m_WbDevice.set(name, value);

		// An action presses the push button named after it.
		const CWbControl *button = isActionField(name) ? m_WbDevice.findControl(value) : NULL;
		if (button && button->isPushButton())
			m_WbDevice.set(value, "1");
	}

	// zigbee2mqtt adds "last_seen" when the advanced.last_seen option is enabled.
	string lastSeen = state.isObject() ? JsonString(state["last_seen"]) : "";
	if (lastSeen.empty()) {
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%lld", nowMs);
		lastSeen = buffer;
	}
	m_WbDevice.set(LAST_SEEN, lastSeen);

	return controlsAdded;
}
