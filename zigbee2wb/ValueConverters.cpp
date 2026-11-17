#include "ValueConverters.h"

void CValueMap::add(const string &zigbeeValue, const string &wbValue)
{
	m_ToWb[zigbeeValue] = wbValue;
	m_ToZigbee[wbValue] = zigbeeValue;
}

static bool Lookup(const string_map &values, const string &key, string &result)
{
	string_map::const_iterator i = values.find(key);
	if (i == values.end())
		return false;

	result = i->second;
	return true;
}

bool CValueMap::toWb(const string &zigbeeValue, string &result) const
{
	return Lookup(m_ToWb, zigbeeValue, result);
}

bool CValueMap::toZigbee(const string &wbValue, string &result) const
{
	return Lookup(m_ToZigbee, wbValue, result);
}

string HectopascalToMmHg(const string &value)
{
	return ftoa(atof(value) / 1.33322387415, 1);
}

string MillivoltToVolt(const string &value)
{
	return ftoa(0.001 * atoi(value), 3);
}

// Units that zigbee2mqtt and the Wiren Board conventions spell the same way.
static const char *COMMON_UNITS[] = {
	"V", "mV", "A", "mA", "W", "kWh", "VA", "var", "kvarh", "Ohm", "mOhm",
	"ppm", "ppb", "lx", "dB", "Hz", "rpm", "s", "min", "h", "m", "g", "kg",
	"Pa", "bar", "mbar", "%", NULL
};

// Units that are spelled differently: zigbee2mqtt name, Wiren Board name.
static const char *UNIT_ALIASES[][2] = {
	{ "\xC2\xB0""C", "deg C" },      // degree sign, UTF-8
	{ "m\xC2\xB3", "m^3" },
	{ "m\xC2\xB3/h", "m^3/h" },
	{ "minutes", "min" },
	{ "meters", "m" },
	{ NULL, NULL }
};

CUnit GetUnit(const string &zigbeeUnit, const string &property)
{
	CUnit unit;

	if (zigbeeUnit == "hPa") {
		// Not a conventional unit, but the usual one for weather in mmHg countries.
		unit.wbUnits = "mmHg";
		unit.converter = HectopascalToMmHg;
	}
	else if (zigbeeUnit == "mV") {
		unit.wbUnits = "V";
		unit.converter = MillivoltToVolt;
	}
	else if (zigbeeUnit == "%" && property.find("humidity") != string::npos) {
		unit.wbUnits = "%, RH";
	}
	else {
		for (int i = 0; UNIT_ALIASES[i][0]; i++) {
			if (zigbeeUnit == UNIT_ALIASES[i][0])
				unit.wbUnits = UNIT_ALIASES[i][1];
		}
		for (int i = 0; COMMON_UNITS[i]; i++) {
			if (zigbeeUnit == COMMON_UNITS[i])
				unit.wbUnits = zigbeeUnit;
		}
	}

	return unit;
}
