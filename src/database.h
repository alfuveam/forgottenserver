/**
 * The Forgotten Server - a free and open-source MMORPG server emulator
 * Copyright (C) 2019  Mark Samman <mark.samman@gmail.com>
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

#ifndef FS_DATABASE_H_A484B0CDFDE542838F506DCE3D40C693
#define FS_DATABASE_H_A484B0CDFDE542838F506DCE3D40C693

#include <boost/lexical_cast.hpp> 
#include <errmsg.h>

#include "definitions.h"
#include "databasemanager.h"
#include "configmanager.h"
extern ConfigManager g_config;

class DBResult;
using DBResult_ptr = std::shared_ptr<DBResult>;

class Database
{
	public:
		Database() {};
		virtual ~Database() {};

		// non-copyable
		//Database(const Database&) = delete;
		//Database& operator=(const Database&) = delete;

		/**
		 * Singleton implementation.
		 *
		 * @return database connection handler singleton
		 */
		static Database& getInstance();

		/**
		 * Connects to the database
		 *
		 * @return true on successful connection, false on error
		 */
		bool connect()
		{
			return m_connected;
		}

		/**
		 * Executes command.
		 *
		 * Executes query which doesn't generates results (eg. INSERT, UPDATE, DELETE...).
		 *
		 * @param query command
		 * @return true on success, false on error
		 */
		virtual bool executeQuery(const std::string& query) = 0;

		/**
		 * Queries database.
		 *
		 * Executes query which generates results (mostly SELECT).
		 *
		 * @return results object (nullptr on error)
		 */
		virtual DBResult_ptr storeQuery(const std::string& query) = 0;

		/**
		 * Escapes string for query.
		 *
		 * Prepares string to fit SQL queries including quoting it.
		 *
		 * @param s string to be escaped
		 * @return quoted string
		 */
		virtual std::string escapeString(const std::string& s) const = 0;

		/**
		 * Escapes binary stream for query.
		 *
		 * Prepares binary stream to fit SQL queries.
		 *
		 * @param s binary stream
		 * @param length stream length
		 * @return quoted string
		 */
		virtual std::string escapeBlob(const char* s, uint32_t length) const = 0;

		/**
		 * Retrieve id of last inserted row
		 *
		 * @return id on success, 0 if last query did not result on any rows with auto_increment keys
		 */
		virtual uint64_t getLastInsertId() = 0;

		/**
		 * Get database engine version
		 *
		 * @return the database engine version
		 */
		virtual std::string getClientVersion() = 0;

		uint64_t getMaxPacketSize() const {
			return maxPacketSize;
		}

		bool m_connected;
		std::recursive_mutex databaseLock;

	private:
		/**
		 * Transaction related methods.
		 *
		 * Methods for starting, commiting and rolling back transaction. Each of the returns boolean value.
		 *
		 * @return true on success, false on error
		 */
		bool beginTransaction();
		bool rollback();
		bool commit();
		
	private:
		uint64_t maxPacketSize = 1048576;
	friend class DBTransaction;
};

class DBResult
{
	public:
		DBResult() {};
		template<class Y> DBResult(Y * p) {};
		virtual ~DBResult() {};

		// non-copyable
		//DBResult(const DBResult&) = delete;
		//DBResult& operator=(const DBResult&) = delete;

		template<typename A>
		A getNumber(const std::string & s) const {
			A data;
			try{
				data = static_cast<A>(getAnyNumber(s));
			}
			catch (const std::exception& e) {
				std::cerr << e.what() << std::endl;				
				return 0;
			}
			return data;
		}	

		virtual std::string getString(const std::string& s) const = 0;
		virtual const char* getStream(const std::string& s, size_t& size) const = 0;

		virtual bool hasNext() = 0;
		virtual bool next() = 0;

	private:
		virtual int64_t getAnyNumber(std::string const& s) const = 0;
	friend class Database;
};

/**
 * INSERT statement.
 */
class DBInsert
{
	public:
		explicit DBInsert(std::string query);
		bool addRow(const std::string& row);
		bool addRow(std::ostringstream& row);
		bool execute();

	private:
		std::string query;
		std::string values;
		size_t length;
};

class DBTransaction
{
	public:
		constexpr DBTransaction() = default;

		~DBTransaction() {
			if (state == STATE_START) {
				Database::getInstance().rollback();
			}
		}

		// non-copyable
		DBTransaction(const DBTransaction&) = delete;
		DBTransaction& operator=(const DBTransaction&) = delete;

		bool begin() {
			state = STATE_START;
			return Database::getInstance().beginTransaction();
		}

		bool commit() {
			if (state != STATE_START) {
				return false;
			}

			state = STATE_COMMIT;
			return Database::getInstance().commit();
		}

	private:
		enum TransactionStates_t {
			STATE_NO_START,
			STATE_START,
			STATE_COMMIT,
		};

		TransactionStates_t state = STATE_NO_START;
};

#endif
