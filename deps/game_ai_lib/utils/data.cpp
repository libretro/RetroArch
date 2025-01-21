// Adapted from OpenAI's retro source code:
// https://github.com/openai/retro

#include "data.h"

//#include "script.h"
#include "utils.h"

#ifdef ERROR
#undef ERROR
#endif

#include "json.hpp"

#include <fstream>

using namespace Retro;
using namespace std;
using nlohmann::json;

static string s_dataDirectory;

template<typename T>
T find(json::const_reference j, const string& key) {
	const auto& iter = j.find(key);
	if (iter == j.end()) {
		return T();
	}
	try {
		T t = *iter;
		return t;
	} catch (json::exception&) {
		return T();
	}
}

static void setActions(const vector<string>& buttonList, const vector<vector<vector<string>>>& actionsIn, map<int, set<int>>& actions) {
	actions.clear();

	for (const auto& outer : actionsIn) {
		set<int> sublist;
		int mask = 0;
		for (const auto& middle : outer) {
			int buttons = 0;
			for (const auto& button : middle) {
				const auto& iter = find(buttonList.begin(), buttonList.end(), button);
				buttons |= 1 << (iter - buttonList.begin());
			}
			mask |= buttons;
			sublist.insert(buttons);
		}
		actions.emplace(mask, move(sublist));
	}
}

static unsigned filterAction(unsigned action, const map<int, set<int>>& actions) {
	unsigned newAction = 0;
	for (const auto& actionSet : actions) {
		unsigned maskedAction = action & actionSet.first;
		if (actionSet.second.find(maskedAction) != actionSet.second.end()) {
			newAction |= maskedAction;
		}
	}
	return newAction;
}

Variable::Variable(const DataType& type, size_t address, uint64_t mask)
	: type(type)
	, address(address)
	, mask(mask) {
}

bool Variable::operator==(const Variable& other) const {
	return type == other.type && address == other.address && mask == other.mask;
}

bool GameData::load(const string& filename) {
	ifstream file(filename);
	return load(&file);
}

bool GameData::load(istream* file) {
	json manifest;
	try {
		*file >> manifest;
	} catch (json::exception&) {
		return false;
	}

	const auto& info = const_cast<const json&>(manifest).find("info");
	if (info == manifest.cend()) {
		return false;
	}

	unordered_map<std::string, Variable> oldVars;
	oldVars.swap(m_vars);
	for (auto var = info->cbegin(); var != info->cend(); ++var) {
		if (var->find("address") == var->cend() || var->find("type") == var->cend()) {
			oldVars.swap(m_vars);
			return false;
		}
		string dtype = var->at("type");
		if (dtype.size() < 3) {
			continue;
		}
		try {
			Variable v(dtype, var->at("address"), var->value("mask", UINT64_MAX));
			setVariable(var.key(), v);
		} catch (std::out_of_range) {
			continue;
		}
	}
	return true;
}

bool GameData::save(const string& filename) const {
	ofstream file(filename);
	return save(&file);
}

bool GameData::save(ostream* file) const {
	json manifest;
	json info;

	for (const auto& var : m_vars) {
		json jvar;
		jvar["address"] = var.second.address;
		jvar["type"] = var.second.type.type;
		if (var.second.mask != UINT64_MAX) {
			jvar["mask"] = var.second.mask;
		}
		info[var.first] = jvar;
	}

	manifest["info"] = info;
	try {
		file->width(2);
		*file << manifest;
		*file << endl;
	} catch (json::exception&) {
		return false;
	}
	return true;
}

string GameData::dataPath(const string& hint) {
	if (s_dataDirectory.size()) {
		return s_dataDirectory;
	}
	const char* envDir = getenv("RETRO_DATA_PATH");
	if (envDir) {
		s_dataDirectory = envDir;
	} else {
		s_dataDirectory = drillUp({ "retro/data", "data" }, ".", hint);
	}
	return s_dataDirectory;
}

void GameData::reset() {
	restart();
	m_lastMem.reset();
	m_cloneMem.reset();
	m_vars.clear();
	//m_searches.clear();
	m_searchOldMem.clear();
}

void GameData::restart() {
	m_customVars.clear();
}

void GameData::updateRam() {
	m_lastMem = move(m_cloneMem);
	m_cloneMem.clone(m_mem);
}

void GameData::setTypes(const vector<DataType> types) {
	m_types = vector<DataType>(types);
}

void GameData::setButtons(const vector<string>& buttons) {
	m_buttons = buttons;
}

vector<string> GameData::buttons() const {
	return m_buttons;
}

void GameData::setActions(const vector<vector<vector<string>>>& actions) {
	::setActions(m_buttons, actions, m_actions);
}

map<int, set<int>> GameData::validActions() const {
	return m_actions;
}

unsigned GameData::filterAction(unsigned action) const {
	return ::filterAction(action, m_actions);
}

Datum GameData::lookupValue(const string& name) {
	auto variant = m_customVars.find(name);
	if (variant != m_customVars.end()) {
		return Datum(variant->second.get());
	}
	auto v = m_vars.find(name);
	if (v == m_vars.end()) {
		throw invalid_argument(name);
	}
	return m_mem[v->second];
}

Variant GameData::lookupValue(const string& name) const {
	auto variant = m_customVars.find(name);
	if (variant != m_customVars.end()) {
		return *variant->second;
	}
	auto v = m_vars.find(name);
	if (v == m_vars.end()) {
		throw invalid_argument(name);
	}
	return m_mem[v->second];
}

/*Datum GameData::lookupValue(const TypedSearchResult& result) {
	return m_mem[Variable{ result.type, result.address }];
}

int64_t GameData::lookupValue(const TypedSearchResult& result) const {
	return m_mem[Variable{ result.type, result.address }];
}*/

int64_t GameData::lookupDelta(const string& name) const {
	const auto& v = m_vars.find(name);
	if (v == m_vars.end()) {
		return 0;
	}
	int64_t newVal = m_cloneMem[v->second];

	if (!m_lastMem.ok()) {
		return 0;
	}
	int64_t oldVal = m_lastMem[v->second];

	return newVal - oldVal;
}

unordered_map<string, Datum> GameData::lookupAll() {
	unordered_map<string, Datum> data;
	for (auto var = m_vars.cbegin(); var != m_vars.cend(); ++var) {
		try {
			data.emplace(var->first, m_mem[var->second]);
		} catch (...) {
		}
	}
	for (auto var = m_customVars.cbegin(); var != m_customVars.cend(); ++var) {
		data.emplace(var->first, var->second.get());
	}
	return data;
}

unordered_map<string, int64_t> GameData::lookupAll() const {
	unordered_map<string, int64_t> data;
	for (auto var = m_vars.cbegin(); var != m_vars.cend(); ++var) {
		try {
			data.emplace(var->first, m_mem[var->second]);
		} catch (...) {
		}
	}
	for (auto var = m_customVars.cbegin(); var != m_customVars.cend(); ++var) {
		data.emplace(var->first, *var->second);
	}
	return data;
}

void GameData::setValue(const std::string& name, int64_t v) {
	auto variant = m_customVars.find(name);
	if (variant != m_customVars.end()) {
		*variant->second = v;
		return;
	}
	auto var = m_vars.find(name);
	if (var != m_vars.end()) {
		m_mem[var->second] = v;
		return;
	}
	m_customVars.emplace(name, std::make_unique<Variant>(v));
}

void GameData::setValue(const std::string& name, const Variant& v) {
	auto variant = m_customVars.find(name);
	if (variant != m_customVars.end()) {
		*variant->second = v;
		return;
	}
	auto var = m_vars.find(name);
	if (var != m_vars.end()) {
		m_mem[var->second] = v;
		return;
	}
	m_customVars.emplace(name, std::make_unique<Variant>(v));
}

Variable GameData::getVariable(const string& name) const {
	const auto& v = m_vars.find(name);
	if (v == m_vars.end()) {
		throw invalid_argument(name);
	}
	return v->second;
}

void GameData::setVariable(const string& name, const Variable& var) {
	removeVariable(name);
	m_vars.emplace(name, var);
}

void GameData::removeVariable(const string& name) {
	auto iter = m_vars.find(name);
	if (iter != m_vars.end()) {
		m_vars.erase(iter);
	}
}

unordered_map<string, Variable> GameData::listVariables() const {
	return m_vars;
}

size_t GameData::numVariables() const {
	return m_vars.size();
}

/*void GameData::search(const std::string& name, int64_t value) {
	if (m_searches.find(name) == m_searches.cend()) {
		if (m_types.size()) {
			m_searches.emplace(name, Search{ m_types });
		} else {
			m_searches.emplace(name, Search{});
		}
	}
	Search* search = &m_searches[name];
	search->search(m_mem, value);
	m_searchOldMem[name].clone(m_mem);
}*/

/*void GameData::deltaSearch(const std::string& name, Operation op, int64_t reference) {
	if (m_searches.find(name) == m_searches.cend()) {
		if (m_types.size()) {
			m_searches.emplace(name, Search{ m_types });
		} else {
			m_searches.emplace(name, Search{});
		}
	}
	if (m_searchOldMem.find(name) == m_searchOldMem.cend()) {
		m_searchOldMem[name].clone(m_mem);
	}
	Search* search = &m_searches[name];
	search->delta(m_mem, m_searchOldMem[name], op, reference);
	m_searchOldMem[name].clone(m_mem);
}*

size_t GameData::numSearches() const {
	return m_searches.size();
}

vector<string> GameData::listSearches() const {
	vector<string> names;
	for (const auto& search : m_searches) {
		names.emplace_back(search.first);
	}
	return names;
}

Search* GameData::getSearch(const string& name) {
	auto iter = m_searches.find(name);
	if (iter != m_searches.end()) {
		return &iter->second;
	}
	return nullptr;
}

void GameData::removeSearch(const string& name) {
	auto iter = m_searches.find(name);
	if (iter != m_searches.end()) {
		m_searches.erase(iter);
	}
}*/



static const vector<pair<string, Operation>> s_ops{
	make_pair("equal", Operation::EQUAL),
	make_pair("negative-equal", Operation::NEGATIVE_EQUAL),
	make_pair("not-equal", Operation::NOT_EQUAL),
	make_pair("less-than", Operation::LESS_THAN),
	make_pair("greater-than", Operation::GREATER_THAN),
	make_pair("less-or-equal", Operation::LESS_OR_EQUAL),
	make_pair("greater-or-equal", Operation::GREATER_OR_EQUAL),
	make_pair("less-or-equal", Operation::LESS_OR_EQUAL),
	make_pair("nonzero", Operation::NONZERO),
	make_pair("zero", Operation::ZERO),
	make_pair("negative", Operation::NEGATIVE),
	make_pair("positive", Operation::POSITIVE),
	make_pair("sign", Operation::SIGN)
};

