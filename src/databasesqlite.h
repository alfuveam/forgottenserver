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

#if defined(__SQLITE__) || defined(__ALLDB__)

#ifndef DATABASESQLITE_H
#define DATABASESQLITE_H

#include "database.h"
#include <sqlite3.h>

class DatabaseSQLite : public Database
{
public:
	DatabaseSQLite();
	virtual ~DatabaseSQLite();
	bool executeQuery(const std::string& query);
	DBResult_ptr storeQuery(const std::string& query);
	std::string escapeString(const std::string& s) const;
	std::string escapeBlob(const char* s, uint32_t length) const;
	uint64_t getLastInsertId();
	std::string getClientVersion();

protected:	
    sqlite3* handle = nullptr;
};


class SQLiteDBResult : public DBResult
{		
public:
	int64_t getAnyNumber(std::string const& s) const;
	std::string getString(const std::string& s) const;
	const char* getStream(const std::string& s, size_t& size) const;

	bool hasNext();
	bool next();

	SQLiteDBResult(sqlite3_stmt* res, bool rowAvailable);
	~SQLiteDBResult();
protected:		

    sqlite3_stmt* handle;
    bool rowAvailable = true;

    std::map<std::string, int> listNames;

	friend class DatabaseSQLite;
};

#endif

#endif
