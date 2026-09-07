// Adapted from OpenAI's retro source code:
// https://github.com/openai/retro

#pragma once

//#include "emulator.h"
#include "memory.h"
//#include "search.h"
#include "utils.h"

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

namespace Retro {

class GameData {
public:
	bool load(const std::string& filename);
	bool load(std::istream* stream);

	bool save(const std::string& filename) const;
	bool save(std::ostream* stream) const;

	void reset();
	void restart();

	static std::string dataPath(const std::string& hint = ".");

	AddressSpace& addressSpace() { return m_mem; }
	const AddressSpace& addressSpace() const { return m_mem; }
	void updateRam();

	void setTypes(const std::vector<DataType> types);
	void setButtons(const std::vector<std::string>& names);
	std::vector<std::string> buttons() const;

	void setActions(const std::vector<std::vector<std::vector<std::string>>>& actions);
	std::map<int, std::set<int>> validActions() const;
	unsigned filterAction(unsigned) const;

	Datum lookupValue(const std::string& name);
	Variant lookupValue(const std::string& name) const;
	//Datum lookupValue(const TypedSearchResult&);
	//int64_t lookupValue(const TypedSearchResult&) const;
	std::unordered_map<std::string, Datum> lookupAll();
	std::unordered_map<std::string, int64_t> lookupAll() const;

	void setValue(const std::string& name, int64_t);
	void setValue(const std::string& name, const Variant&);

	int64_t lookupDelta(const std::string& name) const;

	Variable getVariable(const std::string& name) const;
	void setVariable(const std::string& name, const Variable&);
	void removeVariable(const std::string& name);

	std::unordered_map<std::string, Variable> listVariables() const;
	size_t numVariables() const;

	void search(const std::string& name, int64_t value);
	void deltaSearch(const std::string& name, Operation op, int64_t reference);
	size_t numSearches() const;

	std::vector<std::string> listSearches() const;
	//Search* getSearch(const std::string& name);
	void removeSearch(const std::string& name);

#ifdef USE_CAPNP
	bool loadSearches(const std::string& filename);
	bool saveSearches(const std::string& filename) const;
#endif

private:
	AddressSpace m_mem;
	AddressSpace m_cloneMem;
	AddressSpace m_lastMem;
	std::vector<DataType> m_types;

	std::map<int, std::set<int>> m_actions;
	std::vector<std::string> m_buttons;

	std::unordered_map<std::string, Variable> m_vars;
	//std::unordered_map<std::string, Search> m_searches;
	std::unordered_map<std::string, AddressSpace> m_searchOldMem;
	std::unordered_map<std::string, std::unique_ptr<Variant>> m_customVars;
};
}
