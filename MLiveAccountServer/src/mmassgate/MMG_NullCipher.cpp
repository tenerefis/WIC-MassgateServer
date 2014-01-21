#include "../stdafx.h"

MMG_NullCipher::MMG_NullCipher()
{
	this->m_Indentifier = CIPHER_NULLCIPHER;
}

void MMG_NullCipher::Encrypt(uint *aData, uint aDataLength)
{
}

void MMG_NullCipher::Decrypt(uint *aData, uint aDataLength)
{
}