#include "TestHelpers.h"
#include "../zigbee2wb/ValueConverters.h"

TEST_SUITE("ValueConverters") {

TEST_CASE("CValueMap converts in both directions") {
	CValueMap values;
	CHECK(values.empty());
	values.add("ON", "1");
	values.add("OFF", "0");
	CHECK_FALSE(values.empty());

	string result;
	CHECK(values.toWb("ON", result));
	CHECK(result == "1");
	CHECK(values.toZigbee("0", result));
	CHECK(result == "OFF");
}

TEST_CASE("CValueMap leaves the result untouched for unknown values") {
	CValueMap values;
	values.add("ON", "1");

	string result = "unchanged";
	CHECK_FALSE(values.toWb("TOGGLE", result));
	CHECK_FALSE(values.toZigbee("2", result));
	CHECK(result == "unchanged");
}

TEST_CASE("CValueMap supports the same variable as input and output") {
	CValueMap values;
	values.add("OPEN", "up");

	string value = "OPEN";
	CHECK(values.toWb(value, value));
	CHECK(value == "up");
}

TEST_CASE("hPa is converted to mmHg") {
	CHECK(HectopascalToMmHg("1013.25") == "760.0");
	CHECK(HectopascalToMmHg("1000") == "750.1");
}

TEST_CASE("mV is converted to V") {
	CHECK(MillivoltToVolt("3025") == "3.025");
	CHECK(MillivoltToVolt("3000") == "3.000");
	CHECK(MillivoltToVolt("0") == "0.000");
}

TEST_CASE("units with the same name in both systems pass through") {
	const char *units[] = { "V", "A", "W", "kWh", "ppm", "lx", "s", "rpm", "Hz", "%" };
	for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); i++) {
		CAPTURE(units[i]);
		CUnit unit = GetUnit(units[i], "anything");
		CHECK(unit.wbUnits == units[i]);
		CHECK((unit.converter == NULL));
	}
}

TEST_CASE("units are renamed to the Wiren Board spelling") {
	CHECK(GetUnit("\xC2\xB0""C", "temperature").wbUnits == "deg C");
	CHECK(GetUnit("m\xC2\xB3", "volume").wbUnits == "m^3");
	CHECK(GetUnit("minutes", "fading_time").wbUnits == "min");
	CHECK(GetUnit("meters", "distance").wbUnits == "m");
}

TEST_CASE("percent of humidity is relative humidity") {
	CHECK(GetUnit("%", "humidity").wbUnits == "%, RH");
	CHECK(GetUnit("%", "humidity_2").wbUnits == "%, RH");
	CHECK(GetUnit("%", "battery").wbUnits == "%");
}

TEST_CASE("pressure and millivolts are converted") {
	CUnit pressure = GetUnit("hPa", "pressure");
	CHECK(pressure.wbUnits == "mmHg");
	CHECK((pressure.converter == &HectopascalToMmHg));

	CUnit voltage = GetUnit("mV", "voltage");
	CHECK(voltage.wbUnits == "V");
	CHECK((voltage.converter == &MillivoltToVolt));
}

TEST_CASE("units unknown to Wiren Board are dropped") {
	CHECK(GetUnit("lqi", "linkquality").wbUnits == "");
	CHECK(GetUnit("mired", "color_temp").wbUnits == "");
	CHECK(GetUnit("", "state").wbUnits == "");
	CHECK((GetUnit("lqi", "linkquality").converter == NULL));
}

}
