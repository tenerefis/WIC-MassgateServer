#include "../stdafx.h"

void MMG_Messaging::IM_Settings::ToStream(MN_WriteMessage *aMessage)
{
	aMessage->WriteUChar(this->m_Friends);
	aMessage->WriteUChar(this->m_Clanmembers);
	aMessage->WriteUChar(this->m_Acquaintances);
	aMessage->WriteUChar(this->m_Anyone);
}

bool MMG_Messaging::IM_Settings::FromStream(MN_ReadMessage *aMessage)
{
	if (!aMessage->ReadUChar(this->m_Friends) || !aMessage->ReadUChar(this->m_Clanmembers))
		return false;

	if (!aMessage->ReadUChar(this->m_Acquaintances) || !aMessage->ReadUChar(this->m_Anyone))
		return false;

	return true;
}

MMG_Messaging::MMG_Messaging()
{
}

bool MMG_Messaging::HandleMessage(SvClient *aClient, MN_ReadMessage *aMessage, MMG_ProtocolDelimiters::Delimiter aDelimiter)
{
	MN_WriteMessage	responseMessage(2048);

	switch(aDelimiter)
	{
		case MMG_ProtocolDelimiters::MESSAGING_RETRIEVE_PROFILENAME:
		{
			DebugLog(L_INFO, "MESSAGING_RETRIEVE_PROFILENAME:");

			ushort count;
			if (!aMessage->ReadUShort(count))
				return false;

#ifndef USING_MYSQL_DATABASE

			//handle profiles (count).
			uint profileId1, profileId2;
			if (!aMessage->ReadUInt(profileId2) || !aMessage->ReadUInt(profileId1))
					return false;

			MMG_Profile friends[2];

			friends[0].m_ProfileId = profileId1;
			friends[1].m_ProfileId = profileId2;

			wcscpy_s(friends[0].m_Name, L"tenerefis");
			wcscpy_s(friends[1].m_Name, L"HouseBee");

			friends[0].m_OnlineStatus = 1;

			friends[0].m_Rank = 18;
			friends[1].m_Rank = 18;

			//friends[0].m_ClanId = 4321;
			//friends[1].m_ClanId = 4321;

			//friends[0].m_RankInClan = 2;
			//friends[1].m_RankInClan = 3;
			
			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_RESPOND_PROFILENAME);
			friends[0].ToStream(&responseMessage);

			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_RESPOND_PROFILENAME);
			friends[1].ToStream(&responseMessage);

			if (!aClient->SendData(&responseMessage))
				return false;
#else
			// TODO: limit may be 128, (MMG_Messaging::Update)
			if (count > 128)
				count = 128;

			uint *profildIds = NULL;
			MMG_Profile *profileList = NULL;

			profildIds = (uint*)malloc(count * sizeof(uint));
			memset(profildIds, 0, count * sizeof(uint));

			profileList = new MMG_Profile[count];

			// read profile ids from message
			for (ushort i = 0; i < count; i++)
			{
				if (!aMessage->ReadUInt(profildIds[i]))
					return false;
			}

			// query database
			MySQLDatabase::ourInstance->QueryProfileList(count, profildIds, profileList);

			DebugLog(L_INFO, "MESSAGING_RESPOND_PROFILENAME: sending %d profile name/s", count);

			// write profiles to stream
			for (ushort i = 0; i < count; i++)
			{
				// determine profiles' online status
				MMG_AccountProxy::ourInstance->CheckProfileOnlineStatus(&profileList[i]);

				responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_RESPOND_PROFILENAME);
				profileList[i].ToStream(&responseMessage);
			}

			delete [] profileList;
			profileList = NULL;

			free(profildIds);
			profildIds = NULL;

			// send packet
			if (!aClient->SendData(&responseMessage))
				return false;
#endif
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_GET_FRIENDS_AND_ACQUAINTANCES_REQUEST:
		{
			DebugLog(L_INFO, "MESSAGING_GET_FRIENDS_AND_ACQUAINTANCES_REQUEST:");

			/*MMG_Profile *myProfile = aClient->GetProfile();

			this->SendProfileName(aClient, &responseMessage, myProfile);
			this->SendPingsPerSecond(aClient, &responseMessage);

			if (myProfile->m_ClanId)
			{
				MMG_ProtocolDelimiters::Delimiter delim;
				if (!aMessage->ReadDelimiter((ushort &)delim))
					return false;

				uint clanId;
				uint unkZero;
				if (!aMessage->ReadUInt(clanId) || !aMessage->ReadUInt(unkZero))
					return false;

				if (myProfile->m_ClanId != clanId || unkZero != 0)
					return false;

				// TODO: Write clan information
			}

			this->SendCommOptions(aClient, &responseMessage);
			this->SendStartupSequenceComplete(aClient, &responseMessage);
			*/

			//send acquaintances first, otherwise it screws up the contacts list
			if (!this->SendAcquaintance(aClient, &responseMessage))
				return false;

			if (!this->SendFriend(aClient, &responseMessage))
				return false;
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_ADD_FRIEND_REQUEST:
		{
			DebugLog(L_INFO, "MESSAGING_ADD_FRIEND_REQUEST:");

			uint profileId;
			if (!aMessage->ReadUInt(profileId))
				return false;

#ifdef USING_MYSQL_DATABASE
			MySQLDatabase::ourInstance->AddFriend(aClient->GetProfile()->m_ProfileId, profileId);
#endif
			// NOTE:
			// there doesnt seem to be a response, there is a lot of client side validation, making it unnecessary
			// if the client behaves weird, use MESSAGING_GET_FRIENDS_RESPONSE
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_REMOVE_FRIEND_REQUEST:
		{
			DebugLog(L_INFO, "MESSAGING_REMOVE_FRIEND_REQUEST:");

			uint profileId;
			if (!aMessage->ReadUInt(profileId))
				return false;

#ifdef USING_MYSQL_DATABASE
			MySQLDatabase::ourInstance->RemoveFriend(aClient->GetProfile()->m_ProfileId, profileId);
#endif

			DebugLog(L_INFO, "MESSAGING_REMOVE_FRIEND_RESPONSE:");
			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_REMOVE_FRIEND_RESPONSE);
			responseMessage.WriteUInt(profileId);

			if (!aClient->SendData(&responseMessage))
				return false;
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_IM_CHECK_PENDING_MESSAGES:
		{
			DebugLog(L_INFO, "MESSAGING_IM_CHECK_PENDING_MESSAGES:");

			uint msgCount = 0;
			MMG_InstantMessageListener::InstantMessage *myMsgs = NULL;

#ifdef USING_MYSQL_DATABASE
			MySQLDatabase::ourInstance->QueryPendingMessages(aClient->GetProfile()->m_ProfileId, &msgCount, &myMsgs);
#endif

			// if there are messages, send them
			// pending messages will be removed once they have been acknowledged.
			if (myMsgs)
			{
				for (uint i = 0; i < msgCount; i++)
				{
					// check the online status of the sender
					MMG_AccountProxy::ourInstance->CheckProfileOnlineStatus(&myMsgs->m_SenderProfile);

					//DebugLog(L_INFO, "MESSAGING_IM_RECEIVE");
					responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_IM_RECEIVE);
					myMsgs[i].ToStream(&responseMessage);
				}
			
				delete [] myMsgs;
				myMsgs = NULL;

				if (!aClient->SendData(&responseMessage))
					return false;
			}
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_IM_SEND:
		{
			DebugLog(L_INFO, "MESSAGING_IM_SEND");
			
			MMG_InstantMessageListener::InstantMessage myInstantMessage;
			if(!myInstantMessage.FromStream(aMessage))
				return false;
			
			//handle padding
			uint padZero;
			if (!aMessage->ReadUInt(padZero))
				return false;

			//check to see if recipient is online
			SvClient *recipient = MMG_AccountProxy::ourInstance->GetClientByProfileId(myInstantMessage.m_RecipientProfile);

			if (!recipient)
			{
				// intended recipient is offline
#ifdef USING_MYSQL_DATABASE
				MySQLDatabase::ourInstance->AddInstantMessage(aClient->GetProfile()->m_ProfileId, &myInstantMessage);
#endif
			}
			else
			{
				MN_WriteMessage	recipientMessage(2048);

				// TODO
				// this should probably be done in memory
				// if recipient does not ack, the message is lost, un-read messages are NOT saved client side
#ifdef USING_MYSQL_DATABASE
				MySQLDatabase::ourInstance->AddInstantMessage(aClient->GetProfile()->m_ProfileId, &myInstantMessage);
#endif
				//DebugLog(L_INFO, "MESSAGING_IM_RECEIVE");
				recipientMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_IM_RECEIVE);
				myInstantMessage.ToStream(&recipientMessage);

				if (!recipient->SendData(&recipientMessage))
					return false;
			}
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_IM_ACK:
		{
			DebugLog(L_INFO, "MESSAGING_IM_ACK:");

			uint messageId;
			if (!aMessage->ReadUInt(messageId))
				return false;

			// message has been acked, remove it from queue

#ifdef USING_MYSQL_DATABASE
			MySQLDatabase::ourInstance->RemoveInstantMessage(aClient->GetProfile()->m_ProfileId, messageId);
#endif
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_SET_COMMUNICATION_OPTIONS_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_SET_COMMUNICATION_OPTIONS_REQ:");

			uint commOptions;
			if (!aMessage->ReadUInt(commOptions))
				return false;

			// TODO
			//MMG_Options myOptions;
			//myOptions.FromUInt(commOptions);

			aClient->GetOptions()->FromUInt(commOptions);

#ifdef USING_MYSQL_DATABASE
			MySQLDatabase::ourInstance->SaveUserOptions(aClient->GetProfile()->m_ProfileId, commOptions);
#endif
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_GET_COMMUNICATION_OPTIONS_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_GET_COMMUNICATION_OPTIONS_REQ:");

#ifndef USING_MYSQL_DATABASE
			aClient->GetOptions()->FromUInt(992); // i dont remember the default values
#else
			// no need for the query, MESSAGING_GET_IM_SETTINGS is sent first on startup

			//uint commOptions;
			//MySQLDatabase::ourInstance->QueryUserOptions(aClient->GetProfile()->m_ProfileId, &commOptions);
#endif
			if (!this->SendCommOptions(aClient, &responseMessage))
				return false;
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_GET_IM_SETTINGS:
		{
			DebugLog(L_INFO, "MESSAGING_GET_IM_SETTINGS:");

#ifndef USING_MYSQL_DATABASE
			aClient->GetOptions()->FromUInt(992); // i dont remember the default values
#else
			uint commOptions;
			MySQLDatabase::ourInstance->QueryUserOptions(aClient->GetProfile()->m_ProfileId, &commOptions);

			aClient->GetOptions()->FromUInt(commOptions);
#endif
			if (!this->SendIMSettings(aClient, &responseMessage))
				return false;
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_GET_PPS_SETTINGS_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_GET_PPS_SETTINGS_REQ:");

			this->SendPingsPerSecond(aClient, &responseMessage);
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_CLAN_CREATE_REQUEST:
		{
			DebugLog(L_INFO, "MESSAGING_CLAN_CREATE_REQUEST:");

			// TODO - sub_7A1020
			wchar_t clanName[32];
			wchar_t clanTag[11];
			char displayTag[8]; // todo: verify length
			uint padZero;
			memset(clanName, 0, sizeof(clanName));
			memset(clanTag, 0, sizeof(clanTag));
			memset(displayTag, 0, sizeof(displayTag));
			
			if (!aMessage->ReadString(clanName, ARRAYSIZE(clanName)))
				return false;

			if (!aMessage->ReadString(clanTag, ARRAYSIZE(clanTag)))
				return false;

			if (!aMessage->ReadString(displayTag, ARRAYSIZE(displayTag)))
				return false;

			if (!aMessage->ReadUInt(padZero))
				return false;

#ifndef USING_MYSQL_DATABASE

			aClient->GetProfile()->m_ClanId = 4321;
			aClient->GetProfile()->m_RankInClan = 1;

			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_CLAN_CREATE_RESPONSE);
			responseMessage.WriteUChar(1);
			responseMessage.WriteUInt(4321);
			aClient->SendData(&responseMessage);

			MN_WriteMessage responseMessage2(2048);

			SendProfileName(aClient, &responseMessage2, aClient->GetProfile());

#else
			uchar myStatusCode;
			uint myClanId;

			uint nameId = 0, tagId = 0;

			bool ClanNameQueryOK = MySQLDatabase::ourInstance->CheckIfClanNameExists(clanName, &nameId);
			bool ClanTagQueryOK = MySQLDatabase::ourInstance->CheckIfClanTagExists(clanTag, &tagId);
			
			if (nameId > 0 && ClanNameQueryOK)
			{
				myStatusCode = myClanStrings::FAIL_INVALID_NAME;
				myClanId = 0;
			}
			else if (tagId > 0 && ClanTagQueryOK)
			{
				myStatusCode = myClanStrings::FAIL_TAG_TAKEN;
				myClanId = 0;
			}
			else if (!ClanNameQueryOK || !ClanTagQueryOK)
			{
				myStatusCode = myClanStrings::FAIL_MASSGATE;	// myClanStrings::InternalMassgateError
				myClanId = 0;
			}
			else
			{
				uint newClanId=0;

				// creates clan, updates profile info to "in a clan"
				bool CreateClanQueryOk = MySQLDatabase::ourInstance->CreateClan(aClient->GetProfile()->m_ProfileId, clanName, clanTag, displayTag, &newClanId);

				if (CreateClanQueryOk)
				{
					myStatusCode = 1;
					myClanId = newClanId;

					aClient->GetProfile()->m_ClanId = newClanId;
					aClient->GetProfile()->m_RankInClan = 1;

					MySQLDatabase::ourInstance->UpdatePlayerClanInfo(aClient->GetProfile()->m_ProfileId, newClanId, 1);
				}
				else
				{
					myStatusCode = myClanStrings::InternalMassgateError;
					myClanId = 0;
				}
			}

			DebugLog(L_INFO, "MESSAGING_CLAN_CREATE_RESPONSE:");
			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_CLAN_CREATE_RESPONSE);
			responseMessage.WriteUChar(myStatusCode);
			responseMessage.WriteUInt(myClanId);

			if (!aClient->SendData(&responseMessage))
				return false;

			// TODO update friends and acquaintances
			MMG_AccountProxy::ourInstance->UpdateClients(aClient->GetProfile());
#endif
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_CLAN_MODIFY_REQUEST:
		{
			DebugLog(L_INFO, "MESSAGING_CLAN_MODIFY_REQUEST:");

			wchar_t ClanMotto[WIC_MOTTO_MAX_LENGTH];
			wchar_t ClanMessageoftheday[WIC_MOTD_MAX_LENGTH];
			wchar_t ClanHomepage[WIC_HOMEPAGE_MAX_LENGTH];
			memset(ClanMotto, 0, sizeof(ClanMotto));
			memset(ClanMessageoftheday, 0, sizeof(ClanMessageoftheday));
			memset(ClanHomepage, 0, sizeof(ClanHomepage));
			
			if (!aMessage->ReadString(ClanMotto, ARRAYSIZE(ClanMotto)))
				return false;

			if (!aMessage->ReadString(ClanMessageoftheday, ARRAYSIZE(ClanMessageoftheday)))
				return false;

			if (!aMessage->ReadString(ClanHomepage, ARRAYSIZE(ClanHomepage)))
				return false;

			uint profileId;
			if (!aMessage->ReadUInt(profileId))
				return false;

			// TODO get clan id of sent profileId
			uint clanId = aClient->GetProfile()->m_ClanId;

#ifdef USING_MYSQL_DATABASE
			MySQLDatabase::ourInstance->SaveClanEditableVariables(profileId, clanId, ClanMotto, ClanMessageoftheday, ClanHomepage);
#endif
			// no response, rest of packet at this point is another delimiter (sub_7A0FE0, MESSAGING_CLAN_FULL_INFO_REQUEST)
		}
		break;
		
		/*case MMG_ProtocolDelimiters::MESSAGING_CLAN_MODIFY_RANKS_REQUEST:
		{
			DebugLog(L_INFO, "MESSAGING_CLAN_MODIFY_RANKS_REQUEST:"); //sub_7A0F60

			uint playerid;
			uint aZero;
			if (!aMessage->ReadUInt(playerid) || !aMessage->ReadUInt(aZero))
				return false;
			
#ifdef USING_MYSQL_DATABASE
			MMG_Clan myClan;
			if (!MySQLDatabase::ourInstance->QueryClan(aClient->GetProfile()->m_ClanId, &myClan))
				return false;
			
			//I expect the aZero to be something else when you actually do something different than leaving the clan (e.g. promoting / demoting / making someone clan leader)
			//requires the main clan page to work tho
			if (aZero == 0)
			{
				if (myClan.m_MemberCount == 1)
				{
					if (!MySQLDatabase::ourInstance->DeleteClan(myClan.m_ClanId))
						return false;
				}
				
				if (!MySQLDatabase::ourInstance->UpdatePlayerClanInfo(aClient->GetProfile()->m_ProfileId, 0, 0))
					return false;
			}
#endif
			//no return message yet, maybe a full info request if the clan is not deleted
			//if (!aClient->SendData(&responseMessage))
			//	return false;
		}
		break;
		*/

		/*
		clan invites: MESSAGING_CLAN_INVITE_PLAYER_REQUEST, MESSAGING_CLAN_INVITE_PLAYER_RESPONSE
		use MySQLDatabase::UpdatePlayerClanInfo
		*/

		case MMG_ProtocolDelimiters::MESSAGING_CLAN_FULL_INFO_REQUEST:
		{
			DebugLog(L_INFO, "MESSAGING_CLAN_FULL_INFO_REQUEST:");
			
			uint clanId, padZero;
			if (!aMessage->ReadUInt(clanId) || !aMessage->ReadUInt(padZero))
				return false;
			
#ifndef USING_MYSQL_DATABASE

			// TODO
			wchar_t clanName[32];
			wchar_t clanTag[11];
			wchar_t clanMotto[256];
			wchar_t clanMessageOfTheDay[256];
			wchar_t clanHomepage[256];
			uint clanNumberOfPlayers = 0;
			uint clanPlayerId[512];
			uint clanLeaderId = 0, clanPlayerOfTheWeekId = 0;
			memset(clanName, 0, sizeof(clanName));
			memset(clanTag, 0, sizeof(clanTag));
			memset(clanMotto, 0, sizeof(clanMotto));
			memset(clanMessageOfTheDay, 0, sizeof(clanMessageOfTheDay));
			memset(clanHomepage, 0, sizeof(clanHomepage));

			wcscpy(clanName, L"Developers & Moderators");
			wcscpy(clanTag, L"devs");
			wcscpy(clanMotto, L"Today is a good day for clan wars!");
			wcscpy(clanMessageOfTheDay, L"Welcome clanmembers!");
			wcscpy(clanHomepage, L"massgate.org");
			
			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_CLAN_FULL_INFO_RESPONSE);
			responseMessage.WriteString(clanName);
			responseMessage.WriteString(clanTag);
			responseMessage.WriteString(clanMotto);
			responseMessage.WriteString(clanMessageOfTheDay);
			responseMessage.WriteString(clanHomepage);
			
			responseMessage.WriteUInt(1);	// number of players
			responseMessage.WriteUInt(1234);	// player list

			responseMessage.WriteUInt(4321);	// clan id
			responseMessage.WriteUInt(1234);	// player of the week
			
			aClient->SendData(&responseMessage);
#else
			uint memberCount;
			MMG_Clan::FullInfo clanFullInfo;

			// request clan info from database
			MySQLDatabase::ourInstance->QueryClan(clanId, &memberCount, &clanFullInfo);

			DebugLog(L_INFO, "MESSAGING_CLAN_FULL_INFO_RESPONSE:");
			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_CLAN_FULL_INFO_RESPONSE);
			
			responseMessage.WriteString(clanFullInfo.m_FullClanName);
			responseMessage.WriteString(clanFullInfo.m_ShortClanName);
			responseMessage.WriteString(clanFullInfo.m_Motto);
			responseMessage.WriteString(clanFullInfo.m_LeaderSays);
			responseMessage.WriteString(clanFullInfo.m_Homepage);

			responseMessage.WriteUInt(memberCount);

			for (int i = 0; i < memberCount; i++)
				responseMessage.WriteUInt(clanFullInfo.m_ClanMembers[i]);

			responseMessage.WriteUInt(clanFullInfo.m_ClanId);
			responseMessage.WriteUInt(clanFullInfo.m_PlayerOfWeek);

			if (!aClient->SendData(&responseMessage))
				return false;
#endif
		}
		break;
		
		case MMG_ProtocolDelimiters::MESSAGING_SET_STATUS_ONLINE:
		{
			DebugLog(L_INFO, "MESSAGING_SET_STATUS_ONLINE:");

			/*
			IDA wic.exe sub_7A1360 MMG_Messaging::SetStatusOnline
			after WriteDelimiter, the client appends a Uint ( 0, some sort of padding? ) to the packet
			handling this random 0 is necessary otherwise "message from client delimiter 0 type 0"
			will show up in the debug log.
			
			Massgate will crash if these 0's arent handled since the message reader will 
			treat it as the next delimiter.

			the same thing happens for cases:
				- MESSAGING_GET_CLIENT_METRICS -UChar, (IDA sub_7A1200 MMG_Messaging::GetClientMetrics)
				- MESSAGING_STARTUP_SEQUENCE_COMPLETE -2 x UInt, (IDA sub_7A1B00, see EXMASS_Client::ReceiveNotification lines 41 and 42)

			remove the handle padding code in the 3 cases to reproduce the crash
			*/

			//handle padding
			uint padZero;
			if (!aMessage->ReadUInt(padZero))
				return false;

			// not sure if this is right
			aClient->GetProfile()->m_OnlineStatus = 1;
			MMG_AccountProxy::ourInstance->SetClientOnline(aClient);
			MMG_AccountProxy::ourInstance->UpdateClients(aClient->GetProfile());

			// response maybe MESSAGING_MASSGATE_GENERIC_STATUS_RESPONSE
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_SET_STATUS_PLAYING:
		{
			DebugLog(L_INFO, "MESSAGING_SET_STATUS_PLAYING:");

			//IDA wic.exe sub_7A1290 MMG_Messaging::SetStatusPlaying

			// either MMG_AccountProxy::State_t or MMG_MasterConnection::State
			uint serverId;
			if (!aMessage->ReadUInt(serverId))
				return false;

			//handle padding
			uint padZero;
			if (!aMessage->ReadUInt(padZero))
				return false;

			aClient->GetProfile()->m_OnlineStatus = serverId;
			MMG_AccountProxy::ourInstance->UpdateClients(aClient->GetProfile());

			// response maybe MESSAGING_MASSGATE_GENERIC_STATUS_RESPONSE
		}
		break;

		/*case MMG_ProtocolDelimiters::MESSAGING_GET_CLIENT_METRICS:
		{
			DebugLog(L_INFO, "MESSAGING_GET_CLIENT_METRICS:");

			//handle padding
			uchar padZero;
			if (!aMessage->ReadUChar(padZero))
				return false;

			//DebugLog(L_INFO, "MESSAGING_GET_CLIENT_METRICS:");
			//responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_GET_CLIENT_METRICS);

			//char key[16] = "";
			//char value[96] = "";
			
			// TODO
			// this has something to do with EXMASS_Client::ReceiveNotification case 27) 
			//
			// enum MMG_ClientMetric::Context
			// number of options
			// key
			// value

			//responseMessage.WriteUChar(1);
			//responseMessage.WriteUInt(0);
			//responseMessage.WriteString(key, ARRAYSIZE(key));
			//responseMessage.WriteString(value, ARRAYSIZE(value));

			//if (!aClient->SendData(&responseMessage))
			//	return false;
		}
		break;
		*/

		/*case MMG_ProtocolDelimiters::MESSAGING_SET_CLIENT_SETTINGS:
		{
			DebugLog(L_INFO, "MESSAGING_SET_CLIENT_SETTINGS:");

			//TODO
			//DebugBreak();
		}
		break;
		*/

		case MMG_ProtocolDelimiters::MESSAGING_STARTUP_SEQUENCE_COMPLETE:
		{
			DebugLog(L_INFO, "MESSAGING_STARTUP_SEQUENCE_COMPLETE:");

			//handle padding
			uint padZero;
			if (!aMessage->ReadUInt(padZero) || !aMessage->ReadUInt(padZero))
				return false;

			this->SendStartupSequenceComplete(aClient, &responseMessage);
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_CLIENT_REQ_GET_PCC:
		{
			DebugLog(L_INFO, "MESSAGING_CLIENT_REQ_GET_PCC:");

			// This is called when the clan profile page is opened, player created content

			uint requestCount;
			if (!aMessage->ReadUInt(requestCount))
				return false;

			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_CLIENT_RSP_GET_PCC);
			responseMessage.WriteUInt(requestCount);// Number of responses

			for (int i = 0; i < requestCount; i++)
			{
				uint requestId;
				uchar requestType;

				aMessage->ReadUInt(requestId);
				aMessage->ReadUChar(requestType);

				// Write these back to the outgoing messages
				responseMessage.WriteUInt(requestId);		// Content id
				responseMessage.WriteUInt(1);				// Content sequence number
				responseMessage.WriteUChar(requestType);	// Content type (0 is profile image, 1 is clan image)
				responseMessage.WriteString("https://my_domain_here.abc/my_image.dds"); // Image url
			}

			if (!aClient->SendData(&responseMessage))
				return false;
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_ABUSE_REPORT:
		{
			DebugLog(L_INFO, "MESSAGING_ABUSE_REPORT:");
			
			uint profileIdReported;
			if (!aMessage->ReadUInt(profileIdReported))
				return false;
			
			wchar_t anAbuseReport[256];
			memset(anAbuseReport, 0, sizeof(anAbuseReport));
			if (!aMessage->ReadString(anAbuseReport, ARRAYSIZE(anAbuseReport)))
				return false;
			
			//handle padding
			uchar aZero;
			if (!aMessage->ReadUChar(aZero))
				return false;

#ifdef USING_MYSQL_DATABASE
			MySQLDatabase::ourInstance->AddAbuseReport(aClient->GetProfile()->m_ProfileId, profileIdReported, anAbuseReport);
#endif
		}
		break;
		
		case MMG_ProtocolDelimiters::MESSAGING_OPTIONAL_CONTENT_GET_REQ:
		{
			char langCode[3]; // "EN"
			aMessage->ReadString(langCode, ARRAYSIZE(langCode));

			DebugLog(L_INFO, "MESSAGING_OPTIONAL_CONTENT_GET_REQ: %s", langCode);

			// Optional content (I.E maps)
			this->SendOptionalContent(aClient, &responseMessage);
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_OPTIONAL_CONTENT_RETRY_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_OPTIONAL_CONTENT_RETRY_REQ:");
			// TODO
			// Is this used?
			// Retry
			// this->SendOptionalContent(aClient, &responseMessage);
		}
		break;

		/*case MMG_ProtocolDelimiters::MESSAGING_PROFILE_GUESTBOOK_POST_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_PROFILE_GUESTBOOK_POST_REQ: Sending information");

			//18:00:76:00:4a:00:00:00:01:00:00:00:02:00:00:00:04:00:68:00:69:00:21:00:00:00 for text "hi!" 180076004a000000010000000200000004006800690021000000
			
			uint RequestId = 0, ProfileId = 0, MessageId = 0;
			wchar_t GuestBookMessage[128];

			aMessage->ReadUInt(ProfileId);
			aMessage->ReadUInt(RequestId);
			aMessage->ReadUInt(MessageId);
			aMessage->ReadString(GuestBookMessage, ARRAYSIZE(GuestBookMessage));
			
			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_PROFILE_GUESTBOOK_GET_RSP);
			
			//currentEditables->ToStream(&responseMessage);

			//if (!aClient->SendData(&responseMessage))
			//	return false;
		}
		break;
		*/
		
		/*case MMG_ProtocolDelimiters::MESSAGING_PROFILE_GUESTBOOK_GET_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_PROFILE_GUESTBOOK_GET_REQ: Sending information");

			//0a:00:77:00:01:00:00:00:4a:00:00:00

			MN_WriteMessage	responseMessage1(8192);

			uint RequestId = 0, ProfileId = 0;
			aMessage->ReadUInt(RequestId);
			aMessage->ReadUInt(ProfileId);
			
			MMG_ProfileGuestBookProtocol myProfileGuestBook;

			myProfileGuestBook.m_RequestId = RequestId;
			myProfileGuestBook.m_IgnoresGettingProfile = 0;
			myProfileGuestBook.m_Count = 1;
			myProfileGuestBook.m_MaxSize = 8192;
			
			const int guestbooklength = 1;
			MMG_ProfileGuestBookEntry myGuestBookEntry[guestbooklength];
			for (int i = 0; i < guestbooklength; i++)
			{
				wcscpy(myGuestBookEntry[i].m_GuestBookMessage, L"Sup b1tches");
				myGuestBookEntry[i].m_TimeStamp = 6060 * i;
				myGuestBookEntry[i].m_ProfileId = ProfileId;
				myGuestBookEntry[i].m_MessageId = 1;
				
				myProfileGuestBook.m_GuestBookEntry[i] = myGuestBookEntry[i];
			}
			
			myProfileGuestBook.m_Data = sizeof(myProfileGuestBook.m_GuestBookEntry);
			
			responseMessage1.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_PROFILE_GUESTBOOK_GET_RSP);
			
			myProfileGuestBook.ToStream(&responseMessage1);

			//if (!aClient->SendData(&responseMessage))
			//	return false;
		}
		break;
		*/
		
		/*case MMG_ProtocolDelimiters::MESSAGING_PROFILE_GUESTBOOK_DELETE_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_PROFILE_GUESTBOOK_DELETE_REQ: Sending information");

			//responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_PROFILE_GUESTBOOK_GET_RSP);
			
			//currentEditables->ToStream(&responseMessage);

			//if (!aClient->SendData(&responseMessage))
			//	return false;
		}
		break;
		*/

		case MMG_ProtocolDelimiters::MESSAGING_PROFILE_SET_EDITABLES_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_PROFILE_SET_EDITABLES_REQ:");

			MMG_ProfileEditableVariablesProtocol::GetRsp myResponse;

			if (!myResponse.FromStream(aMessage))
				return false;

#ifdef USING_MYSQL_DATABASE
			MySQLDatabase::ourInstance->SaveEditableVariables(aClient->GetProfile()->m_ProfileId, myResponse.motto, myResponse.homepage);
#endif

			// no response required
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_PROFILE_GET_EDITABLES_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_PROFILE_GET_EDITABLES_REQ:");

			uint profileId;
			if (!aMessage->ReadUInt(profileId))
				return false;

			MMG_ProfileEditableVariablesProtocol::GetRsp myResponse;

#ifndef USING_MYSQL_DATABASE
			wcscpy(myResponse.motto, L"");
			wcscpy(myResponse.homepage, L"");
#else
			MySQLDatabase::ourInstance->QueryEditableVariables(profileId, myResponse.motto, myResponse.homepage);
#endif
			DebugLog(L_INFO, "MESSAGING_PROFILE_GET_EDITABLES_RSP:");
			responseMessage.WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_PROFILE_GET_EDITABLES_RSP);
			myResponse.ToStream(&responseMessage);

			if (!aClient->SendData(&responseMessage))
				return false;
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_IGNORELIST_ADD_REMOVE_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_IGNORELIST_ADD_REMOVE_REQ:");

			uint profileId;
			if (!aMessage->ReadUInt(profileId))
				return false;

			uchar operation;
			if (!aMessage->ReadUChar(operation))
				return false;
#ifndef USING_MYSQL_DATABASE
			if (operation == 1)
				printf("add\n");
			else
				printf("remove\n");
#else
			if (operation == 1)
				MySQLDatabase::ourInstance->AddIgnoredProfile(aClient->GetProfile()->m_ProfileId, profileId);
			else
				MySQLDatabase::ourInstance->RemoveIgnoredProfile(aClient->GetProfile()->m_ProfileId, profileId);
#endif
			// wic.exe IDA sub_BD7C90 (called from WICMASS_ContactsScreenHandler)
			// NOTE:
			// there doesnt seem to be a response, there is a lot of client side validation, making it unnecessary
			// if the client behaves weird, use MESSAGING_IGNORELIST_GET_RSP
		}
		break;
		
		case MMG_ProtocolDelimiters::MESSAGING_IGNORELIST_GET_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_IGNORELIST_GET_REQ:");

			this->SendProfileIgnoreList(aClient, &responseMessage);
		}
		break;

		case MMG_ProtocolDelimiters::MESSAGING_CLAN_COLOSSEUM_GET_FILTER_WEIGHTS_REQ:
		{
			DebugLog(L_INFO, "MESSAGING_CLAN_COLOSSEUM_GET_FILTER_WEIGHTS_REQ:");

			this->SendClanWarsFilterWeights(aClient, &responseMessage);
		}
		break;

		default:
			DebugLog(L_WARN, "Unknown delimiter %i", aDelimiter);
		return false;
	}

	return true;
}

bool MMG_Messaging::SendProfileName(SvClient *aClient, MN_WriteMessage	*aMessage, MMG_Profile *aProfile)
{
	DebugLog(L_INFO, "MESSAGING_RESPOND_PROFILENAME: %ws", aProfile->m_Name);
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_RESPOND_PROFILENAME);
	aProfile->ToStream(aMessage);

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendFriend(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_GET_FRIENDS_RESPONSE:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_GET_FRIENDS_RESPONSE);

#ifndef USING_MYSQL_DATABASE
	//write uint (num friends)
	aMessage->WriteUInt(2);
	aMessage->WriteUInt(1235);
	aMessage->WriteUInt(1236);

	//for each friend
		//write uint (profile id)
#else
	uint friendCount = 0;
	uint *myFriends = NULL;

	bool QueryOK = MySQLDatabase::ourInstance->QueryFriends(aClient->GetProfile()->m_ProfileId, &friendCount, &myFriends);

	if (QueryOK)
	{
		aMessage->WriteUInt(friendCount);

		for (uint i=0; i < friendCount; i++)
		{
			aMessage->WriteUInt(myFriends[i]);
		}
	}
	else
	{
		aMessage->WriteUInt(0);
	}

	delete [] myFriends;
	myFriends = NULL;
#endif

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendAcquaintance(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_GET_ACQUAINTANCES_RESPONSE:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_GET_ACQUAINTANCES_RESPONSE);

	//write uint (num acquaintances)
	aMessage->WriteUInt(0);

	//for each acquaintance
		//write uint (profile id)
		//write uint number times played

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendCommOptions(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_GET_COMMUNICATION_OPTIONS_RSP:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_GET_COMMUNICATION_OPTIONS_RSP);

	aMessage->WriteUInt(aClient->GetOptions()->ToUInt());

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendIMSettings(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_GET_IM_SETTINGS:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_GET_IM_SETTINGS);

	IM_Settings mySettings;
	mySettings.m_Friends = aClient->GetOptions()->m_ReceiveFromFriends;
	mySettings.m_Clanmembers = aClient->GetOptions()->m_ReceiveFromClanMembers;
	mySettings.m_Acquaintances = aClient->GetOptions()->m_ReceiveFromAcquaintances;
	mySettings.m_Anyone = aClient->GetOptions()->m_ReceiveFromEveryoneElse;
	mySettings.ToStream(aMessage);

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendPingsPerSecond(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_GET_PPS_SETTINGS_RSP:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_GET_PPS_SETTINGS_RSP);
	aMessage->WriteInt(WIC_CLIENT_PPS);

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendStartupSequenceComplete(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_STARTUP_SEQUENCE_COMPLETE:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_STARTUP_SEQUENCE_COMPLETE);

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendOptionalContent(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_OPTIONAL_CONTENT_GET_RSP:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_OPTIONAL_CONTENT_GET_RSP);

	// Write the map info data stream
	MMG_OptionalContentProtocol::ourInstance->ToStream(aMessage);

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendProfileIgnoreList(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_IGNORELIST_GET_RSP:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_IGNORELIST_GET_RSP);

#ifndef USING_MYSQL_DATABASE
	//write uint (num ignored profiles)
	aMessage->WriteUInt(0);

	//for each ignored profile
		//write uint (profile id)
#else
	uint ignoredCount = 0;
	uint *myIgnoreList = NULL;

	bool QueryOK = MySQLDatabase::ourInstance->QueryIgnoredProfiles(aClient->GetProfile()->m_ProfileId, &ignoredCount, &myIgnoreList);

	if (QueryOK)
	{
		aMessage->WriteUInt(ignoredCount);

		for (uint i=0; i < ignoredCount; i++)
		{
			aMessage->WriteUInt(myIgnoreList[i]);
		}
	}
	else
	{
		aMessage->WriteUInt(0);
	}

	delete [] myIgnoreList;
	myIgnoreList = NULL;
#endif

	return aClient->SendData(aMessage);
}

bool MMG_Messaging::SendClanWarsFilterWeights(SvClient *aClient, MN_WriteMessage *aMessage)
{
	DebugLog(L_INFO, "MESSAGING_CLAN_COLOSSEUM_GET_FILTER_WEIGHTS_RSP:");
	aMessage->WriteDelimiter(MMG_ProtocolDelimiters::MESSAGING_CLAN_COLOSSEUM_GET_FILTER_WEIGHTS_RSP);

	//TODO: i have no idea what these are for
	aMessage->WriteFloat(0.0f);	//myClanWarsHaveMapWeight
	aMessage->WriteFloat(0.0f);	//myClanWarsDontHaveMapWeight
	aMessage->WriteFloat(0.0f);	//myClanWarsPlayersWeight
	aMessage->WriteFloat(0.0f);	//myClanWarsWrongOrderWeight
	aMessage->WriteFloat(0.0f);	//myClanWarsRatingDiffWeight
	aMessage->WriteFloat(0.0f);	//myClanWarsMaxRatingDiff

	return aClient->SendData(aMessage);
}