// Safe helpers around jsoncpp: none of them throws jsoncpp exceptions.
#pragma once
#include "zigbee2wb.h"

// Parses a JSON document. Throws CHaException(ErrParsingError) on malformed input.
Json::Value ParseJson(const string &text);

// String form of a scalar value ("true"/"false" for booleans);
// empty string for null, arrays and objects.
string JsonString(const Json::Value &value);

// Compact single-line JSON text.
string ToJson(const Json::Value &value);

// Integer form of a numeric value, clamped to the int range;
// defaultValue for anything that is not a number.
int JsonInt(const Json::Value &value, int defaultValue = 0);
