/**
 * The Forgotten Server - a free and open-source MMORPG server emulator
 * Copyright (C) 2017  Mark Samman <mark.samman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "otpch.h"

#if defined(__SQLITE__) || defined(__ALLDB__)

#include "databasesqlite.h"
#include <regex>

DatabaseSQLite::DatabaseSQLite()
{
	// connection handle initialization
	if (sqlite3_open(g_config.getString(ConfigManager::SQLITE_DB).c_str(), &handle) != SQLITE_OK) {
		std::cout << std::endl << "Failed to initialize SQLite connection handle." << std::endl;
		sqlite3_close(handle);
		m_connected = false;
	}
	m_connected = true;
}

DatabaseSQLite::~DatabaseSQLite()
{
	sqlite3_close(handle);
}

bool DatabaseSQLite::executeQuery(const std::string& query)
{
	// executes the query
	std::lock_guard<std::recursive_mutex> lock(databaseLock);

	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(handle, query.c_str(), static_cast<int>(query.size()), &stmt, nullptr) != SQLITE_OK) {
		sqlite3_finalize(stmt);
		std::cout << "[Error - sqlite3_prepare] Query: " << query.substr(0, 256) << std::endl << "Message: " << sqlite3_errmsg(handle) << std::endl;
		return false;
	}

	auto rc = sqlite3_step(stmt);
	while (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		std::cout << "[Error - sqlite3_step] Query: " << query.substr(0, 256) << std::endl << "Message: " << sqlite3_errmsg(handle) << std::endl;
		return false;
	}

	sqlite3_finalize(stmt);
	return true;
}

DBResult_ptr DatabaseSQLite::storeQuery(const std::string& query)
{
	std::lock_guard<std::recursive_mutex> lock(databaseLock);

	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(handle, query.c_str(), static_cast<int>(query.size()), &stmt, nullptr) != SQLITE_OK) {
		sqlite3_finalize(stmt);
		std::cout << "[Error - sqlite3_prepare] Query: " << query.substr(0, 256) << std::endl << "Message: " << sqlite3_errmsg(handle) << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	auto rc = sqlite3_step(stmt);
	if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		std::cout << "[Error - sqlite3_step] Query: " << query.substr(0, 256) << std::endl << "Message: " << sqlite3_errmsg(handle) << std::endl;
		return nullptr;
	}

	DBResult_ptr result = std::make_shared<SQLiteDBResult>(stmt, rc == SQLITE_ROW);
	if (!result->hasNext()) {
		return nullptr;
	}
	return result;
}


std::string DatabaseSQLite::getClientVersion() {
	return "SQLite - " + (std::string)SQLITE_VERSION;
}

uint64_t DatabaseSQLite::getLastInsertId()
{
    return static_cast<uint64_t>(sqlite3_last_insert_rowid(handle));
}

std::string DatabaseSQLite::escapeString(const std::string &s) const
{
	// remember about quoiting even an empty string!
	if(!s.size())
		return std::string("''");

	// the worst case is 2n + 3
	char* output = new char[s.length() * 2 + 3];
	// quotes escaped string and frees temporary buffer
	sqlite3_snprintf(s.length() * 2 + 1, output, "%Q", s.c_str());

	std::string r(output);
	delete[] output;

	//escape % and _ because we are using LIKE operator.
	r = std::regex_replace(r, std::regex("%"), "\\%");
	r = std::regex_replace(r, std::regex("_"), "\\_");
	if(r[r.length() - 1] != '\'')
		r += "'";

	return r;
}

std::string DatabaseSQLite::escapeBlob(const char* s, uint32_t length) const
{
	std::string buf = "x'";
	char* hex = new char[2 + 1]; //need one extra byte for null-character
	for(uint32_t i = 0; i < length; i++)
	{
		sprintf(hex, "%02x", ((uint8_t)s[i]));
		buf += hex;
	}

	delete[] hex;
	buf += "'";
	return buf;
}


std::string SQLiteDBResult::getString(const std::string &s) const
{
	auto it = listNames.find(s);
	if (it == listNames.end()) {
		std::cout << "[Error - DBResult::getString] Column '" << s << "' does not exist in result set." << std::endl;
		return std::string();
	}

	const char* data = reinterpret_cast<const char*>(sqlite3_column_text(handle, it->second));
	if (data == nullptr) {
		return std::string();
	}
	return data;
}

const char* SQLiteDBResult::getStream(const std::string &s, size_t &size) const
{
	auto it = listNames.find(s);
	if (it == listNames.end()) {
		std::cout << "[Error - DBResult::getStream] Column '" << s << "' doesn't exist in the result set" << std::endl;
		size = 0;
		return nullptr;
	}

	const char* data = reinterpret_cast<const char*>(sqlite3_column_blob(handle, it->second));
	size = static_cast<unsigned long>(sqlite3_column_bytes(handle, it->second));
	return data;
}

bool SQLiteDBResult::hasNext()
{
	return rowAvailable;
}

bool SQLiteDBResult::next()
{
	rowAvailable = sqlite3_step(handle) == SQLITE_ROW;
	return rowAvailable;
}


SQLiteDBResult::SQLiteDBResult(sqlite3_stmt* res, bool rowAvailable): handle(res), rowAvailable(rowAvailable)
{
	for (int i = 0; i < sqlite3_column_count(handle); ++i) {
		listNames[sqlite3_column_name(handle, i)] = i;
	}
}

SQLiteDBResult::~SQLiteDBResult()
{
	sqlite3_finalize(handle);
}

int64_t SQLiteDBResult::getAnyNumber(std::string const &s) const
{	
	auto it = listNames.find(s);
	if (it == listNames.end()) {
		std::cout << "[Error - DBResult::getNumber] Column '" << s << "' doesn't exist in the result set" << std::endl;
		return 0;
	}
	return sqlite3_column_int64(handle, it->second);
}

#endif
