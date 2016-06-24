#include "../stdafx.h"

void MN_WriteMessage::WriteDelimiter(ushort aDelimiter)
{
	if (this->m_TypeChecks)
		this->Write<ushort>('DL');

	this->Write<ushort>(aDelimiter);
}

void MN_WriteMessage::WriteBool(bool aBool)
{
	if (this->m_TypeChecks)
		this->Write<ushort>('BL');

	this->Write<bool>(aBool);
}

void MN_WriteMessage::WriteUChar(uchar aUChar)
{
	if (this->m_TypeChecks)
		this->Write<ushort>('UC');

	this->Write<uchar>(aUChar);
}

void MN_WriteMessage::WriteUShort(ushort aUShort)
{
	if (this->m_TypeChecks)
		this->Write<ushort>('US');

	this->Write<ushort>(aUShort);
}

void MN_WriteMessage::WriteInt(int aInt)
{
	if (this->m_TypeChecks)
		this->Write<ushort>('IN');

	this->Write<int>(aInt);
}

void MN_WriteMessage::WriteUInt(uint aUInt)
{
	if (this->m_TypeChecks)
		this->Write<ushort>('UI');

	this->Write<uint>(aUInt);
}

void MN_WriteMessage::WriteULong(ulong aULong)
{
	this->WriteUInt(aULong);
}

void MN_WriteMessage::WriteUInt64(uint64 aUInt64)
{
	if (this->m_TypeChecks)
		this->Write<ushort>('U6');

	this->Write<uint64>(aUInt64);
}

void MN_WriteMessage::WriteFloat(float aFloat)
{
	if (this->m_TypeChecks)
		this->Write<ushort>('FL');

	this->Write<float>(aFloat);
}

void MN_WriteMessage::WriteRawData(voidptr_t aBuffer, sizeptr_t aBufferSize)
{
	assert(aBuffer && aBufferSize > 0);
	assert(aBufferSize < MESSAGE_MAX_LENGTH);

	if (this->m_TypeChecks)
		this->Write<ushort>('RD');

	// Packet will exceed bounds before it exceeds ushort
	this->Write<ushort>(aBufferSize);	// No typecheck
	this->CheckWriteSize(aBufferSize);

	memcpy((voidptr_t)this->m_WritePtr, aBuffer, aBufferSize);

	this->IncWritePos(aBufferSize);
}

void MN_WriteMessage::WriteString(char *aBuffer)
{
	this->WriteString(aBuffer, strlen(aBuffer) + 1);
}

void MN_WriteMessage::WriteString(char *aBuffer, sizeptr_t aStringSize)
{
	assert(aBuffer && aStringSize > 0);
	assert((aStringSize * sizeof(char)) < MESSAGE_MAX_LENGTH);

	// Calculate actual size from number of characters * sizeof(single char)
	ushort stringSize = (ushort)aStringSize;
	ushort bufferSize = (ushort)(stringSize * sizeof(char));

	this->Write<ushort>(stringSize);	// No typecheck
	this->CheckWriteSize(bufferSize);

	memcpy((voidptr_t)this->m_WritePtr, aBuffer, bufferSize);

	this->IncWritePos(bufferSize);
}

void MN_WriteMessage::WriteString(wchar_t *aBuffer)
{
	this->WriteString(aBuffer, wcslen(aBuffer) + 1);
}

void MN_WriteMessage::WriteString(wchar_t *aBuffer, sizeptr_t aStringSize)
{
	assert(aBuffer && aStringSize > 0);
	assert((aStringSize * sizeof(wchar_t)) < MESSAGE_MAX_LENGTH);

	// Calculate actual size from number of characters * sizeof(single wchar)
	ushort stringSize = (ushort)aStringSize;
	ushort bufferSize = (ushort)(stringSize * sizeof(wchar_t));

	this->Write<ushort>(stringSize);	// No typecheck
	this->CheckWriteSize(bufferSize);

	memcpy((voidptr_t)this->m_WritePtr, aBuffer, bufferSize);

	this->IncWritePos(bufferSize);
}

bool MN_WriteMessage::SendMe(SOCKET sock)
{
	return this->SendMe(sock, true);
}

bool MN_WriteMessage::SendMe(SOCKET sock, bool aClearData)
{
	// Windows sockets only supports int as the data size
	if (this->m_DataLen >= INT_MAX)
		return false;

	bool retVal = send(sock, (const char *)this->m_PacketData, (int)this->m_DataLen, 0) != SOCKET_ERROR;

	if (aClearData)
	{
		this->m_WritePtr	= this->m_PacketData + sizeof(ushort);
		this->m_WritePos	= 0;
		this->m_DataLen		= 2;
	}

	return retVal;
}

voidptr_t MN_WriteMessage::GetDataStream()
{
	return (voidptr_t)this->m_PacketData;
}

sizeptr_t MN_WriteMessage::GetDataLength()
{
	return this->m_DataLen;
}

void MN_WriteMessage::CheckWriteSize(sizeptr_t aSize)
{
	assert((this->m_DataLen + aSize) < this->m_PacketMaxSize && "Packet write exceeded bounds.");
}

void MN_WriteMessage::IncWritePos(sizeptr_t aSize)
{
	this->m_WritePtr	= this->m_WritePtr + aSize;
	this->m_WritePos	+= aSize;
	this->m_DataLen		+= aSize;

	// Write the message-specific information that is stored in the first 2 bytes
	ushort packetInfo = 0;

	// Packet length
	packetInfo = (ushort)(this->m_DataLen - sizeof(ushort));
	packetInfo &= MESSAGE_MAX_LENGTH;

	// Packet flags
	if (this->m_TypeChecks)
		packetInfo |= MESSAGE_FLAG_TYPECHECKS;

	*(ushort *)this->m_PacketData = packetInfo;
}