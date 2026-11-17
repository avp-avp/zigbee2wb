#include "TestHelpers.h"
#include "../zigbee2wb/Exposes.h"

static CExposeMap Parse(const string &json)
{
	CExposeMap result;
	ParseExposes(ParseJson(json), result);
	return result;
}

static CExpose Expose(const string &name, const string &type, int access = 1, int valueMax = 0)
{
	CExpose expose;
	expose.name = name;
	expose.type = type;
	expose.access = access;
	expose.valueMax = valueMax;
	return expose;
}

TEST_SUITE("Exposes") {

TEST_CASE("a simple expose is parsed with all its attributes") {
	CExposeMap exposes = Parse(
		"[{\"type\":\"numeric\",\"property\":\"battery\",\"name\":\"battery\","
		"\"access\":1,\"unit\":\"%\",\"value_min\":5,\"value_max\":100}]");

	REQUIRE(exposes.size() == 1);
	const CExpose &battery = exposes["battery"];
	CHECK(battery.name == "battery");
	CHECK(battery.type == "numeric");
	CHECK(battery.unit == "%");
	CHECK(battery.access == 1);
	CHECK(battery.valueMin == 5);
	CHECK(battery.valueMax == 100);
	CHECK_FALSE(battery.isSettable());
	CHECK(battery.isPublished());
	CHECK_FALSE(battery.isOnOff());
}

TEST_CASE("a composite expose is replaced by its features") {
	CExposeMap exposes = Parse(
		"[{\"type\":\"light\",\"features\":["
		"{\"type\":\"binary\",\"property\":\"state\",\"access\":7,\"value_on\":\"ON\",\"value_off\":\"OFF\"},"
		"{\"type\":\"numeric\",\"property\":\"brightness\",\"access\":7,\"value_max\":254}]}]");

	REQUIRE(exposes.size() == 2);
	CHECK(exposes["state"].type == "binary");
	CHECK(exposes["state"].isOnOff());
	CHECK(exposes["state"].isSettable());
	CHECK(exposes["brightness"].valueMax == 254);
}

TEST_CASE("a composite expose with its own property is kept as a single value") {
	CExposeMap exposes = Parse(
		"[{\"type\":\"composite\",\"property\":\"color\",\"features\":["
		"{\"type\":\"numeric\",\"property\":\"x\"},{\"type\":\"numeric\",\"property\":\"y\"}]}]");

	REQUIRE(exposes.size() == 1);
	CHECK(exposes.count("color") == 1);
}

TEST_CASE("enum values are collected") {
	CExposeMap exposes = Parse(
		"[{\"type\":\"enum\",\"property\":\"action\",\"access\":1,\"values\":[\"single\",\"double\"]}]");

	REQUIRE(exposes["action"].values.size() == 2);
	CHECK(exposes["action"].values[0] == "single");
	CHECK(exposes["action"].values[1] == "double");
}

TEST_CASE("boolean on/off payloads are kept as text") {
	CExposeMap exposes = Parse(
		"[{\"type\":\"binary\",\"property\":\"occupancy\",\"access\":1,\"value_on\":true,\"value_off\":false}]");

	CHECK(exposes["occupancy"].valueOn == "true");
	CHECK(exposes["occupancy"].valueOff == "false");
	CHECK(exposes["occupancy"].isOnOff());
	CHECK(exposes["occupancy"].jsonOn.isBool());
	CHECK(exposes["occupancy"].jsonOn.asBool());
}

TEST_CASE("access bits") {
	CHECK(Expose("effect", "enum", 2).isSettable());
	CHECK_FALSE(Expose("effect", "enum", 2).isPublished());
	CHECK(Expose("power", "numeric", 5).isPublished());
	CHECK_FALSE(Expose("power", "numeric", 5).isSettable());
	CHECK(Expose("state", "binary", 7).isSettable());
}

TEST_CASE("isOnOff requires a binary type and both payloads") {
	CExpose expose = Expose("state", "binary");
	CHECK_FALSE(expose.isOnOff());
	expose.valueOn = "ON";
	CHECK_FALSE(expose.isOnOff());
	expose.valueOff = "OFF";
	CHECK(expose.isOnOff());
	expose.type = "enum";
	CHECK_FALSE(expose.isOnOff());
}

TEST_CASE("entries without a property are skipped") {
	CExposeMap exposes = Parse(
		"[{\"type\":\"numeric\"}, 42, null,"
		"{\"type\":\"switch\",\"features\":[{\"type\":\"binary\"}, \"junk\"]}]");
	CHECK(exposes.empty());
}

TEST_CASE("a missing or malformed exposes list gives no exposes") {
	CHECK(Parse("null").empty());
	CHECK(Parse("{}").empty());
	CHECK(Parse("\"text\"").empty());
}

TEST_CASE("a huge value_max does not break parsing") {
	CExposeMap exposes = Parse("[{\"type\":\"numeric\",\"property\":\"energy\",\"value_max\":4294967295}]");
	CHECK(exposes["energy"].valueMax == INT_MAX);
}

TEST_CASE("binary values are switches") {
	CHECK(GuessControlType(Expose("occupancy", "binary")) == CWBControl::Switch);
	CHECK(GuessControlType(Expose("state", "binary", 7)) == CWBControl::Switch);
}

TEST_CASE("numeric values are generic values, whatever they measure") {
	CHECK(GuessControlType(Expose("temperature", "numeric")) == CWBControl::Generic);
	CHECK(GuessControlType(Expose("humidity", "numeric")) == CWBControl::Generic);
	CHECK(GuessControlType(Expose("voltage", "numeric")) == CWBControl::Generic);
	CHECK(GuessControlType(Expose("pressure", "numeric")) == CWBControl::Generic);
	CHECK(GuessControlType(Expose("power", "numeric", 5)) == CWBControl::Generic);
}

TEST_CASE("a numeric value is a range only when it is settable and limited") {
	CHECK(GuessControlType(Expose("brightness", "numeric", 7, 254)) == CWBControl::Range);
	CHECK(GuessControlType(Expose("countdown", "numeric", 3, 43200)) == CWBControl::Range);
	CHECK(GuessControlType(Expose("battery", "numeric", 1, 100)) == CWBControl::Generic);
	CHECK(GuessControlType(Expose("linkquality", "numeric", 1, 255)) == CWBControl::Generic);
	CHECK(GuessControlType(Expose("calibration", "numeric", 3, 0)) == CWBControl::Generic);
}

TEST_CASE("everything else is text") {
	CHECK(GuessControlType(Expose("state", "enum", 3)) == CWBControl::Text);
	CHECK(GuessControlType(Expose("effect", "enum", 2)) == CWBControl::Text);
	CHECK(GuessControlType(Expose("schedule", "text")) == CWBControl::Text);
	CHECK(GuessControlType(Expose("anything", "")) == CWBControl::Text);
}

}
