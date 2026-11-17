// Interpretation of zigbee2mqtt "exposes": the capabilities a device announces
// in bridge/devices (https://www.zigbee2mqtt.io/guide/usage/exposes.html).
#pragma once
#include "zigbee2wb.h"

struct CExpose
{
	enum Access { AccessState = 1, AccessSet = 2, AccessGet = 4 };

	string name;   // key of the value in the device state payload ("property")
	string type;   // numeric, binary, enum, text, ...
	string unit;
	int access;
	int valueMin, valueMax;     // 0 when not announced
	string valueOn, valueOff;   // binary only, as text
	Json::Value jsonOn, jsonOff;   // binary only, as announced ("ON" or true)
	string_vector values;       // enum only

	CExpose();
	bool isSettable() const { return (access & AccessSet) != 0; }
	// False for values that can only be set and never appear in the device state.
	bool isPublished() const { return (access & AccessState) != 0; }
	// A binary value with announced on/off payloads.
	bool isOnOff() const;
};

typedef map<string, CExpose> CExposeMap;

// Flattens the "exposes" array of a device definition: composite exposes
// (switch, light, cover, ...) are replaced by their features.
// Entries without a property name are skipped.
void ParseExposes(const Json::Value &exposes, CExposeMap &result);

// Wiren Board control type for an expose that has no explicit rule in the config.
CWBControl::ControlType GuessControlType(const CExpose &expose);
