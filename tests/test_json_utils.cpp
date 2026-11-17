#include "TestHelpers.h"

TEST_SUITE("JsonUtils") {

TEST_CASE("ParseJson parses a document") {
	Json::Value value = ParseJson("{\"a\": 1, \"b\": [true, \"x\"]}");
	CHECK(value["a"].asInt() == 1);
	CHECK(value["b"].size() == 2);
}

TEST_CASE("ParseJson throws CHaException on malformed input") {
	CHECK_THROWS_AS(ParseJson("{\"a\": "), CHaException);
	CHECK_THROWS_AS(ParseJson("online"), CHaException);
	CHECK_THROWS_AS(ParseJson(""), CHaException);
}

TEST_CASE("JsonString converts scalars") {
	CHECK(JsonString(Json::Value("text")) == "text");
	CHECK(JsonString(Json::Value(42)) == "42");
	CHECK(JsonString(Json::Value(21.5)) == "21.5");
	CHECK(JsonString(Json::Value(true)) == "true");
	CHECK(JsonString(Json::Value(false)) == "false");
}

TEST_CASE("JsonString is empty for null, arrays and objects") {
	CHECK(JsonString(Json::Value()) == "");
	CHECK(JsonString(Json::Value(Json::arrayValue)) == "");
	CHECK(JsonString(ParseJson("{\"a\": 1}")) == "");
}

TEST_CASE("ToJson writes compact JSON") {
	Json::Value value(Json::objectValue);
	value["b"] = true;
	value["a"] = 12.5;
	value["c"] = "x";
	CHECK(ToJson(value) == "{\"a\":12.5,\"b\":true,\"c\":\"x\"}");
	CHECK(ToJson(Json::Value(0.1)) == "0.1");
}

TEST_CASE("JsonInt converts numbers") {
	CHECK(JsonInt(Json::Value(7)) == 7);
	CHECK(JsonInt(Json::Value(-3)) == -3);
	CHECK(JsonInt(Json::Value(2.9)) == 2);
}

TEST_CASE("JsonInt falls back to the default for non-numbers") {
	CHECK(JsonInt(Json::Value()) == 0);
	CHECK(JsonInt(Json::Value(), 5) == 5);
	CHECK(JsonInt(Json::Value("12"), 5) == 5);
	CHECK(JsonInt(Json::Value(true), 5) == 5);
}

TEST_CASE("JsonInt clamps values outside the int range") {
	CHECK(JsonInt(ParseJson("4294967295")) == INT_MAX);
	CHECK(JsonInt(ParseJson("-1e30")) == INT_MIN);
}

}
