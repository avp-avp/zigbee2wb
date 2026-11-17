#include "Exposes.h"
#include "JsonUtils.h"

CExpose::CExpose()
	:access(0), valueMin(0), valueMax(0)
{
}

bool CExpose::isOnOff() const
{
	return (type == "switch" || type == "binary") && valueOn != "" && valueOff != "";
}

static void AddExpose(const Json::Value &json, CExposeMap &result)
{
	CExpose expose;
	expose.name = JsonString(json["property"]);
	if (expose.name.empty())
		return;

	expose.type     = JsonString(json["type"]);
	expose.unit     = JsonString(json["unit"]);
	expose.access   = JsonInt(json["access"]);
	expose.valueMin = JsonInt(json["value_min"]);
	expose.valueMax = JsonInt(json["value_max"]);
	expose.valueOn  = JsonString(json["value_on"]);
	expose.valueOff = JsonString(json["value_off"]);
	expose.jsonOn   = json["value_on"];
	expose.jsonOff  = json["value_off"];

	const Json::Value &values = json["values"];
	if (values.isArray()) {
		for (Json::ArrayIndex i = 0; i < values.size(); i++)
			expose.values.push_back(JsonString(values[i]));
	}

	result[expose.name] = expose;
}

void ParseExposes(const Json::Value &exposes, CExposeMap &result)
{
	if (!exposes.isArray())
		return;

	for (Json::ArrayIndex i = 0; i < exposes.size(); i++) {
		const Json::Value &expose = exposes[i];
		if (!expose.isObject())
			continue;

		const Json::Value &features = expose["features"];
		if (!JsonString(expose["property"]).empty() || !features.isArray()) {
			AddExpose(expose, result);
			continue;
		}

		for (Json::ArrayIndex feature = 0; feature < features.size(); feature++) {
			if (features[feature].isObject())
				AddExpose(features[feature], result);
		}
	}
}

CWBControl::ControlType GuessControlType(const CExpose &expose)
{
	if (expose.type == "binary")
		return CWBControl::Switch;

	if (expose.type == "numeric") {
		// A slider makes sense only for a value that can be changed and has a limit.
		// Everything else is a generic value; what it measures is told by its units.
		return expose.isSettable() && expose.valueMax > 0 ? CWBControl::Range : CWBControl::Generic;
	}

	return CWBControl::Text;
}
