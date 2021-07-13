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
	- reduce to one query per function:
		- CreateUserProfile
		- DeleteUserProfile
		- QueryUserProfile
	- better error message output

*/

// MMG_FriendsLadderProtocol
struct LadderEntry
{
	uint m_ProfileId;
	uint m_Score;
};

// MMG_IFriendsListener
struct Acquaintance
{
	uint m_ProfileId;
	uint m_NumTimesPlayed;
};

#ifndef USING_MYSQL_DATABASE
//#define USING_MYSQL_DATABASE
//#define USING_PROFILE_GUESTBOOK
//#define USING_CLAN_GUESTBOOK
#endif

#define ROUND_TIME(time_as_int) (time_as_int / 86400) * 86400;

const unsigned int MAX_CONFIG_OPTIONS		= 16;
const unsigned int MAX_CONFIG_BUFFER_LENGTH	= 64;

CLASS_SINGLE(MySQLDatabase)
{
private:

	enum
	{
		ABUSEREPORTS_TABLE,
		ACCOUNTS_TABLE,
		ACQUAINTANCES_TABLE,
		CDKEYS_TABLE,
		CDKEYSPRIVATE_TABLE,
		CLAN_GB_TABLE,
		CLANS_TABLE,
		FRIENDS_TABLE,
		IGNORED_TABLE,
		MESSAGES_TABLE,
		PLAYER_LADDER_TABLE,
		PLAYER_MATCHSTATS_TABLE,
		PROFILE_GB_TABLE,
		PROFILES_TABLE,
		SERVERKEYS_TABLE,

		TOTAL_TABLES
	};

	char TABLENAME[TOTAL_TABLES][MAX_CONFIG_BUFFER_LENGTH];
	char host[MAX_CONFIG_BUFFER_LENGTH];
	char user[MAX_CONFIG_BUFFER_LENGTH];
	char pass[MAX_CONFIG_BUFFER_LENGTH];
	char db[MAX_CONFIG_BUFFER_LENGTH];
	char charset_name[MAX_CONFIG_BUFFER_LENGTH];
	char bind_interface[MAX_CONFIG_BUFFER_LENGTH];
	my_bool auto_reconnect;

	MYSQL *m_Connection;
	bool isConnected;

	unsigned long sleep_interval_h;
	unsigned long sleep_interval_m;
	unsigned long sleep_interval_s;
	unsigned long sleep_interval_ms;

	HANDLE m_PingThreadHandle;

	static DWORD WINAPI PingThread(LPVOID lpArg);

public:
	MySQLDatabase();
	~MySQLDatabase();

	bool	HasConnection		();
	bool	PingDatabase		();
	bool	EmergencyMassgateDisconnect		();

	bool	Initialize			();
	void	Unload				();
	bool	ReadConfig			(const char *filename);

	bool	ConnectDatabase		();
	bool	ReadDatabaseSchema	();
	bool	TestDatabaseTables	();
	bool	PrintDatabaseInfo	();
	bool	ResetOnlineStatus	();

	bool	TestDatabase		();
	void	BeginTransaction	();
	void	RollbackTransaction	();
	void	CommitTransaction	();

	// MMG_AccountProtocol
	bool	CheckIfEmailExists	(const char *email, uint *dstId);
	bool	CheckIfCDKeyExists			(const uint sequenceNum, uint *dstId);
	bool	CheckIfPrivateCDKeyUser		(const uint sequencenum, uint *id, char *email, uchar *validated);
	bool	AuthPrivateCDKey			(const uint sequencenum, const char *email, uint *id, uint *accountid);
	bool	UpdatePrivateCDKeyAccountID	(const uint sequencenum, const char *email, const uint accountid);
	bool	InsertUserAccount			(const char *email, const char *password, const char *country, const char *realcountry, const uchar *emailgamerelated, const uchar *acceptsemail, uint *accountInsertId);
	bool	InsertUserCDKeyInfo			(const uint accountId, const uint sequenceNum, const ulong cipherKeys[]);
	bool	CreateUserAccount			(const bool isPrivateKeyUser, const char *email, const char *password, const char *country, const char *realcountry, const uchar *emailgamerelated, const uchar *acceptsemail, const uint sequenceNum, const ulong cipherKeys[]);
	bool	QueryUserAccount			(const char *email, char *dstPassword, uchar *dstIsBanned, MMG_AuthToken *authToken);
	bool	QueryUserCDKeyId			(const uint accountId, MMG_AuthToken *authToken);
	bool	AuthUserAccount				(const char *email, char *dstPassword, uchar *dstIsBanned, MMG_AuthToken *authToken);
	bool	UpdateRealCountry			(const uint accountId, const ulong ipaddress);
	bool	UpdateCDKeyInfo				(const uint accountId, const uint sequenceNum, const ulong cipherKeys[]);
	bool	UpdatePassword				(const uint accountId, const char *password);
	bool	CheckIfProfileExists	(const wchar_t* name, uint *dstId);
	bool	CreateUserProfile	(const uint accountId, const wchar_t* name, const char* email);
	bool	DeleteUserProfile	(const uint accountId, const uint profileId, const char* email);
	bool	QueryProfileCreationDate	(const uint profileId, uint *membersince);
	bool	QueryPreorderStatus			(const uint accountId, uchar *isPreorder);
	bool	QueryAskRecruitedQuestion	(const uint accountId, uchar *askrecruitedquestion);
	bool	QueryNumFriendsRecruited	(const uint accountId, uint *numFriendsRecruited);
	bool	QueryActiveProfileId		(const uint accountId, uint *activeprofileid);
	bool	UpdateNumFriendsRecruited	(const uint accountId, const uint numfriendsrecruited);
	bool	IncNumFriendsRecruited		(const uint accountId);
	bool	AddValidRecruiterMessage	(const uint accountId, const uint profileId);
	bool	UpdateAskRecruitedQuestion	(const uint accountId, const uchar askrecruitedquestion);
	bool	AddRecruitedQuestionMessage	(const uint accountId, const uint profileId);
	bool	UpdateMembershipBadges		(const uint accountId, const uint profileId);
	bool	QueryUserProfile	(const uint accountId, const uint profileId, MMG_Profile *profile);
	bool	RetrieveUserProfiles		(const uint accountId, ulong *dstProfileCount, MMG_Profile *profiles);

	// MMG_Messaging: profile related
	bool	AppendClanTag				(MMG_Profile *profile);
	bool	QueryClanTag				(const uint clanId, wchar_t *shortclanname, char *displaytag);
	bool	QueryProfileName			(const uint profileId, MMG_Profile *profile);
	bool	QueryProfileList			(const size_t Count, const uint profileIds[], MMG_Profile profiles[]);
	bool	UpdateProfileOnlineStatus	(const uint profileId, const uint onlinestatus);
	bool	UpdateProfileRank			(const uint profileId, const uchar rank);
	bool	UpdateProfileClanId			(const uint profileId, const uint clanId);
	bool	UpdateProfileClanRank		(const uint profileId, const uchar rankInClan);

	// MMG_Messaging
	bool	AddSystemMessage			(const uint profileId, const wchar_t *message);
	bool	QueryUserOptions	(const uint profileId, uint *options);
	bool	SaveUserOptions		(const uint profileId, const uint options);
	bool	QueryFriends				(const uint profileId, uint *dstProfileCount, uint friendIds[]);
	bool	AddFriend			(const uint profileId, uint friendProfileId);
	bool	RemoveFriend		(const uint profileId, uint friendProfileId);
	bool	QueryAcquaintances			(const uint profileId, uint *dstCount, Acquaintance acquaintances[]);
	bool	QueryIgnoredProfiles		(const uint profileId, uint *dstProfileCount, uint ignoredIds[]);
	bool	AddIgnoredProfile		(const uint profileId, uint ignoredProfileId);
	bool	RemoveIgnoredProfile	(const uint profileId, uint ignoredProfileId);
	bool	QueryPlayerSearch			(const wchar_t* const name, const uint maxResults, uint *dstCount, uint profileIds[]);
	bool	QueryClanSearch				(const wchar_t* const name, const uint maxResults, uint *dstCount, MMG_Clan::Description clans[]);
	bool	QueryEditableVariables	(const uint profileId, wchar_t *dstMotto, wchar_t *dstHomepage);
	bool	SaveEditableVariables	(const uint profileId, const wchar_t *motto, const wchar_t *homepage);
	bool	QueryPendingMessages		(const uint profileId, uint *dstMessageCount, MMG_InstantMessageListener::InstantMessage *messages);
	bool	InsertInstantMessage		(MMG_InstantMessageListener::InstantMessage *message);
	bool	AddInstantMessage			(const uint profileId, MMG_InstantMessageListener::InstantMessage *message);
	bool	DeleteInstantMessage		(const uint profileId, const uint messageId);
	bool	AddAbuseReport				(const uint profileId, const MMG_Profile senderProfile, const uint flaggedProfileId, const wchar_t *report);
	bool	QueryProfileAccountId		(const uint profileId, uint *dstAccountId);
	bool	QueryProfileGuestbook		(const uint profileId, uint *dstEntryCount, MMG_ProfileGuestBookProtocol::GetRsp *guestbook);
	bool	AddProfileGuestbookEntry	(const uint profileId, const uint requestId, MMG_ProfileGuestBookProtocol::GetRsp::GuestbookEntry *entry);
	bool	DeleteProfileGuestbookEntry	(const uint profileId, const uint messageId, const uchar deleteAllByProfile);

	// MMG_Messaging: clans & clan related
	bool	CheckIfClanNameExists		(const wchar_t* clanname, uint *dstId);
	bool	CheckIfClanTagExists		(const wchar_t* clantag, uint *dstId);
	bool	CreateClan					(const uint profileId, const wchar_t* clanname, const wchar_t* clantag, const char* displayTag, uint *dstId);
	bool	DeleteProfileClanInvites	(const uint profileId, const uint clanId);
	bool	DeleteProfileClanMessages	(const uint profileId);
	bool	UpdateClanPlayerOfWeek		(const uint clanId, const uint profileId);
	bool	DeleteClan					(const uint clanId);
	bool	QueryClanLeader				(const uint profileId, MMG_Profile *profile);
	bool	QueryClanFullInfo			(const uint clanId, uint *dstMemberCount, MMG_Clan::FullInfo *fullinfo);
	bool	QueryClanDescription		(const uint clanId, MMG_Clan::Description *description);
	bool	SaveClanEditableVariables	(const uint clanId, const uint profileId, const wchar_t *motto, const wchar_t *motd, const wchar_t *homepage);
	bool	CheckIfInvitedAlready		(const uint clanId, const uint inviteeProfileId, uint *dstId);
	bool	QueryClanGuestbook			(const uint clanId, uint *dstEntryCount, MMG_ClanGuestbookProtocol::GetRsp *guestbook);
	bool	AddClanGuestbookEntry		(const uint clanId, const uint requestId, MMG_ClanGuestbookProtocol::GetRsp::GuestbookEntry *entry);
	bool	DeleteClanGuestbookEntry	(const uint clanId, const uint messageId, const uchar deleteAllByProfile);

	// MMG_ServerTracker
	bool	QueryProfileLadderPosition	(const uint profileId, uint *position);
	bool	QueryPlayerLadderCount		(uint *dstTotalCount);
	bool	QueryPlayerLadder			(const uint startPos, const uint maxResults, uint *dstFoundItems, MMG_LadderProtocol::LadderRsp *ladder);
	bool	QueryPlayerLadder			(const uint profileId, uint *dstFoundItems, MMG_LadderProtocol::LadderRsp *ladder);
	bool	QueryProfileMedals			(const uint profileId, const size_t Count, MMG_Stats::Medal medals[]);
	bool	QueryProfileBadges			(const uint profileId, const size_t Count, MMG_Stats::Badge badges[]);
	bool	QueryProfileMedalsRawData	(const uint profileId, voidptr_t Data, ulong Length);
	bool	QueryProfileBadgesRawData	(const uint profileId, voidptr_t Data, ulong Length);
	bool	QueryProfileStats			(const uint profileId, MMG_Stats::PlayerStatsRsp *playerstats);
	bool	QueryProfileExtraStats		(const uint profileId, MMG_Stats::ExtraPlayerStats *extrastats);

	// MMG_TrackableServer
	bool	VerifyServerKey				(const uint sequenceNum, uint *dstId);
	bool	InsertPlayerMatchStats		(const uint datematchplayed, const MMG_Stats::PlayerMatchStats playerstats);
	bool	DeletePlayerLadder			();
	bool	InsertPlayerLadderItem		(const uint id, const uint profileid, const uint ladderscore);
	bool	GeneratePlayerLadderData	(const uint datematchplayed);
	bool	BuildPlayerLeaderboard		(const uint datematchplayed);
	bool	UpdateProfileMedals			(const uint profileId, const size_t Count, MMG_Stats::Medal medals[]);
	bool	UpdateProfileBadges			(const uint profileId, const size_t Count, MMG_Stats::Badge badges[]);
	bool	UpdateProfileMedalsRawData	(const uint profileId, voidptr_t Data, ulong Length);
	bool	UpdateProfileBadgesRawData	(const uint profileId, voidptr_t Data, ulong Length);
	bool	UpdateProfileMatchStats		(const uint profileId, const uint datematchplayed, const MMG_Stats::PlayerMatchStats *playerstats);
	bool	ProcessMatchStatistics		(const uint datematchplayed, const uint Count, const MMG_Stats::PlayerMatchStats playermatchstats[]);
	bool	CalculatePlayerRanks		(const uint Count, const uint profileIds[]);
	bool	InsertAcquaintances			(const uint datematchplayed, const uint Count, const uint profileIds[]);
};