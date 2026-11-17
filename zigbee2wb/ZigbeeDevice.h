// One zigbee2mqtt device mirrored as a Wiren Board device.
#pragma once
#include "zigbee2wb.h"
#include "Exposes.h"
#include "Settings.h"
#include "WbDevice.h"

class CZigbeeDevice
{
	typedef map<string, UnitConverterFunc> CUnitConverters;

	string m_Name;
	CWbDevice m_WbDevice;
	CExposeMap m_Exposes;
	CControlRules m_Rules;
	CUnitConverters m_UnitConverters;
	string_vector m_ActionFields;
	bool m_JsonControl;

public:
	static const char *LAST_SEEN;   // control with the time of the last message
	static const char *RAW;         // control with the whole state payload

	// name is the zigbee2mqtt friendly name; modelTemplate may be NULL.
	// Controls for the values a device reports are not created here: each one
	// appears when the value is reported for the first time.
	CZigbeeDevice(const string &name, const CExposeMap &exposes,
		const CModelTemplate *modelTemplate, const string_vector &actionFields);

	const string &name() const { return m_Name; }
	CWbDevice &wbDevice() { return m_WbDevice; }

	// Applies a state payload published by zigbee2mqtt. nowMs is stored as the
	// last seen time unless the payload carries its own "last_seen".
	// Returns true when new controls appeared and the device has to be described again.
	bool applyState(const Json::Value &state, const string &rawPayload, long long nowMs);

	// Builds the zigbee2mqtt "set" payload for a value written to a Wiren Board
	// control. Returns false when the control cannot be written.
	bool buildSetPayload(const string &control, const string &value, string &payload) const;

private:
	const CControlRule *findRule(const string &control) const;
	const CExpose *findExpose(const string &name) const;
	bool isActionField(const string &name) const;
	// Returns false when the device does not expose such a value.
	bool createControl(const string &name);
	string toWb(const string &control, const string &value) const;
	Json::Value toZigbee(const string &control, const string &value) const;
};
