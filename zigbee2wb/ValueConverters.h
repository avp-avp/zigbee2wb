// Value conversion between zigbee2mqtt and Wiren Board representations.
#pragma once
#include "zigbee2wb.h"

// Bidirectional dictionary of textual values, e.g. "ON" <-> "1".
class CValueMap
{
	string_map m_ToWb, m_ToZigbee;

public:
	void add(const string &zigbeeValue, const string &wbValue);
	bool empty() const { return m_ToWb.empty(); }

	// Both return false and leave `result` untouched when there is no mapping.
	bool toWb(const string &zigbeeValue, string &result) const;
	bool toZigbee(const string &wbValue, string &result) const;
};

// Converts a numeric zigbee2mqtt value to the unit used by Wiren Board.
typedef string (*UnitConverterFunc)(const string &value);

string HectopascalToMmHg(const string &value);
string MillivoltToVolt(const string &value);

// How a zigbee2mqtt unit is shown in Wiren Board.
struct CUnit
{
	string wbUnits;                // value of the "units" meta; empty when there is no match
	UnitConverterFunc converter;   // NULL when the value is published as is

	CUnit() :converter(NULL) {}
};

// Maps a zigbee2mqtt unit (degrees Celsius, "hPa", "mV", ...) of the given property to the
// units of the Wiren Board MQTT conventions.
CUnit GetUnit(const string &zigbeeUnit, const string &property);
