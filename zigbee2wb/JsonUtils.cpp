#include <limits.h>

#include "JsonUtils.h"

Json::Value ParseJson(const string &text)
{
	Json::CharReaderBuilder builder;
	unique_ptr<Json::CharReader> reader(builder.newCharReader());
	Json::Value root;
	string errors;

	if (!reader->parse(text.data(), text.data() + text.size(), &root, &errors))
		throw CHaException(CHaException::ErrParsingError, "Invalid JSON: %s", errors.c_str());

	return root;
}

string JsonString(const Json::Value &value)
{
	if (value.isNull() || value.isArray() || value.isObject())
		return "";

	return value.asString();
}

int JsonInt(const Json::Value &value, int defaultValue)
{
	if (value.isBool() || !value.isNumeric())
		return defaultValue;

	double number = value.asDouble();
	if (number >= INT_MAX) return INT_MAX;
	if (number <= INT_MIN) return INT_MIN;
	return (int)number;
}

string ToJson(const Json::Value &value)
{
	Json::StreamWriterBuilder builder;
	builder["indentation"] = "";
	builder["precision"] = 10;
	return Json::writeString(builder, value);
}
