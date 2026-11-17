#include <algorithm>

#include "Settings.h"

CControlRule::CControlRule()
	:type(CWBControl::Error), max(-1)
{
}

CModelTemplate::CModelTemplate()
	:jsonControl(false)
{
}

CSettings::CSettings()
	:mqttHost("wirenboard"), baseTopic("zigbee2mqtt")
{
}

static void LoadConverters(const CConfigItem &config, CValueMaps &converters)
{
	configValues items;
	config.getNode("converters").getValues(items);

	for_each_const(configValues, items, item) {
		configValues values;
		item->second.getValues(values);
		for_each_const(configValues, values, value)
			converters[item->first].add(value->first, value->second.getStr(""));
	}
}

static CControlRule LoadControlRule(const CConfigItem &config, const CValueMaps &converters)
{
	CControlRule rule;
	string typeName = config.getStr("type");
	rule.type = CWBControl::getType(typeName);
	if (rule.type == CWBControl::Error)
		throw CHaException(CHaException::ErrBadParam, "Unknown type: %s", typeName.c_str());

	rule.max = config.getInt("max", false, -1);

	string converter = config.getStr("converter", false, "");
	if (converter != "") {
		CValueMaps::const_iterator i = converters.find(converter);
		if (i == converters.end())
			throw CHaException(CHaException::ErrBadParam, "Unknown converter: %s", converter.c_str());
		rule.valueMap = i->second;
	}

	return rule;
}

static void LoadModels(const CConfigItem &config, const CValueMaps &converters, CModelTemplates &models)
{
	configValues items;
	config.getNode("models").getValues(items);

	for_each_const(configValues, items, item) {
		CModelTemplate &model = models[item->first];
		model.jsonControl = item->second.getInt("json_template", false, false) != 0;

		configValues controls;
		item->second.getNode("controls").getValues(controls);
		for_each_const(configValues, controls, control)
			model.controls[control->first] = LoadControlRule(control->second, converters);
	}
}

CSettings CSettings::Load(const CConfigItem &config)
{
	CSettings settings;
	settings.mqttHost  = config.getStr("mqtt/host", false, settings.mqttHost);
	settings.baseTopic = config.getStr("mqtt/base_topic", false, settings.baseTopic);
	settings.actionFields.push_back(config.getStr("general/action_field", false, "action"));

	LoadConverters(config, settings.converters);
	LoadModels(config, settings.converters, settings.models);
	return settings;
}

bool CSettings::isActionField(const string &name) const
{
	return find(actionFields.begin(), actionFields.end(), name) != actionFields.end();
}

const CModelTemplate *CSettings::findModel(const string &modelId) const
{
	CModelTemplates::const_iterator i = models.find(modelId);
	return i == models.end() ? NULL : &i->second;
}
