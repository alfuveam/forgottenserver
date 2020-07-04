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

#if defined(__ODBC__) || defined(__ALLDB__)

#include "databaseodbc.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>

#define RETURN_SUCCESS(ret) (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)

DatabaseODBC::DatabaseODBC()
{
	m_connected = false;

	char* dns = new char[SQL_MAX_DSN_LENGTH];
	char* user = new char[32];
	char* pass = new char[32];

	strcpy((char*)dns, g_config.getString(ConfigManager::SQL_DB).c_str());
	strcpy((char*)user, g_config.getString(ConfigManager::SQL_USER).c_str());
	strcpy((char*)pass, g_config.getString(ConfigManager::SQL_PASS).c_str());

	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_env);
	if(!RETURN_SUCCESS(ret)){
		std::cout << "Failed to allocate ODBC SQLHENV enviroment handle." << std::endl;
		m_env = NULL;
		return;
	}

	ret = SQLSetEnvAttr(m_env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);
	if(!RETURN_SUCCESS(ret)){
		std::cout << "SQLSetEnvAttr(SQL_ATTR_ODBC_VERSION): Failed to switch to ODBC 3 version." << std::endl;
		SQLFreeHandle(SQL_HANDLE_ENV, m_env);
		m_env = NULL;
	}

	if(m_env == NULL){
		std::cout << "ODBC SQLHENV enviroment not initialized." << std::endl;
		return;
	}

	ret = SQLAllocHandle(SQL_HANDLE_DBC, m_env, &m_handle);
	if(!RETURN_SUCCESS(ret)){
		std::cout << "Failed to allocate ODBC SQLHDBC connection handle." << std::endl;
		m_handle = NULL;
		return;
	}

	ret = SQLSetConnectAttr(m_handle, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER*)5, 0);
	if(!RETURN_SUCCESS(ret)){
		std::cout << "SQLSetConnectAttr(SQL_ATTR_CONNECTION_TIMEOUT): Failed to set connection timeout." << std::endl;
		SQLFreeHandle(SQL_HANDLE_DBC, m_handle);
		m_handle = NULL;
		return;
	}

	ret = SQLConnect(m_handle, (SQLCHAR*)dns, SQL_NTS, (SQLCHAR*)user, SQL_NTS, (SQLCHAR*)pass, SQL_NTS);
	if(!RETURN_SUCCESS(ret)){
		std::cout << "Failed to connect to ODBC via DSN: " << dns << " (user " << user << ")" << std::endl;
		SQLFreeHandle(SQL_HANDLE_DBC, m_handle);
		m_handle = NULL;
		return;
	}

	m_connected = true;
}

DatabaseODBC::~DatabaseODBC()
{
	if(m_connected){
		SQLDisconnect(m_handle);
		SQLFreeHandle(SQL_HANDLE_DBC, m_handle);
		m_handle = NULL;
		m_connected = false;
	}

	SQLFreeHandle(SQL_HANDLE_ENV, m_env);
}

bool DatabaseODBC::executeQuery(const std::string& query)
{
	if(!m_connected)
		return false;

	std::string buf = (std::string)query;
	boost::replace_all(buf, "`", "");

	SQLHSTMT stmt;

	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_handle, &stmt);
	if(!RETURN_SUCCESS(ret)){
		std::cout << "Failed to allocate ODBC SQLHSTMT statement." << std::endl;
		return false;
	}

	ret = SQLExecDirect(stmt, (SQLCHAR*)buf.c_str(), buf.length() );

	if(!RETURN_SUCCESS(ret)){
		std::cout << "SQLExecDirect(): " << query << ": ODBC ERROR." << std::endl;
		return false;
	}

	return true;
}

DBResult_ptr DatabaseODBC::storeQuery(const std::string& query)
{
	if(!m_connected)
		return DBResult_ptr();

	std::string buf = (std::string)query;
	boost::replace_all(buf, "`", "");

	SQLHSTMT stmt;

	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_handle, &stmt);
	if(!RETURN_SUCCESS(ret)){
		std::cout << "Failed to allocate ODBC SQLHSTMT statement." << std::endl;
		return DBResult_ptr();
	}

	ret = SQLExecDirect(stmt, (SQLCHAR*)buf.c_str(), buf.length() );

	if(!RETURN_SUCCESS(ret)){
		std::cout << "SQLExecDirect(): " << query << ": ODBC ERROR." << std::endl;
		return DBResult_ptr();
	}
	// retrieving results of query
	DBResult_ptr result = std::make_shared<ODBCDBResult>(stmt);
	if (!result->hasNext()) {
		return nullptr;
	}
	return result;
}


std::string DatabaseODBC::getClientVersion() {
	return "ODBC - ";
}

uint64_t DatabaseODBC::getLastInsertId()
{
	return 0;
}

std::string DatabaseODBC::escapeString(const std::string &s) const
{
	return escapeBlob( s.c_str(), s.length() );
}

std::string DatabaseODBC::escapeBlob(const char* s, uint32_t length) const
{
	std::string buf = "'";

	for(uint32_t i = 0; i < length; i++){
		switch(s[i]){
		case '\'':
			buf += "\'\'";
			break;

		case '\0':
			buf += "\\0";
			break;

		case '\\':
			buf += "\\\\";
			break;

		case '\r':
			buf += "\\r";
			break;

		case '\n':
			buf += "\\n";
			break;

		default:
			buf += s[i];
		}
	}

	buf += "'";
	return buf;
}


std::string ODBCDBResult::getString(const std::string &s) const
{
	auto it = m_listNames.find(s);
	if(it != m_listNames.end() )
	{
		char* value = new char[1024];
		SQLRETURN ret = SQLGetData(m_handle, it->second, SQL_C_CHAR, value, 1024, NULL);

		if( RETURN_SUCCESS(ret) ){
		std::string buff = std::string(value);
		return buff;
		}
		else{
		std::cout << "Error during getDataString(" << s << ")." << std::endl;
		}
	}

	std::cout << "Error during getDataString(" << s << ")." << std::endl;
	return std::string(""); // Failed
}

const char* ODBCDBResult::getStream(const std::string &s, size_t &size) const
{
	auto it = m_listNames.find(s);
	if(it != m_listNames.end() )
	{
		char* value = new char[1024];
		SQLRETURN ret = SQLGetData(m_handle, it->second, SQL_C_BINARY, value, 1024, (SQLLEN*)&size);

		if( RETURN_SUCCESS(ret) )
		return value;
		else
		std::cout << "Error during getDataStream(" << s << ")." << std::endl;
	}

	std::cout << "Error during getDataStream(" << s << ")." << std::endl;
	return 0; // Failed
}

bool ODBCDBResult::hasNext()
{
	return !m_rowAvailable;
}

bool ODBCDBResult::next()
{
	return !m_rowAvailable;
}


ODBCDBResult::ODBCDBResult(SQLHSTMT stmt)
{
	m_handle = stmt;
	m_rowAvailable = false;

	int16_t numCols;
	SQLNumResultCols(m_handle, &numCols);

	for(int32_t i = 1; i <= numCols; i++){
		char* name = new char[129];
		SQLDescribeCol(m_handle, i, (SQLCHAR*)name, 129, NULL, NULL, NULL, NULL, NULL);
		m_listNames[name] = i;
	}
}

ODBCDBResult::~ODBCDBResult()
{
	SQLFreeHandle(SQL_HANDLE_STMT, m_handle);
}

int64_t ODBCDBResult::getAnyNumber(std::string const &s) const
{	
	auto it = m_listNames.find(s);
	if(it != m_listNames.end() )
	{
		int64_t value;
		SQLRETURN ret = SQLGetData(m_handle, it->second, SQL_C_SBIGINT, &value, 0, NULL);

		if( RETURN_SUCCESS(ret) )
		return value;
		else
		std::cout << "Error during getDataLong(" << s << ")." << std::endl;
	}

	std::cout << "Error during getDataLong(" << s << ")." << std::endl;
	return 0; // Failed
}

#endif
