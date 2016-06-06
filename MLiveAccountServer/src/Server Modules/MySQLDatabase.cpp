#include "../stdafx.h"

#define DatabaseLog(format, ...) DebugLog(L_INFO, "[Database]: "format, __VA_ARGS__)

DWORD WINAPI MySQLDatabase::PingThread(LPVOID lpArg)
{
	return ((MySQLDatabase*)lpArg)->PingDatabase();
}

bool MySQLDatabase::EmergencyMassgateDisconnect()
{
	// set maintenance mode, server will most likely need attending if it gets to this point.
	DatabaseLog("connection to %s lost, check the MySQL server.", this->host);

	MMG_AccountProtocol::ourInstance->m_MaintenanceMode = true;
	SvClientManager::ourInstance->EmergencyDisconnectAll();
	return true;
}

bool MySQLDatabase::Initialize()
{
	// check if client version started
	const char *info = mysql_get_client_info();

	if (!info)
		return false;

	DatabaseLog("MySQL client version: %s started", info);

	// read mysql configuration from file
	if (!this->ReadConfig("settings.ini"))
		return false;

	if (this->ConnectDatabase())
	{
		DatabaseLog("Connected to server MySQL %s on '%s'", mysql_get_server_info(this->m_Connection), this->host);
		//this->PrintStatus();

		//create a thread to 'ping' the database every 5 hours, preventing timeout.
		this->m_PingThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) PingThread, this, 0, NULL);
	}

	return true;
}

void MySQLDatabase::Unload()
{
	TerminateThread(this->m_PingThreadHandle, 0);
	this->m_PingThreadHandle = NULL;

	mysql_close(this->m_Connection);
	this->m_Connection = NULL;
	this->isConnected = false;

	if (this->host) free((char*)this->host);
	if (this->user) free((char*)this->user);
	if (this->pass) free((char*)this->pass);
	if (this->db) free((char*)this->db);

	this->host = NULL;
	this->user = NULL;
	this->pass = NULL;
	this->db = NULL;
}

bool MySQLDatabase::ReadConfig(const char *filename)
{
	//might need to change working directory in property pages if it cant find the file
	FILE *file;
	file = fopen(filename, "r");

	if(!file)
	{
		DatabaseLog("could not open configuration file '%s'", filename);
		return false;
	}

	const int bufferlen = 32;		// characters read per line
	const int maxoptions = 4;		// number of options read from the file
	int i = 0;

	char line[bufferlen];
	memset(line, 0, sizeof(line));

	char settings[maxoptions][bufferlen];
	memset(settings, 0, sizeof(settings));

	while (fgets(line, bufferlen, file) && i < maxoptions)
	{
		// remove newline
		if (strchr(line, 10))
			line[strlen(line)-1] = 0;

		// ignoring standard comment characters and header. 91 = '[', 47 = '/'
		if (!strchr(line, 91) && !strchr(line, 47) && strlen(line))
		{
			strcpy(settings[i], line);
			i++;
		}

		// reset the line buffer
		memset(line, 0, bufferlen);
	}

	fclose(file);

	this->host = (char*)malloc(strlen(settings[0]) + 1);
	memcpy((char*)this->host, settings[0], strlen(settings[0]) + 1);

	this->user = (char*)malloc(strlen(settings[1]) + 1);
	memcpy((char*)this->user, settings[1], strlen(settings[1]) + 1);

	this->pass = (char*)malloc(strlen(settings[2]) + 1);
	memcpy((char*)this->pass, settings[2], strlen(settings[2]) + 1);

	this->db = (char*)malloc(strlen(settings[3]) + 1);
	memcpy((char*)this->db, settings[3], strlen(settings[3]) + 1);

	return true;
}

bool MySQLDatabase::ConnectDatabase()
{
	//create the connection object
	this->m_Connection = mysql_init(NULL);

	//set the connection options
	mysql_options(this->m_Connection, MYSQL_OPT_RECONNECT, &this->reconnect);
	mysql_options(this->m_Connection, MYSQL_OPT_BIND, this->bind_interface);
	mysql_options(this->m_Connection, MYSQL_SET_CHARSET_NAME, this->charset_name);

	// attempt connection to the database
	if (mysql_real_connect(this->m_Connection, this->host, this->user, this->pass, this->db, 0, NULL, CLIENT_REMEMBER_OPTIONS) == NULL)
	{
		DebugLog(L_WARN, "[Database]: %s", mysql_error(this->m_Connection));

		mysql_close(this->m_Connection);
		this->m_Connection = NULL;
		this->isConnected = false;

		return false;
	}

	this->isConnected = true;

	return true;
}

bool MySQLDatabase::HasConnection()
{
	return (this->m_Connection != NULL) && this->isConnected;
}

void MySQLDatabase::PrintStatus()
{
	if (!this->HasConnection())
		return;

	//connection type
	const char *info = mysql_get_host_info(this->m_Connection);

	const char *find = "via ";
	size_t pos = (size_t)(strstr(info, find) - info) + strlen(find);

	const char *hostname = this->host;
	const char *type = info+pos;

	// server version
	const char *version = mysql_get_server_info(this->m_Connection);

	// character set info
	const char *charsetname = mysql_character_set_name(this->m_Connection);

	MY_CHARSET_INFO charsetinfo;
	mysql_get_character_set_info(this->m_Connection, &charsetinfo);

	printf("\n");
	printf("Connection Status\n");
	printf("-----------------------------------\n");
	printf("Hostname:         %s\n", hostname);
	printf("Type:             %s\n", type);
	printf("Version:          MySQL %s\n", version);
	printf("Current Charset:  %s\n", charsetname);
	printf("-----------------------------------\n");
	printf("Default Client Charset\n");
	printf("Charset:          %s\n", charsetinfo.name);
	printf("Collation:        %s\n", charsetinfo.csname);
	printf("-----------------------------------\n");
}

bool MySQLDatabase::TestDatabase()
{
	MYSQL_RES *result = NULL;

	bool error_flag = false;

	if (!this->HasConnection())
		error_flag = true;

	if (!error_flag)
	{
		// execute a standard query on the database
		if (mysql_query(this->m_Connection, "SELECT poll FROM mg_utils LIMIT 1"))
			error_flag = true;

		// must call mysql_store_result after calling mysql_query
		if (!error_flag)
			result = mysql_store_result(this->m_Connection);

		if (!result)
			error_flag = true;
		else
		{
			// if there is only one field, query has executed successfully, no need to check the row
			if(mysql_num_fields(result) != 1)
				error_flag = true;

			mysql_free_result(result);
		}
	}

	if (error_flag)
	{
		this->isConnected = false;
		return false;
	}

	this->isConnected = true;
	return true;
}

bool MySQLDatabase::PingDatabase()
{
	while (true)
	{
		Sleep(this->sleep_interval_ms);

		if (mysql_ping(this->m_Connection) == 0)
		{
			this->isConnected = true;
			MMG_AccountProtocol::ourInstance->m_MaintenanceMode = false;
		}
		else
		{
			//connection lost
			//mysql_ping will attempt to reconnect automatically, see notes in header.

			DatabaseLog("connection to %s lost, attempting reconnect", this->host);

			this->isConnected = false;
			MMG_AccountProtocol::ourInstance->m_MaintenanceMode = true;
		}
	}
	
	return true;
}

bool MySQLDatabase::InitializeSchema()
{
	if (!this->m_Connection)
		return false;

	// Go through each table entry
	for(int i = 0; i < ARRAYSIZE(MySQLTableValues); i++)
	{
		MySQLQuery query(this->m_Connection, MySQLTableValues[i]);
		
		if (!query.Step())
			return false;
	}

	DatabaseLog("database tables created OK");

	return true;
}

bool MySQLDatabase::CheckIfEmailExists(const char *email, uint *dstId)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT id FROM mg_accounts WHERE email = ? LIMIT 1");
	
	// prepared statement binding structures
	MYSQL_BIND param[1], result[1];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(result, 0, sizeof(result));

	// query specific variables
	ulong emailLength = strlen(email);
	uint id;

	// bind parameters to prepared statement
	query.Bind(&param[0], email, &emailLength);

	// bind results
	query.Bind(&result[0], &id);				//mg_accounts.id

	// execute prepared statement
	if(!query.StmtExecute(param, result))
	{
		DatabaseLog("CheckIfEmailExists() query failed: %s", email);
		*dstId = 0;
	}
	else
	{
		if (!query.StmtFetch())
		{
			DatabaseLog("email %s not found", email);
			*dstId = 0;
		}
		else
		{
			DatabaseLog("email %s found", email);
			*dstId = id;
		}
	}

	return query.Success();
}

bool MySQLDatabase::CheckIfCDKeyExists(const ulong cipherKeys[], uint *dstId)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT id FROM mg_cdkeys WHERE cipherkeys0 = ? AND cipherkeys1 = ? AND cipherkeys2 = ? AND cipherkeys3 = ? LIMIT 1");

	// prepared statement binding structures
	MYSQL_BIND params[4], result[1];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));
	memset(result, 0, sizeof(result));

	// query specific variables
	uint id;

	// bind parameters to prepared statement
	query.Bind(&params[0], &cipherKeys[0]);
	query.Bind(&params[1], &cipherKeys[1]);
	query.Bind(&params[2], &cipherKeys[2]);
	query.Bind(&params[3], &cipherKeys[3]);

	// bind results
	query.Bind(&result[0], &id);			//mg_cdkeys.id

	// execute prepared statement
	if(!query.StmtExecute(params, result))
	{
		DatabaseLog("CheckIfCDKeyExists() query failed:");
		*dstId = 0;
	}
	else
	{
		if (!query.StmtFetch())
		{
			DatabaseLog("cipherkeys not found");
			*dstId = 0;
		}
		else
		{
			DatabaseLog("cipherkeys found");
			*dstId = id;
		}
	}

	return query.Success();
}

bool MySQLDatabase::CreateUserAccount(const char *email, const wchar_t *password, const char *country, const uchar *emailgamerelated, const uchar *acceptsemail, const ulong cipherKeys[])
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// *Step 1: insert account* //

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "INSERT INTO mg_accounts (email, password, country, emailgamerelated, acceptsemail, activeprofileid, isbanned) VALUES (?, ?, ?, ?, ?, 0, 0)");

	// prepared statement binding structures
	MYSQL_BIND params[5];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	ulong emailLength = strlen(email);
	ulong passLength = wcslen(password);
	ulong countryLength = strlen(country);
	uint account_insert_id, cdkey_insert_id;

	// bind parameters to prepared statement
	query.Bind(&params[0], email, &emailLength);			//email
	query.Bind(&params[1], password, &passLength);		//password
	query.Bind(&params[2], country, &countryLength);		//country
	query.Bind(&params[3], emailgamerelated);			//emailgamerelated (game related news and events)
	query.Bind(&params[4], acceptsemail);				//acceptsemail (from ubisoft (pfft) rename this field)

	// execute prepared statement
	if (!query.StmtExecute(params))
	{
		DatabaseLog("CreateUserAccount() query failed: %s", email);
		account_insert_id = 0;
	}
	else
	{
		DatabaseLog("created account: %s", email);
		account_insert_id = (uint)query.StmtInsertId();
	}

	// *Step 2: insert cdkeys* //

	// prepared statement wrapper object
	MySQLQuery query2(this->m_Connection, "INSERT INTO mg_cdkeys (accountid, cipherkeys0, cipherkeys1, cipherkeys2, cipherkeys3) VALUES (?, ?, ?, ?, ?)");

	// prepared statement binding structures
	MYSQL_BIND params2[5];

	// initialize (zero) bind structures
	memset(params2, 0, sizeof(params2));

	// bind parameters to prepared statement
	query2.Bind(&params2[0], &account_insert_id);	
	query2.Bind(&params2[1], &cipherKeys[0]);
	query2.Bind(&params2[2], &cipherKeys[1]);
	query2.Bind(&params2[3], &cipherKeys[2]);
	query2.Bind(&params2[4], &cipherKeys[3]);

	// execute prepared statement
	if (!query2.StmtExecute(params2))
	{
		DatabaseLog("CreateUserAccount() query2 failed: %s", email);
		cdkey_insert_id = 0;
	}
	else
	{
		DatabaseLog("inserted cipherkeys: %s", email);
		cdkey_insert_id = (uint)query2.StmtInsertId();
	}

	return query.Success() && query2.Success();
}

bool MySQLDatabase::AuthUserAccount(const char *email, wchar_t *dstPassword, uchar *dstIsBanned, MMG_AuthToken *authToken)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	char *sql = "SELECT mg_accounts.id, mg_accounts.password, mg_accounts.activeprofileid, mg_accounts.isbanned, mg_cdkeys.id AS cdkeyid "
				"FROM mg_accounts "
				"JOIN mg_cdkeys "
				"ON mg_accounts.id = mg_cdkeys.accountid "
				"WHERE mg_accounts.email = ? LIMIT 1";

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, sql);

	// prepared statement binding structures
	MYSQL_BIND param[1], results[5];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(results, 0, sizeof(results));

	// query specific variables
	ulong emailLength = strlen(email);
	
	wchar_t password[WIC_PASSWORD_MAX_LENGTH];
	memset(password, 0, sizeof(password));
	ulong passLength = ARRAYSIZE(password);

	uint id, activeprofileid, cdkeyid;
	uchar isbanned;

	// bind parameters to prepared statement
	query.Bind(&param[0], email, &emailLength);		//email

	// bind results
	query.Bind(&results[0], &id);					//mg_accounts.id
	query.Bind(&results[1], password, &passLength);	//mg_accounts.password
	query.Bind(&results[2], &activeprofileid);		//mg_accounts.activeprofileid
	query.Bind(&results[3], &isbanned);				//mg_accounts.isbanned
	query.Bind(&results[4], &cdkeyid);				//mg_cdkeys.id AS cdkeyid

	// execute prepared statement
	if(!query.StmtExecute(param, results))
	{
		DatabaseLog("AuthUserAccount() query failed: %s", email);

		authToken->m_AccountId = 0;
		authToken->m_ProfileId = 0;
		//authToken->m_TokenId = 0;
		authToken->m_CDkeyId = 0;
		dstPassword = L"";
		*dstIsBanned = 0;
	}
	else
	{
		if (!query.StmtFetch())
		{
			DatabaseLog("account %s not found", email);

			authToken->m_AccountId = 0;
			authToken->m_ProfileId = 0;
			//authToken->m_TokenId = 0;
			authToken->m_CDkeyId = 0;
			dstPassword = L"";
			*dstIsBanned = 0;
		}
		else
		{
			DatabaseLog("account %s found", email);

			authToken->m_AccountId = id;
			authToken->m_ProfileId = activeprofileid;
			//authToken->m_TokenId = 0;
			authToken->m_CDkeyId = cdkeyid;
			wcscpy_s(dstPassword, ARRAYSIZE(password), password);
			*dstIsBanned = isbanned;
		}
	}

	return query.Success();
}

bool MySQLDatabase::CheckIfProfileExists(const wchar_t* name, uint *dstId)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT id FROM mg_profiles WHERE name = ? LIMIT 1");

	// prepared statement binding structures
	MYSQL_BIND param[1], result[1];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(result, 0, sizeof(result));

	// query specific variables
	bool querySuccess;
	ulong nameLength = wcslen(name);
	uint id;

	// bind parameters to prepared statement
	query.Bind(&param[0], name, &nameLength);

	// bind results
	query.Bind(&result[0], &id);		//mg_profiles.id

	// execute prepared statement
	if(!query.StmtExecute(param, result))
	{
		DatabaseLog("CheckIfProfileExists() query failed: %ws", name);
		querySuccess = false;
		*dstId = 0;
	}
	else
	{
		if (!query.StmtFetch())
		{
			DatabaseLog("profile %ws not found", name);
			querySuccess = true;
			*dstId = 0;
		}
		else
		{
			DatabaseLog("profile %ws found", name);
			querySuccess = true;
			*dstId = id;
		}
	}

	return querySuccess;
}

bool MySQLDatabase::CreateUserProfile(const uint accountId, const wchar_t* name, const char* email)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// *Step 1: create the profile as usual* //

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "INSERT INTO mg_profiles (accountid, name, rank, clanid, rankinclan, commoptions, lastlogindate, motto, homepage) VALUES (?, ?, 0, 0, 0, 992, ?, NULL, NULL)");
	
	// prepared statement binding structures
	MYSQL_BIND params[3];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	bool query1Success, query2Success, query3Success;
	ulong count;
	uint last_id;

	ulong nameLength = wcslen(name);
	uint id;

	MYSQL_TIME datetime;

	datetime.year = 1970;
	datetime.month = 1;
	datetime.day = 1;
	datetime.hour = 0;
	datetime.minute = 0;
	datetime.second = 0;

	// bind parameters to prepared statement
	query.Bind(&params[0], &accountId);			//account id
	query.Bind(&params[1], name, &nameLength);	//profile name
	query.BindDateTime(&params[2], &datetime);	//last login date, set to 1/1/1970 for new accounts

	// execute prepared statement
	if (!query.StmtExecute(params))
	{
		DatabaseLog("%s create profile '%ws' failed for account id %d", email, name, accountId);
		query1Success = false;
		last_id = 0;
	}
	else
	{
		DatabaseLog("%s created profile %ws", email, name);
		query1Success = true;
		last_id = (uint)query.StmtInsertId();
	}

	// *Step 2: get profile count for current account* //

	if(query1Success)
	{
		char *sql2 = "SELECT mg_profiles.id "
					"FROM mg_profiles "
					"JOIN mg_accounts "
					"ON mg_profiles.accountid = mg_accounts.id "
					"WHERE mg_accounts.email = ? "
					"ORDER BY lastlogindate DESC, id ASC LIMIT 5";

		// prepared statement wrapper object
		MySQLQuery query2(this->m_Connection, sql2);

		// prepared statement binding structures
		MYSQL_BIND param2[1], result2[1];

		// initialize (zero) bind structures
		memset(param2, 0, sizeof(param2));
		memset(result2, 0, sizeof(result2));

		// query specific variables
		ulong emailLength = strlen(email);

		// bind parameters to prepared statement
		query2.Bind(&param2[0], email, &emailLength);	//email

		// bind results
		query2.Bind(&result2[0], &id);					//mg_profiles.id

		// execute prepared statement
		if(!query2.StmtExecute(param2, result2))
		{
			DatabaseLog("CreateUserProfile() failed:");
			query2Success = false;
			count = 0;
		}
		else
		{
			query2Success = true;
			count = (ulong)query2.StmtNumRows();
			DatabaseLog("found %d profiles", count);
		}
	}

	// *Step 3: if there is only one profile for the account, set the only profile as the active profile* //

	if(count == 1)
	{
		// prepared statement wrapper object
		MySQLQuery query3(this->m_Connection, "UPDATE mg_accounts SET activeprofileid = ? WHERE id = ?");

		// prepared statement binding structures
		MYSQL_BIND params3[2];

		// initialize (zero) bind structures
		memset(params3, 0, sizeof(params3));

		// bind parameters to prepared statement
		query3.Bind(&params3[0], &last_id);			//profileid
		query3.Bind(&params3[1], &accountId);		//account id
		
		// execute prepared statement
		if(!query3.StmtExecute(params3))
		{
			DatabaseLog("set active profile failed");
			query3Success = false;
			last_id = 0;
		}
		else
		{
			DatabaseLog("set active profile to %d OK", last_id);
			query3Success = true;
		}
	}

	return query1Success && query2Success && query3Success;
}

bool MySQLDatabase::DeleteUserProfile(const uint accountId, const uint profileId, const char* email)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	//TODO: (when forum is implemented) 
	//dont delete the profile, just rename the profile name
	//set an 'isdeleted' flag to 1
	//disassociate the profile with the account

	// *Step 1: just delete the profile* //

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "DELETE FROM mg_profiles WHERE id = ?");

	// prepared statement binding structures
	MYSQL_BIND param[1];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));

	// query specific variables
	bool query1Success, query2Success, query3Success;
	bool query4Success, query5Success;
	ulong count;
	uint id;

	// bind parameters to prepared statement
	query.Bind(&param[0], &profileId);		//profile id

	// execute prepared statement
	if (!query.StmtExecute(param))
	{
		DatabaseLog("DeleteUserProfile(query1) failed: profile id(%d)", email, profileId);
		query1Success = false;
	}
	else
	{
		DatabaseLog("%s deleted profile id(%d)", email, profileId);
		query1Success = true;
	}

	// *Step 2: get profile count for current account* //

	//if(query1Success)
	//{
		char *sql2 = "SELECT mg_profiles.id "
					"FROM mg_profiles "
					"JOIN mg_accounts "
					"ON mg_profiles.accountid = mg_accounts.id "
					"WHERE mg_accounts.email = ? "
					"ORDER BY lastlogindate DESC, id ASC LIMIT 5";

		// prepared statement wrapper object
		MySQLQuery query2(this->m_Connection, sql2);

		// prepared statement binding structures
		MYSQL_BIND params[1], results[1];

		// initialize (zero) bind structures
		memset(params, 0, sizeof(params));
		memset(results, 0, sizeof(results));

		// query specific variables
		ulong emailLength = strlen(email);

		// bind parameters to prepared statement
		query2.Bind(&params[0], email, &emailLength);	//email

		// bind results
		query2.Bind(&results[0], &id);					//mg_profiles.id

		// execute prepared statement
		if(!query2.StmtExecute(params, results))
		{
			DatabaseLog("DeleteUserProfile(query2) failed: account id(%d), profile id(%d)", accountId, profileId);
			query2Success = false;
			count = 0;
		}
		else
		{
			query2Success = true;
			count = (ulong)query2.StmtNumRows();
		}
	//}

	// *Step 3: set active profile to last used profile id, if there are no profiles set active profile id to 0* //
		
	if(count > 0)
	{

		//fetch first row from query2, this is the last used id, as it is sorted by date
		query2.StmtFetch();

		// prepared statement wrapper object
		MySQLQuery query3(this->m_Connection, "UPDATE mg_accounts SET activeprofileid = ? WHERE id = ?");

		// prepared statement binding structures
		MYSQL_BIND params3[2];

		// initialize (zero) bind structures
		memset(params3, 0, sizeof(params3));

		// bind parameters to prepared statement
		query3.Bind(&params3[0], &id);				//last used id
		query3.Bind(&params3[1], &accountId);		//account id
		
		// execute prepared statement
		if(!query3.StmtExecute(params3))
		{
			DatabaseLog("DeleteUserProfile(query3) failed: account id(%d), profile id(%d)", accountId, profileId);
			query3Success = false;
		}
		else
		{
			DatabaseLog("account id(%d) set active profile id(0)", accountId);
			query3Success = true;
		}
	}
	else
	{
		// prepared statement wrapper object
		MySQLQuery query3(this->m_Connection, "UPDATE mg_accounts SET activeprofileid = 0 WHERE id = ?");

		// prepared statement binding structures
		MYSQL_BIND param3[1];

		// initialize (zero) bind structures
		memset(param3, 0, sizeof(param3));

		// bind parameters to prepared statement
		query3.Bind(&param3[0], &accountId);		//account id
		
		// execute prepared statement
		if(!query3.StmtExecute(param3))
		{
			DatabaseLog("DeleteUserProfile(query3) failed: account id(%d), profile id(%d)", accountId, profileId);
			query3Success = false;
		}
		else
		{
			DatabaseLog("account id(%d) set active profile id(0)", accountId);
			query3Success = true;
		}
	}

	// *Step 4: delete friends list for profile id and delete profile id from all friends lists* //

	// prepared statement wrapper object
	MySQLQuery query4(this->m_Connection, "DELETE FROM mg_friends WHERE profileid = ? OR friendprofileid = ?");

	// prepared statement binding structures
	MYSQL_BIND params4[2];

	// initialize (zero) bind structures
	memset(params4, 0, sizeof(params4));

	// bind parameters to prepared statement
	query4.Bind(&params4[0], &profileId);		//profile id
	query4.Bind(&params4[1], &profileId);

	// execute prepared statement
	if (!query4.StmtExecute(params4))
	{
		DatabaseLog("DeleteUserProfile(query4) failed: profile id(%d)", email, profileId);
		query4Success = false;
	}
	else
	{
		DatabaseLog("%s deleted friends list for profile id(%d)", email, profileId);
		query4Success = true;
	}

	// *Step 5: delete ignore list for profile id and delete profile id from all ignore lists* //

	// prepared statement wrapper object
	MySQLQuery query5(this->m_Connection, "DELETE FROM mg_ignored WHERE profileid = ? OR ignoredprofileid = ?");

	// prepared statement binding structures
	MYSQL_BIND params5[2];

	// initialize (zero) bind structures
	memset(params5, 0, sizeof(params5));

	// bind parameters to prepared statement
	query5.Bind(&params5[0], &profileId);		//profile id
	query5.Bind(&params5[1], &profileId);

	// execute prepared statement
	if (!query5.StmtExecute(params5))
	{
		DatabaseLog("DeleteUserProfile(query5) failed: profile id(%d)", email, profileId);
		query5Success = false;
	}
	else
	{
		DatabaseLog("%s deleted ignore list for profile id(%d)", email, profileId);
		query5Success = true;
	}

	return query1Success && query2Success && query3Success && query4Success && query5Success;
}

bool MySQLDatabase::QueryUserProfile(const uint accountId, const uint profileId, MMG_Profile *profile, MMG_Options *options)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// *Step 1: get the requested profile* //
	
	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT id, name, rank, clanid, rankinclan, commoptions FROM mg_profiles WHERE id = ? LIMIT 1");

	// prepared statement binding structures
	MYSQL_BIND param[1], results[6];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(results, 0, sizeof(results));

	// query specific variables
	bool query1Success, query2Success, query3Success;

	wchar_t name[WIC_NAME_MAX_LENGTH];
	memset(name, 0, sizeof(name));
	ulong nameLength = ARRAYSIZE(name);

	uint id, clanid, commoptions;
	uchar rank, rankinclan;

	// bind parameters to prepared statement
	query.Bind(&param[0], &profileId);			//profile id

	// bind results
	query.Bind(&results[0], &id);				//mg_profiles.id
	query.Bind(&results[1], name, &nameLength);	//mg_profiles.name
	query.Bind(&results[2], &rank);				//mg_profiles.rank
	query.Bind(&results[3], &clanid);			//mg_profiles.clanid
	query.Bind(&results[4], &rankinclan);		//mg_profiles.rankinclan
	query.Bind(&results[5], &commoptions);		//mg_profiles.commoptions

	// execute prepared statement
	if(!query.StmtExecute(param, results))
	{
		DatabaseLog("QueryUserProfile(query) failed: profile id(%d)", profileId);
		query1Success = false;

		//profile
		profile->m_ProfileId = 0;
		wcscpy_s(profile->m_Name, L"");
		profile->m_Rank = 0;
		profile->m_ClanId = 0;
		profile->m_RankInClan = 0;
		profile->m_OnlineStatus = 0;

		//communication options
		options->FromUInt(0);
	}
	else
	{
		if (!query.StmtFetch())
		{
			DatabaseLog("profile id(%d) not found", profileId);
			query1Success = true;

			//profile
			profile->m_ProfileId = 0;
			wcscpy_s(profile->m_Name, L"");
			profile->m_Rank = 0;
			profile->m_ClanId = 0;
			profile->m_RankInClan = 0;
			profile->m_OnlineStatus = 0;

			//communication options
			options->FromUInt(0);
		}
		else
		{
			DatabaseLog("profile id(%d) %ws found", profileId, name);
			query1Success = true;

			//profile
			profile->m_ProfileId = id;
			wcscpy_s(profile->m_Name, name);
			profile->m_Rank = rank;
			profile->m_ClanId = clanid;
			profile->m_RankInClan = rankinclan;
			profile->m_OnlineStatus = 0;

			//communication options
			options->FromUInt(commoptions);
		}
	}

	// *Step 2: update last login date for the requested profile* //

	if(query1Success)
	{
		// prepared statement wrapper object
		MySQLQuery query2(this->m_Connection, "UPDATE mg_profiles SET lastlogindate = NOW() WHERE id = ?");

		// prepared statement binding structures
		MYSQL_BIND param2[1];

		// initialize (zero) bind structures
		memset(param2, 0, sizeof(param2));

		// bind parameters to prepared statement
		query2.Bind(&param2[0], &profile->m_ProfileId);		//profile id
		
		// execute prepared statement
		if(!query2.StmtExecute(param2))
		{
			DatabaseLog("QueryUserProfile(query2) failed: profile id(%d)", profile->m_ProfileId);
			query2Success = false;
		}
		else
		{
			DatabaseLog("profile id(%d) %ws lastlogindate updated", profile->m_ProfileId, profile->m_Name);
			query2Success = true;
		}
	}

	// *Step 3: set new requested profile as active for the account* //

	if(query2Success)
	{
		// prepared statement wrapper object
		MySQLQuery query3(this->m_Connection, "UPDATE mg_accounts SET activeprofileid = ? WHERE id = ?");

		// prepared statement binding structures
		MYSQL_BIND params3[2];

		// initialize (zero) bind structures
		memset(params3, 0, sizeof(params3));

		// bind parameters to prepared statement
		query3.Bind(&params3[0], &profile->m_ProfileId);		//profileid
		query3.Bind(&params3[1], &accountId);					//account id
		
		// execute prepared statement
		if(!query3.StmtExecute(params3))
		{
			DatabaseLog("QueryUserProfile(query3) failed: account id(%d), profile id(%d)", accountId, profile->m_ProfileId);
			query3Success = false;
		}
		else
		{
			DatabaseLog("account id(%d) set active profile id(%d) %ws", accountId, profile->m_ProfileId, profile->m_Name);
			query3Success = true;
		}
	}

	return query1Success && query2Success && query3Success;
}

bool MySQLDatabase::RetrieveUserProfiles(const char *email, const wchar_t *password, ulong *dstProfileCount, MMG_Profile *profiles[])
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	char *sql = "SELECT mg_profiles.id, mg_profiles.name, mg_profiles.rank, mg_profiles.clanid, mg_profiles.rankinclan "
				"FROM mg_profiles "
				"JOIN mg_accounts "
				"ON mg_profiles.accountid = mg_accounts.id "
				"WHERE mg_accounts.email = ? "
				"ORDER BY lastlogindate DESC, id ASC LIMIT 5";

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, sql);

	// prepared statement binding structures
	MYSQL_BIND param[1], results[5];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(results, 0, sizeof(results));

	// query specific variables
	bool querySuccess;
	ulong emailLength = strlen(email);

	wchar_t name[WIC_NAME_MAX_LENGTH];
	memset(name, 0, sizeof(name));
	ulong nameLength = ARRAYSIZE(name);

	uint id, clanid;
	uchar rank, rankinclan;

	// bind parameters to prepared statement
	query.Bind(&param[0], email, &emailLength);		//email

	// bind results
	query.Bind(&results[0], &id);					//mg_profiles.id
	query.Bind(&results[1], name, &nameLength);		//mg_profiles.name
	query.Bind(&results[2], &rank);					//mg_profiles.rank
	query.Bind(&results[3], &clanid);				//mg_profiles.clanid
	query.Bind(&results[4], &rankinclan);			//mg_profiles.rankinclan

	// execute prepared statement
	if(!query.StmtExecute(param, results))
	{
		DatabaseLog("RetrieveUserProfiles() query failed: %s", email);
		querySuccess = false;

		MMG_Profile *tmp = new MMG_Profile[1];

		tmp[0].m_ProfileId = 0;
		wcscpy_s(tmp[0].m_Name, L"");
		tmp[0].m_Rank = 0;
		tmp[0].m_ClanId = 0;
		tmp[0].m_RankInClan = 0;
		tmp[0].m_OnlineStatus = 0;

		*profiles = tmp;
		*dstProfileCount = 0;
	}
	else
	{
		ulong count = (ulong)query.StmtNumRows();
		DatabaseLog("%s: %d profiles found", email, count);
		querySuccess = true;

		if (count < 1)
		{
			MMG_Profile *tmp = new MMG_Profile[1];

			tmp[0].m_ProfileId = 0;
			wcscpy_s(tmp[0].m_Name, L"");
			tmp[0].m_Rank = 0;
			tmp[0].m_ClanId = 0;
			tmp[0].m_RankInClan = 0;
			tmp[0].m_OnlineStatus = 0;

			*profiles = tmp;
			*dstProfileCount = 0;
		}
		else
		{
			MMG_Profile *tmp = new MMG_Profile[count];
			int i = 0;

			while(query.StmtFetch())
			{
				tmp[i].m_ProfileId = id;
				wcscpy_s(tmp[i].m_Name, name);
				tmp[i].m_Rank = rank;
				tmp[i].m_ClanId = clanid;
				tmp[i].m_RankInClan = rankinclan;
				tmp[i].m_OnlineStatus = 0;

				i++;
			}

			*profiles = tmp;
			*dstProfileCount = count;
		}
	}
	
	return querySuccess;
}

bool MySQLDatabase::QueryUserOptions(const uint profileId, int *options)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	//TODO
	return true;
}

bool MySQLDatabase::SaveUserOptions(const uint profileId, const int options)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "UPDATE mg_profiles SET commoptions = ? WHERE id = ?");

	// prepared statement binding structures
	MYSQL_BIND params[2];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	bool querySuccess;

	// bind parameters to prepared statement
	query.Bind(&params[0], &options);
	query.Bind(&params[1], &profileId);

	// execute prepared statement
	if(!query.StmtExecute(params))
	{
		DatabaseLog("SaveUserOptions() failed: profile id(%d), options(%d)", profileId, options);
		querySuccess = false;
	}
	else
	{
		DatabaseLog("profile id(%d) set commoptions(%d)", profileId, options);
		querySuccess = true;
	}

	return querySuccess;
}

bool MySQLDatabase::QueryFriends(const uint profileId, uint *dstProfileCount, uint *friendIds[])
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT friendprofileid from mg_friends WHERE profileid = ? LIMIT 64");

	// prepared statement binding structures
	MYSQL_BIND param[1], result[1];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(result, 0, sizeof(result));

	// query specific variables
	bool querySuccess;
	uint id;

	// bind parameters to prepared statement
	query.Bind(&param[0], &profileId);		//profileid

	// bind results
	query.Bind(&result[0], &id);		//mg_friends.friendprofileid

	// execute prepared statement
	if(!query.StmtExecute(param, result))
	{
		DatabaseLog("QueryFriends() query failed: %s", profileId);
		querySuccess = false;

		uint *tmp = new uint[1];

		tmp[0] = 0;

		*friendIds = tmp;
		*dstProfileCount = 0;
	}
	else
	{
		ulong count = (ulong)query.StmtNumRows();
		DatabaseLog("profile id(%d), %d friends found", profileId, count);
		querySuccess = true;

		if (count < 1)
		{
			uint *tmp = new uint[1];

			tmp[0] = 0;

			*friendIds = tmp;
			*dstProfileCount = 0;
		}
		else
		{
			uint *tmp = new uint[count];
			int i = 0;

			while(query.StmtFetch())
			{
				tmp[i] = id;
				i++;
			}

			*friendIds = tmp;
			*dstProfileCount = count;
		}
	}

	return querySuccess;
}

bool MySQLDatabase::AddFriend(const uint profileId, uint friendProfileId)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "INSERT INTO mg_friends (profileid, friendprofileid) VALUES (?, ?)");

	// prepared statement binding structures
	MYSQL_BIND params[2];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	bool querySuccess;
	uint friend_insert_id;

	// bind parameters to prepared statement
	query.Bind(&params[0], &profileId);			//mg_friends.profileid
	query.Bind(&params[1], &friendProfileId);	//mg_friends.friendprofileid

	// execute prepared statement
	if (!query.StmtExecute(params))
	{
		DatabaseLog("AddFriend() query failed: %d", profileId);
		friend_insert_id = 0;
		querySuccess = false;
	}
	else
	{
		DatabaseLog("profileid (%d) added friendprofileid(%d)", profileId, friendProfileId);
		friend_insert_id = (uint)query.StmtInsertId();
		querySuccess = true;
	}
	
	return querySuccess;
}

bool MySQLDatabase::RemoveFriend(const uint profileId, uint friendProfileId)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "DELETE FROM mg_friends WHERE profileid = ? AND friendprofileid = ?");

	// prepared statement binding structures
	MYSQL_BIND params[2];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));
	
	// query specific variables
	bool querySuccess;

	// bind parameters to prepared statement
	query.Bind(&params[0], &profileId);			//mg_friends.profileid
	query.Bind(&params[1], &friendProfileId);	//mg_friends.friendprofileid

	// execute prepared statement
	if (!query.StmtExecute(params))
	{
		DatabaseLog("RemoveFriend() query failed: %d", profileId);
		querySuccess = false;
	}
	else
	{
		DatabaseLog("profileid (%d) remove friendprofileid(%d)", profileId, friendProfileId);
		querySuccess = true;
	}
	
	return querySuccess;
}

bool MySQLDatabase::QueryAcquaintances(const uint profileId, uint *dstProfileCount, uint *acquaintanceIds[])
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// TODO:
	// this function is temporary
	// must fix when stats are implemented

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT id from mg_profiles WHERE id <> ? LIMIT 100");

	// prepared statement binding structures
	MYSQL_BIND param[1], result[1];
	
	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(result, 0, sizeof(result));

	// query specific variables
	bool querySuccess;
	uint id;

	// bind parameters to prepared statement
	query.Bind(&param[0], &profileId);		//profileid

	// bind results
	query.Bind(&result[0], &id);			//mg_profiles.id

	// execute prepared statement
	if(!query.StmtExecute(param, result))
	{
		DatabaseLog("QueryAcquaintances() query failed: %s", profileId);
		querySuccess = false;

		uint *tmp = new uint[1];

		tmp[0] = 0;

		*acquaintanceIds = tmp;
		*dstProfileCount = 0;
	}
	else
	{
		ulong count = (ulong)query.StmtNumRows();
		DatabaseLog("profile id(%d), %d acquaintances found", profileId, count);
		querySuccess = true;

		if (count < 1)
		{
			uint *tmp = new uint[1];

			tmp[0] = 0;

			*acquaintanceIds = tmp;
			*dstProfileCount = 0;
		}
		else
		{
			uint *tmp = new uint[count];
			int i = 0;

			while(query.StmtFetch())
			{
				tmp[i] = id;
				i++;
			}

			*acquaintanceIds = tmp;
			*dstProfileCount = count;
		}
	}

	return querySuccess;
}

bool MySQLDatabase::QueryIgnoredProfiles(const uint profileId, uint *dstProfileCount, uint *ignoredIds[])
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT ignoredprofileid from mg_ignored WHERE profileid = ? LIMIT 64");

	// prepared statement binding structures
	MYSQL_BIND param[1], result[1];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(result, 0, sizeof(result));
	
	// query specific variables
	bool querySuccess;
	uint id;

	// bind parameters to prepared statement
	query.Bind(&param[0], &profileId);		//profileid

	// bind results
	query.Bind(&result[0], &id);		//mg_ignored.ignoredprofileid

	// execute prepared statement
	if(!query.StmtExecute(param, result))
	{
		DatabaseLog("QueryIgnoredProfiles() query failed: %s", profileId);
		querySuccess = false;

		uint *tmp = new uint[1];

		tmp[0] = 0;

		*ignoredIds = tmp;
		*dstProfileCount = 0;
	}
	else
	{
		ulong count = (ulong)query.StmtNumRows();
		DatabaseLog("profile id(%d), %d ignored profiles found", profileId, count);
		querySuccess = true;

		if (count < 1)
		{
			uint *tmp = new uint[1];

			tmp[0] = 0;

			*ignoredIds = tmp;
			*dstProfileCount = 0;
		}
		else
		{
			uint *tmp = new uint[count];
			int i = 0;

			while(query.StmtFetch())
			{
				tmp[i] = id;
				i++;
			}

			*ignoredIds = tmp;
			*dstProfileCount = count;
		}
	}

	return querySuccess;
}

bool MySQLDatabase::AddIgnoredProfile(const uint profileId, uint ignoredProfileId)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "INSERT INTO mg_ignored (profileid, ignoredprofileid) VALUES (?, ?)");

	// prepared statement binding structures
	MYSQL_BIND params[2];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	bool querySuccess;
	uint ignored_insert_id;

	// bind parameters to prepared statement
	query.Bind(&params[0], &profileId);			//mg_ignored.profileid
	query.Bind(&params[1], &ignoredProfileId);	//mg_ignored.ignoredprofileid

	// execute prepared statement
	if (!query.StmtExecute(params))
	{
		DatabaseLog("AddIgnoredProfile() query failed: %d", profileId);
		ignored_insert_id = 0;
		querySuccess = false;
	}
	else
	{
		DatabaseLog("profileid (%d) added ingoredprofileid(%d)", profileId, ignoredProfileId);
		ignored_insert_id = (uint)query.StmtInsertId();
		querySuccess = true;
	}
	
	return querySuccess;
}

bool MySQLDatabase::RemoveIgnoredProfile(const uint profileId, uint ignoredProfileId)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "DELETE FROM mg_ignored WHERE profileid = ? AND ignoredprofileid = ?");

	// prepared statement binding structures
	MYSQL_BIND params[2];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	bool querySuccess;

	// bind parameters to prepared statement
	query.Bind(&params[0], &profileId);			//mg_ignored.profileid
	query.Bind(&params[1], &ignoredProfileId);	//mg_ignored.ignoredprofileid

	// execute prepared statement
	if (!query.StmtExecute(params))
	{
		DatabaseLog("RemoveIgnoredProfile() query failed: %d", profileId);
		querySuccess = false;
	}
	else
	{
		DatabaseLog("profileid (%d) remove ignoredprofileid(%d)", profileId, ignoredProfileId);
		querySuccess = true;
	}
	
	return querySuccess;
}

bool MySQLDatabase::QueryProfileName(const uint profileId, MMG_Profile *profile)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT id, name, rank, clanid, rankinclan FROM mg_profiles WHERE id = ? LIMIT 1");

	// prepared statement binding structures
	MYSQL_BIND param[1], results[5];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(results, 0, sizeof(results));

	// query specific variables
	bool querySuccess;

	wchar_t name[WIC_NAME_MAX_LENGTH];
	memset(name, 0, sizeof(name));
	ulong nameLength = ARRAYSIZE(name);

	uint id, clanid;
	uchar rank, rankinclan;

	// bind parameters to prepared statement
	query.Bind(&param[0], &profileId);			//profile id

	// bind results
	query.Bind(&results[0], &id);				//mg_profiles.id
	query.Bind(&results[1], name, &nameLength);	//mg_profiles.name
	query.Bind(&results[2], &rank);				//mg_profiles.rank
	query.Bind(&results[3], &clanid);			//mg_profiles.clanid
	query.Bind(&results[4], &rankinclan);		//mg_profiles.rankinclan

	// execute prepared statement
	if(!query.StmtExecute(param, results))
	{
		DatabaseLog("QueryProfileName() failed: profile id(%d)", profileId);
		querySuccess = false;

		profile->m_ProfileId = 0;
		wcscpy_s(profile->m_Name, L"");
		profile->m_Rank = 0;
		profile->m_ClanId = 0;
		profile->m_RankInClan = 0;
		profile->m_OnlineStatus = 0;
	}
	else
	{
		if (!query.StmtFetch())
		{
			DatabaseLog("profile id(%d) not found", profileId);
			querySuccess = true;

			profile->m_ProfileId = 0;
			wcscpy_s(profile->m_Name, L"");
			profile->m_Rank = 0;
			profile->m_ClanId = 0;
			profile->m_RankInClan = 0;
			profile->m_OnlineStatus = 0;
		}
		else
		{
			DatabaseLog("profile id(%d) %ws found", profileId, name);
			querySuccess = true;

			//profile
			profile->m_ProfileId = id;
			wcscpy_s(profile->m_Name, name);
			profile->m_Rank = rank;
			profile->m_ClanId = clanid;
			profile->m_RankInClan = rankinclan;
			profile->m_OnlineStatus = 0;
		}
	}

	return querySuccess;
}

bool MySQLDatabase::QueryProfileList(const size_t Count, const uint *profileIds, MMG_Profile *profiles)
{
	for (uint i = 0; i < Count; i++)
	{
		if (!this->QueryProfileName(profileIds[i], &profiles[i]))
			return false;
	}

	return true;
}

bool MySQLDatabase::QueryEditableVariables(const uint profileId, wchar_t *dstMotto, wchar_t *dstHomepage)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT motto, homepage FROM mg_profiles WHERE id = ? LIMIT 1");
	
	// prepared statement binding structures
	MYSQL_BIND param[1], results[2];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(results, 0, sizeof(results));

	// query specific variables
	bool querySuccess;

	wchar_t motto[WIC_MOTTO_MAX_LENGTH], homepage[WIC_HOMEPAGE_MAX_LENGTH];
	memset(motto, 0, sizeof(motto));
	memset(homepage, 0, sizeof(homepage));

	ulong mottoLength = ARRAYSIZE(motto);
	ulong homepageLength = ARRAYSIZE(homepage);

	// bind parameters to prepared statement
	query.Bind(&param[0], &profileId);					//profile id

	// bind results
	query.Bind(&results[0], motto, &mottoLength);			//mg_profiles.motto
	query.Bind(&results[1], homepage, &homepageLength);	//mg_profiles.homepage

	// execute prepared statement
	if(!query.StmtExecute(param, results))
	{
		DatabaseLog("QueryEditableVariables() failed: profile id(%d)", profileId);
		querySuccess = false;

		dstMotto = L"";
		dstHomepage = L"";
	}
	else
	{
		if (!query.StmtFetch())
		{
			DatabaseLog("profile id(%d) not found", profileId);
			querySuccess = true;

			dstMotto = L"";
			dstHomepage = L"";
		}
		else
		{
			DatabaseLog("profile id(%d) found", profileId);
			querySuccess = true;

			wcscpy_s(dstMotto, ARRAYSIZE(motto), motto);
			wcscpy_s(dstHomepage, ARRAYSIZE(homepage), homepage);
		}
	}

	return querySuccess;
}

bool MySQLDatabase::SaveEditableVariables(const uint profileId, const wchar_t *motto, const wchar_t *homepage)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "UPDATE mg_profiles SET motto = ?, homepage = ? WHERE id = ?");

	// prepared statement binding structures
	MYSQL_BIND params[3];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	bool querySuccess;

	ulong mottoLength = wcslen(motto);
	ulong homepageLength = wcslen(homepage);

	// bind parameters to prepared statement
	// if either string is empty BindNull
	if (mottoLength)
		query.Bind(&params[0], motto, &mottoLength);
	else
		query.BindNull(&params[0]);

	if (homepageLength)
		query.Bind(&params[1], homepage, &homepageLength);
	else
		query.BindNull(&params[1]);

	query.Bind(&params[2], &profileId);

	// execute prepared statement
	if(!query.StmtExecute(params))
	{
		DatabaseLog("SaveEditableVariables() failed: profile id(%d)", profileId);
		querySuccess = false;
	}
	else
	{
		DatabaseLog("SaveEditableVariables() success: profile id(%d)", profileId);
		querySuccess = true;
	}

	return querySuccess;
}

bool MySQLDatabase::QueryPendingMessages(const uint profileId, uint *dstMessageCount, MMG_InstantMessageListener::InstantMessage *messages[])
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "SELECT id, writtenat, senderprofileid, recipientprofileid, message from mg_messages WHERE recipientprofileid = ? ORDER BY writtenat ASC LIMIT 20");

	// prepared statement binding structures
	MYSQL_BIND param[1], results[5];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));
	memset(results, 0, sizeof(results));
	
	// query specific variables
	wchar_t message[256];
	memset(message, 0, sizeof(message));
	ulong msgLength = ARRAYSIZE(message);

	uint id, senderid, recipientid;
	MYSQL_TIME writtenat;

	//
	// TODO
	// note: temporarily limiting returned pending messages to 20, real limit for the client is unknown
	// m_WrittenAt = number of seconds since midnight UTC, the client adjusts timezone aswell
	//

	// bind parameters to prepared statement
	query.Bind(&param[0], &profileId);		//profileid

	// bind results
	query.Bind(&results[0], &id);					//mg_messages.id
	query.BindTimeStamp(&results[1], &writtenat);	//mg_messages.writtenat
	query.Bind(&results[2], &senderid);				//mg_messages.senderprofileid
	query.Bind(&results[3], &recipientid);			//mg_messages.recipientprofileid
	query.Bind(&results[4], message, &msgLength);	//mg_messages.message

	// execute prepared statement
	if(!query.StmtExecute(param, results))
	{
		DatabaseLog("QueryPendingMessages() failed: profile id(%d)", profileId);

		//messages = NULL;
		*dstMessageCount = 0;
	}
	else
	{
		ulong count = (ulong)query.StmtNumRows();
		DatabaseLog("found %d pending messages for profile id(%d)", count, profileId);

		*dstMessageCount = 0;

		if (count > 0)
		{
			MMG_InstantMessageListener::InstantMessage *tmp = new MMG_InstantMessageListener::InstantMessage[count];
			int i = 0;

			while(query.StmtFetch())
			{
				tmp[i].m_MessageId = id;
				tmp[i].m_WrittenAt = 0; // TODO
				this->QueryProfileName(senderid, &tmp[i].m_SenderProfile);
				tmp[i].m_RecipientProfile = recipientid;
				wcscpy_s(tmp[i].m_Message, message);

				i++;
			}

			*messages = tmp;
			*dstMessageCount = count;
		}
	}

	return query.Success();
}

bool MySQLDatabase::AddInstantMessage(const uint profileId, MMG_InstantMessageListener::InstantMessage *message)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "INSERT INTO mg_messages (writtenat, senderprofileid, recipientprofileid, message) VALUES (NOW(), ?, ?, ?)");

	// prepared statement binding structures
	MYSQL_BIND params[3];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	ulong msgLength = wcslen(message->m_Message);
	MYSQL_TIME datetime;

	// TODO store time as utc format
	// might have to store as int rather than timestamp

	// bind parameters to prepared statement
	query.Bind(&params[0], &message->m_SenderProfile.m_ProfileId);
	query.Bind(&params[1], &message->m_RecipientProfile);
	query.Bind(&params[2], message->m_Message, &msgLength);

	// execute prepared statement
	if (!query.StmtExecute(params))
	{
		DatabaseLog("AddInstantMessage() query failed: profile id(%d)", profileId);
		message->m_MessageId = 0;
	}
	else
	{
		message->m_MessageId = (uint)query.StmtInsertId();
		//DatabaseLog("message id(%d) queued:", message->m_MessageId);
	}

	return query.Success();
}

bool MySQLDatabase::RemoveInstantMessage(const uint messageId)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "DELETE FROM mg_messages WHERE id = ?");  // AND recipientprofileid = ?

	// prepared statement binding structures
	MYSQL_BIND param[1];

	// initialize (zero) bind structures
	memset(param, 0, sizeof(param));

	// bind parameters to prepared statement
	query.Bind(&param[0], &messageId);			//mg_messages.id

	// execute prepared statement
	if (!query.StmtExecute(param))
		DatabaseLog("RemoveInstantMessage() query failed: message id(%d)", messageId);
	//else
	//	DatabaseLog("message id(%d) acked", messageId);

	return query.Success();
}

bool MySQLDatabase::AddAbuseReport(const uint profileId, const uint flaggedProfile, const wchar_t *report)
{
	// test the connection before proceeding, disconnects everyone on fail
	if (!this->TestDatabase())
	{
		this->EmergencyMassgateDisconnect();
		return false;
	}

	// prepared statement wrapper object
	MySQLQuery query(this->m_Connection, "INSERT INTO mg_abusereports (senderprofileid, reportedprofileid, report, datereported) VALUES (?, ?, ?, NOW())");

	// prepared statement binding structures
	MYSQL_BIND params[3];

	// initialize (zero) bind structures
	memset(params, 0, sizeof(params));

	// query specific variables
	ulong reportLength = wcslen(report);

	// bind parameters to prepared statement
	query.Bind(&params[0], &profileId);
	query.Bind(&params[1], &flaggedProfile);
	query.Bind(&params[2], report, &reportLength);

	// execute prepared statement
	if (!query.StmtExecute(params))
		DatabaseLog("AddAbuseReport() query failed: profileid(%d)", profileId);
	else
		DatabaseLog("player id(%d) reported player id(%d):", profileId, flaggedProfile);

	return query.Success();
}