#include "../stdafx.h"

MMG_ClanGuestbookProtocol::GetRsp::GuestbookEntry::GuestbookEntry()
{
	memset(this->m_Message, 0, sizeof(this->m_Message));
	this->m_Timestamp = 0;
	this->m_ProfileId = 0;
	this->m_MessageId = 0;
}

MMG_ClanGuestbookProtocol::GetRsp::GetRsp() : m_Entries()
{
	this->m_ClanId = 0;
	this->m_RequestId = 0;
}

void MMG_ClanGuestbookProtocol::GetRsp::ToStream(MN_WriteMessage *aMessage)
{
}

MMG_ClanGuestbookProtocol::MMG_ClanGuestbookProtocol()
{
}