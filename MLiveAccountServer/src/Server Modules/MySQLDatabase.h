#pragma once

/*
	MySQL C API - Documentation
	https://dev.mysql.com/doc/refman/5.7/en/c-api.html

	MySQL C API Prepared Statements
	https://dev.mysql.com/doc/refman/5.7/en/c-api-prepared-statements.html

	Taken directly from the documentation:
	1)	To obtain a statement handle, pass a MYSQL connection handler to mysql_stmt_init(), which returns a pointer to a MYSQL_STMT data structure. 
		This structure is used for further operations with the statement. 
		To specify the statement to prepare, pass the MYSQL_STMT pointer and the statement string to mysql_stmt_prepare().
			- The string must consist of a single SQL statement. You should not add a terminating semicolon (";") or \g to the statement.
			  See: https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-prepare.html

		*The MYSQLQuery constuctor calls mysql_stmt_init() and mysql_stmt_prepare()

	2)	To provide input parameters for a prepared statement, set up MYSQL_BIND structures and pass them to mysql_stmt_bind_param(). 
		To receive output column values, set up MYSQL_BIND structures and pass them to mysql_stmt_bind_result().
		See:https://dev.mysql.com/doc/refman/5.7/en/c-api-prepared-statement-data-structures.html

		*after using Bind(), you can use StmtExecute() to execute your prepared statement
		*otherwise you can use Step() if its not a prepared statement
		*use StmtFetch() to get the results

	3)	Multiple statement handles can be associated with a single connection. The limit on the number of handles depends on the available system resources.

	4)	To use a MYSQL_BIND structure, zero its contents to initialize it, then set its members appropriately.

		*Bind has been overloaded for ease of use, to bind dates, nulls, and blobs, use BindDate(), BindNull() and BindBlob()

		*to use a statement that returns a result set, call StmtFetch() after calling StmtExecute()

	*More Information about threaded clients
	https://dev.mysql.com/doc/refman/5.7/en/c-api-threaded-clients.html
	https://dev.mysql.com/doc/refman/5.7/en/mysql-thread-init.html
	https://dev.mysql.com/doc/refman/5.7/en/mysql-thread-end.html

	Notes:
	The MySQLQuery class is basically a wrapper that utilises all of the functions needed to execute a prepared statement
	Massgate connects to the database on startup and leaves the connection open, periodically
	pinging it to keep it from dropping due to inactivity.

	There could be an issue when clients start querying the database at the same time.
	so far there have been no data corruptions.

	the ping function can affect the connection state, more info:
			http://dev.mysql.com/doc/refman/5.7/en/mysql-ping.html
			http://dev.mysql.com/doc/refman/5.7/en/auto-reconnect.html

	TODO:
		experience "experience int(10) unsigned not null,"
	- better error handling of queries (ie querySuccess)
		- CreateUserAccount
		- CreateUserProfile
		- DeleteUserProfile
		- QueryUserProfile
	- handle communication options properly
	- create a global 'logged in' player list/map
		MMG_AccountProtocol is messy, move account database queries to Query::FromStream
	- change mg_cdkeys from cipherkeys to sequencenumber?
	- maybe start using InnoDB/transactions (START TRANSACTION/BEGIN, COMMIT, ROLLBACK)/autocommit=0, 
		indexing (any WHERE column = ?), replication, utf-8
	- fix the message display time for instant messaging

*/

#ifndef USING_MYSQL_DATABASE
//#define USING_MYSQL_DATABASE
#endif

CLASS_SINGLE(MySQLDatabase)
{
private:
	//mysql connection settings
	char* host;
	char* user;
	char* pass;
	char* db;

	// mysql connection object
	MYSQL *m_Connection;
	bool isConnected;

	// mysql connection options
	my_bool reconnect;
	char *bind_interface;
	char *charset_name;

	unsigned long sleep_interval_h;
	unsigned long sleep_interval_m;
	unsigned long sleep_interval_s;		//connection timeout for a default mysql install is 28800 seconds (8 hours)
	unsigned long sleep_interval_ms;

	HANDLE m_PingThreadHandle;

	static DWORD WINAPI PingThread(LPVOID lpArg);

public:
	MySQLDatabase()
	{
		this->host = NULL;
		this->user = NULL;
		this->pass = NULL;
		this->db = NULL;

		this->m_Connection = NULL;
		this->isConnected = false;

		// mysql connection options
		this->reconnect = 1;
		this->bind_interface = "localhost";
		this->charset_name = "latin1";		//TODO: change to utf8

		this->sleep_interval_h = 5;
		this->sleep_interval_m = this->sleep_interval_h * 60;
		this->sleep_interval_s = this->sleep_interval_m * 60;
		this->sleep_interval_ms = this->sleep_interval_s * 1000;

		this->m_PingThreadHandle = NULL;
	}

	~MySQLDatabase()
	{
		this->Unload();
	}

private:
	bool	EmergencyMassgateDisconnect		();

	bool	ReadConfig			(const char *filename);
	bool	ConnectDatabase		();
	void	PrintStatus			();
	bool	TestDatabase		();
	bool	PingDatabase		();

public:
	bool	Initialize			();
	void	Unload				();
	bool	HasConnection		();
	bool	InitializeSchema	();
	
	//accounts
	bool	CheckIfEmailExists	(const char *email, uint *dstId);
	bool	CheckIfCDKeyExists	(const ulong cipherKeys[], uint *dstId);
	bool	CreateUserAccount	(const char *email, const char *password, const char *country, const uchar *emailgamerelated, const uchar *acceptsemail, const ulong cipherKeys[]);
	bool	AuthUserAccount		(const char *email, char *dstPassword, uchar *dstIsBanned, MMG_AuthToken *authToken);
	
	//profiles (logins)
	bool	CheckIfProfileExists	(const wchar_t* name, uint *dstId);
	bool	CreateUserProfile	(const uint accountId, const wchar_t* name, const char* email);
	bool	DeleteUserProfile	(const uint accountId, const uint profileId, const char* email);
	bool	QueryUserProfile	(const uint accountId, const uint profileId, MMG_Profile *profile, MMG_Options *options);
	bool	RetrieveUserProfiles	(const char *email, const char *password, ulong *dstProfileCount, MMG_Profile *profiles[]);

	//messaging
	bool	QueryUserOptions	(const uint profileId, int *options);	//TODO
	bool	SaveUserOptions		(const uint profileId, const int options);
	bool	QueryFriends		(const uint profileId, uint *dstProfileCount, uint *friendIds[]);
	bool	AddFriend			(const uint profileId, uint friendProfileId);
	bool	RemoveFriend		(const uint profileId, uint friendProfileId);
	bool	QueryAcquaintances	(const uint profileId, uint *dstProfileCount, uint *acquaintanceIds[]);
	bool	QueryIgnoredProfiles	(const uint profileId, uint *dstProfileCount, uint *ignoredIds[]);
	bool	AddIgnoredProfile		(const uint profileId, uint ignoredProfileId);
	bool	RemoveIgnoredProfile	(const uint profileId, uint ignoredProfileId);
	bool	QueryProfileName	(const uint profileId, MMG_Profile *profile);
	bool	QueryProfileList	(const size_t Count, const uint *profileId, MMG_Profile *profiles);
	bool	QueryEditableVariables	(const uint profileId, wchar_t *dstMotto, wchar_t *dstHomepage);
	bool	SaveEditableVariables	(const uint profileId, const wchar_t *motto, const wchar_t *homepage);
	bool	QueryPendingMessages	(const uint profileId, uint *dstMessageCount, MMG_InstantMessageListener::InstantMessage *messages[]);
	bool	AddInstantMessage		(const uint profileId, MMG_InstantMessageListener::InstantMessage *message);
	bool	RemoveInstantMessage	(const uint messageId);
	bool	AddAbuseReport		(const uint profileId, const uint flaggedProfile, const wchar_t *report);
};