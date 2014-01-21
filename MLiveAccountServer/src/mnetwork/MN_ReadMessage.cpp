#include "../stdafx.h"

bool MN_ReadMessage::BuildMessage(PVOID aData, uint aDataLen)
{
	assert(this->m_PacketData && this->m_DataLen == 0 && aData && aDataLen < this->m_PacketMaxSize);

	ushort packetFlags	= *(ushort *)aData;
	ushort packetLen	= packetFlags & 0x3FFF;
	uint addSize		= sizeof(ushort);

	// Decompress if needed
	if(packetFlags & MESSAGE_FLAG_COMPRESSED)
	{
		uint usedBytes;
		this->m_DataLen = MP_Pack::UnpackZip((voidptr_t)((uintptr_t)aData + addSize), (voidptr_t)this->m_PacketData, packetLen, this->m_PacketMaxSize, &usedBytes);

		if(!this->m_DataLen || usedBytes != packetLen)
			return false;
	}
	else
	{
		// Copy the raw packet data
		memcpy((voidptr_t)this->m_PacketData, (voidptr_t)((uintptr_t)aData + addSize), packetLen);

		// Set the maximum read length
		this->m_DataLen = packetLen;
	}

	return true;
}

bool MN_ReadMessage::TypeCheck(ushort aType)
{
	return (!this->m_TypeChecks || aType == this->Read<ushort>());
}

bool MN_ReadMessage::ReadDelimiter(ushort &aDelimiter)
{
	if(!this->TypeCheck('DL'))
		return false;

	aDelimiter = this->Read<ushort>();

	return true;
}

bool MN_ReadMessage::ReadUChar(uchar &aUChar)
{
	if(!this->TypeCheck('UC'))
		return false;

	aUChar = this->Read<uchar>();

	return true;
}

bool MN_ReadMessage::ReadUShort(ushort &aUShort)
{
	if(!this->TypeCheck('US'))
		return false;

	aUShort = this->Read<ushort>();

	return true;
}

bool MN_ReadMessage::ReadUInt(uint &aUInt)
{
	if(!this->TypeCheck('UI'))
		return false;

	aUInt = this->Read<uint>();

	return true;
}

bool MN_ReadMessage::ReadULong(ulong &aULong)
{
	return this->ReadUInt((uint &)aULong);
}

bool MN_ReadMessage::ReadUInt64(uint64 &aUInt64)
{
	if(!this->TypeCheck('U6'))
		return false;

	aUInt64 = this->Read<uint64>();

	return true;
}

bool MN_ReadMessage::ReadFloat(float &aFloat)
{
	if(!this->TypeCheck('FL'))
		return false;

	aFloat = this->Read<float>();

	return true;
}

bool MN_ReadMessage::ReadRawData(PVOID aBuffer, unsigned int aBufferSize, unsigned int *aTotalSize)
{
	assert((aBuffer && aBufferSize > 0) || aTotalSize);

	if(!this->TypeCheck('RD'))
		return false;

	ushort dataLength = this->Read<ushort>();

	this->CheckReadSize(dataLength);

	if(aBuffer)
	{
		memset(aBuffer, 0, aBufferSize);

		if(dataLength > aBufferSize)
			return false;

		memcpy(aBuffer, (voidptr_t)this->m_ReadPtr, dataLength);
	}

	if(aTotalSize)
		*aTotalSize = dataLength;

	if(aBuffer)
		this->IncReadPos(dataLength);

	return true;
}

bool MN_ReadMessage::ReadString(char *aBuffer, uint aStringSize)
{
	assert(aBuffer && aStringSize > 0);

	ushort bufferLength = aStringSize * sizeof(char);
	ushort dataLength	= this->Read<ushort>() * sizeof(char);

	this->CheckReadSize(dataLength);

	if(aBuffer)
	{
		memset(aBuffer, 0, bufferLength);

		if(dataLength > bufferLength)
			return false;

		memcpy(aBuffer, (voidptr_t)this->m_ReadPtr, dataLength);
	}

	this->IncReadPos(dataLength);

	return true;
}

bool MN_ReadMessage::ReadString(wchar_t *aBuffer, uint aStringSize)
{
	assert(aBuffer && aStringSize > 0);

	ushort bufferLength = aStringSize * sizeof(wchar_t);
	ushort dataLength	= this->Read<ushort>() * sizeof(wchar_t);

	this->CheckReadSize(dataLength);

	if(aBuffer)
	{
		memset(aBuffer, 0, bufferLength);

		if(dataLength > bufferLength)
			return false;

		memcpy(aBuffer, (voidptr_t)this->m_ReadPtr, dataLength);
	}

	this->IncReadPos(dataLength);

	return true;
}

void MN_ReadMessage::CheckReadSize(uint aSize)
{
	assert((this->m_ReadPos + aSize) < this->m_PacketMaxSize && "Packet read exceeded bounds.");
}

void MN_ReadMessage::IncReadPos(uint aSize)
{
	this->m_ReadPtr = this->m_ReadPtr + aSize;
	this->m_ReadPos += aSize;
}