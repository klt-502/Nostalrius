#include "easyspawn.h"

#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
#include <utility>
#include <pugixml.hpp>

namespace EasyMode {
namespace {
std::string normalizedName(std::string name)
{
	for (char& c : name) {
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	}
	return name;
}

bool number(pugi::xml_node node, const char* name, uint32_t minimum, uint32_t maximum,
            uint32_t& value, std::string& error, bool optional = false)
{
	const auto attribute = node.attribute(name);
	if (!attribute && optional) return true;
	const std::string text = attribute.value();
	uint64_t parsed = 0;
	if (!text.empty()) {
		for (char digit : text) {
			if (digit < '0' || digit > '9') {
				error = std::string("invalid unsigned integer: ") + name;
				return false;
			}
			parsed = parsed * 10 + (digit - '0');
			if (parsed > maximum) break;
		}
	}
	if (text.empty() || parsed < minimum || parsed > maximum) {
		error = std::string("missing or out-of-range attribute: ") + name;
		return false;
	}
	value = static_cast<uint32_t>(parsed);
	return true;
}

int64_t after(int64_t now, uint32_t delay)
{
	return now > std::numeric_limits<int64_t>::max() - delay ?
	       std::numeric_limits<int64_t>::max() : now + delay;
}
}

bool RespawnManifest::Key::operator<(const Key& other) const
{
	return std::tie(monster, x, y, z) < std::tie(other.monster, other.x, other.y, other.z);
}

bool RespawnManifest::load(const std::string& filename, std::string& error)
{
	// Failure discards every entry, including entries loaded by an earlier call.
	entries.clear();
	error.clear();
	pugi::xml_document doc;
	const auto result = doc.load_file(filename.c_str());
	if (!result) {
		error = result.description();
		return false;
	}
	const auto root = doc.document_element();
	for (auto node = root.next_sibling(); node; node = node.next_sibling()) {
		if (node.type() == pugi::node_element) {
			error = "only one root element is allowed";
			return false;
		}
	}
	if (std::string(root.name()) != "easyspawns" || std::string(root.attribute("version").value()) != "1") {
		error = "expected <easyspawns version=\"1\">";
		return false;
	}
	std::set<std::string> rootAttributes;
	for (const auto attribute : root.attributes()) {
		if (std::string(attribute.name()) != "version" || !rootAttributes.insert(attribute.name()).second) {
			error = "unknown easyspawns attribute";
			return false;
		}
	}
	std::map<Key, Entry> validated;
	const std::set<std::string> attributes = {"monster", "x", "y", "z", "classification",
	    "baseSeconds", "intervalPercent", "minSeconds", "populationFloor"};
	for (const auto node : root.children()) {
		if (std::string(node.name()) != "spawn" || node.first_child()) {
			error = "only empty spawn elements are allowed";
			return false;
		}
		std::set<std::string> seen;
		for (const auto attribute : node.attributes()) {
			if (!attributes.count(attribute.name()) || !seen.insert(attribute.name()).second) {
				error = std::string("unknown or duplicate spawn attribute: ") + attribute.name();
				return false;
			}
		}
		const std::string monster = node.attribute("monster").value();
		if (monster.empty() || monster.front() == ' ' || monster.back() == ' ' ||
		    std::string(node.attribute("classification").value()) != "ordinary") {
			error = "spawn requires an exact monster name and classification=ordinary";
			return false;
		}
		uint32_t x, y, z, seconds;
		Entry entry;
		if (!number(node, "x", 0, 65535, x, error) ||
		    !number(node, "y", 0, 65535, y, error) ||
		    !number(node, "z", 0, 15, z, error) ||
		    !number(node, "baseSeconds", 2, std::numeric_limits<int32_t>::max() / 1000, seconds, error) ||
		    !number(node, "intervalPercent", 1, 100, entry.policy.intervalPercent, error) ||
		    !number(node, "minSeconds", 2, seconds, entry.policy.minimumMs, error) ||
		    !number(node, "populationFloor", 0, 10000, entry.policy.populationFloor, error, true)) {
			return false;
		}
		entry.baseInterval = seconds * 1000;
		entry.policy.minimumMs *= 1000;
		Key key{normalizedName(monster), static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint8_t>(z)};
		if (!validated.emplace(std::move(key), entry).second) {
			error = "duplicate monster/position entry";
			return false;
		}
	}
	entries.swap(validated);
	return true;
}

const RespawnPolicy* RespawnManifest::find(const std::string& monster, uint16_t x, uint16_t y,
                                         uint8_t z, uint32_t baseInterval) const
{
	const auto it = entries.find(Key{normalizedName(monster), x, y, z});
	return it != entries.end() && it->second.baseInterval == baseInterval ? &it->second.policy : nullptr;
}

uint32_t populationRespawnInterval(uint32_t interval, size_t playersOnline)
{
	if (playersOnline > 800) return static_cast<uint64_t>(2) * interval / 5;
	if (playersOnline > 200) return static_cast<uint64_t>(200) * interval / (playersOnline / 2 + 100);
	return interval;
}

void RespawnDeadline::begin(const RespawnPolicy& policy, uint32_t interval, size_t playersOnline,
                            int64_t now, const Random& random)
{
	uint32_t duration = interval;
	if (policy.populationFloor && interval > 500000) {
		duration = populationRespawnInterval(interval, std::max(playersOnline, static_cast<size_t>(policy.populationFloor)));
		duration = random(duration / 2, duration);
	}
	duration = static_cast<uint64_t>(duration) * policy.intervalPercent / 100;
	sampledDelay = std::min(interval, std::max(duration, policy.minimumMs));
	pending = true;
	deadline = after(now, sampledDelay);
}

void RespawnDeadline::postpone(int64_t now)
{
	if (pending) deadline = after(now, sampledDelay);
}

uint32_t RespawnDeadline::nextCheck(int64_t now, uint32_t classicPoll) const
{
	if (!pending) return classicPoll;
	// An obstructed placement retries at most once/second rather than spinning.
	if (now >= deadline) return std::min(classicPoll, static_cast<uint32_t>(1000));
	return static_cast<uint32_t>(std::min(static_cast<uint64_t>(classicPoll), static_cast<uint64_t>(deadline - now)));
}

}
