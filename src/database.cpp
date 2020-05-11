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
#include "database.h"

#if !defined(__ALLDB__)
	#if !defined(__MYSQL__) && !defined(__PGSQL__) && !defined(__SQLITE__)
		#error You must define one Database.
	#endif
// #else
// 	#if defined(__MYSQL__) && defined(__PGSQL__) 
// 		#error To all database use __ALLDB__ in preprocessor.
// 	#endif	
#endif

#if defined(__MYSQL__) || defined(__ALLDB__)
	#include "databasemysql.h"
#endif

#if defined(__PGSQL__) || defined(__ALLDB__)
	#include "databasepgsql.h"
#endif

#if defined(__SQLITE__) || defined(__ALLDB__)
	#include "databasesqlite.h"
#endif

Database& Database::getInstance() 		
{
#if defined(__MYSQL__) || defined(__ALLDB__)
	if (g_config.getString(ConfigManager::SQL_TYPE) == "mysql") {
		static DatabaseMYsql instanceMY;		
		return instanceMY;
	}
#endif
	if (g_config.getString(ConfigManager::SQL_TYPE) == "odbc") {
	}
#if defined(__SQLITE__) || defined(__ALLDB__)	
	if (g_config.getString(ConfigManager::SQL_TYPE) == "sqlite") {
		static DatabaseSQLite instanceSQLite;
		return instanceSQLite;
	}
#endif
#if defined(__PGSQL__) || defined(__ALLDB__)
	if (g_config.getString(ConfigManager::SQL_TYPE) == "pgsql") {
		static DatabasePGsql instancePG;
		return instancePG;
	}
#endif
	std::cout << "Database with Incorrect name in config.lua. Or not compiled to this database.\n";
	exit(EXIT_FAILURE);
	//	wow this is loop. but will never touch =)
	return Database::getInstance();
}

bool Database::beginTransaction()
{
	return executeQuery("BEGIN");
}

bool Database::rollback()
{
	return executeQuery("ROLLBACK");
}

bool Database::commit()
{
	return executeQuery("COMMIT");
}

/***************/

DBInsert::DBInsert(std::string query) : query(query)
{
	this->length = this->query.length();
}

bool DBInsert::addRow(const std::string& row)
{
	
	// adds new row to buffer
	const size_t rowLength = row.length();
	length += rowLength;	
	if (length > Database::getInstance().getMaxPacketSize() && !execute()) {
		return false;
	}

	if (values.empty()) {
		values.reserve(rowLength + 2);
		values.push_back('(');
		values.append(row);
		values.push_back(')');
	}
	else {
		values.reserve(values.length() + rowLength + 3);
		values.push_back(',');
		values.push_back('(');
		values.append(row);
		values.push_back(')');
	}
	return true;
}

bool DBInsert::addRow(std::ostringstream& row)
{
	bool ret = addRow(row.str());
	row.str(std::string());
	return ret;
}

bool DBInsert::execute()
{
	if (values.empty()) {
		return true;
	}

	// executes buffer
	bool res = Database::getInstance().executeQuery(query + values);
	values.clear();
	length = query.length();
	return res;
}
