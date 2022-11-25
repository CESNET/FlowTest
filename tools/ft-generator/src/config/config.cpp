#include "config.h"

namespace generator {
namespace config {

Config::Config(const YAML::Node& node)
{
	(void) node;
}

Config Config::LoadFromFile(const std::string& configFilename)
{
	YAML::Node node = YAML::LoadFile(configFilename);
	return Config(node);
}


} // namespace config
} // namespace generator
