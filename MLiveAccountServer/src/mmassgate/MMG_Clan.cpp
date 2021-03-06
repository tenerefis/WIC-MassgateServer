#include "../stdafx.h"

MMG_Clan::FullInfo::FullInfo()
{
	m_ClanId		= 0;
	memset(this->m_FullClanName, 0, sizeof(this->m_FullClanName));
	memset(this->m_ShortClanName, 0, sizeof(this->m_ShortClanName));
	memset(this->m_Motto, 0, sizeof(this->m_Motto));
	memset(this->m_LeaderSays, 0, sizeof(this->m_LeaderSays));
	memset(this->m_Homepage, 0, sizeof(this->m_Homepage));
	memset(this->m_ClanMembers, 0, sizeof(this->m_ClanMembers));
	m_PlayerOfWeek	= 0;
}

MMG_Clan::Description::Description()
{
	m_ClanId		= 0;
	memset(this->m_FullName, 0, sizeof(this->m_FullName));
	memset(this->m_ClanTag, 0, sizeof(this->m_ClanTag));
}

void MMG_Clan::Description::ToStream(MN_WriteMessage *aMessage)
{
	aMessage->WriteString(this->m_FullName);
	aMessage->WriteString(this->m_ClanTag);
	aMessage->WriteUInt(this->m_ClanId);
}