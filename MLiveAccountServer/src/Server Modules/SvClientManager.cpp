#include "../stdafx.h"

//
// Begin SvClient
//
SvClient::SvClient()
{
	this->m_ProfileStateObserver = nullptr;
	this->m_Profile		= nullptr;
	this->m_AuthToken	= nullptr;
	this->m_Options		= nullptr;
	this->m_Socket		= INVALID_SOCKET;

	this->Reset();
}

SvClient::~SvClient()
{
	this->Reset();
}

void SvClient::UpdateActivity()
{
	InterlockedExchange((volatile LONG *)&this->m_LastActiveTime, MI_Time::GetSystemTime());
}

bool SvClient::IsActive()
{
	return (MI_Time::GetSystemTime() - this->m_LastActiveTime) < this->m_TimeoutTime;
}

bool SvClient::IsLoggedIn()
{
	return this->m_LoggedIn;
}

bool SvClient::IsPlayer()
{
	return this->m_IsPlayer;
}

bool SvClient::IsServer()
{
	return this->m_IsServer;
}

void SvClient::SetTimeout(uint aTimeout)
{
	InterlockedExchange((volatile LONG *)&this->m_TimeoutTime, aTimeout);
}

void SvClient::SetLoginStatus(bool aStatus)
{
	this->m_LoggedIn = aStatus;
}

void SvClient::SetIsPlayer(bool aIsPlayer)
{
	// only add observer on auth success
	if (aIsPlayer)
		this->m_Profile->addObserver(this->m_ProfileStateObserver);

	this->m_IsPlayer = aIsPlayer;
}

void SvClient::SetIsServer(bool aIsServer)
{
	this->m_IsServer = aIsServer;
}

bool SvClient::SendData(MN_WriteMessage *aMessage)
{
	return aMessage->SendMe(this->m_Socket);
}

MMG_Profile *SvClient::GetProfile()
{
	return this->m_Profile;
}

MMG_AuthToken *SvClient::GetToken()
{
	return this->m_AuthToken;
}

MMG_Options *SvClient::GetOptions()
{
	return this->m_Options;
}

SOCKET SvClient::GetSocket()
{
	return this->m_Socket;
}

uint SvClient::GetIPAddress()
{
	return this->m_IpAddress;
}

uint SvClient::GetPort()
{
	return this->m_Port;
}

void SvClient::Reset()
{
	// Invalidate to prevent any other lookups
	this->m_Valid = false;
	this->m_Index = 0;

	this->m_LastActiveTime	= 0;
	this->m_TimeoutTime		= 0;
	this->m_LoggedIn		= false;
	this->m_IsPlayer		= false;
	this->m_IsServer		= false;

	// Close the socket if it's open
	if (this->m_Socket != INVALID_SOCKET)
	{
		shutdown(this->m_Socket, 2/*SD_BOTH*/);
		closesocket(this->m_Socket);

		this->m_Socket = INVALID_SOCKET;
	}

	this->m_IpAddress	= 0;
	this->m_Port		= 0;

	// Reset encryption information
	this->m_CipherIdentifier			= CIPHER_UNKNOWN;
	this->m_EncryptionKeySequenceNumber = 0;
	memset(this->m_CipherKeys, 0, sizeof(this->m_CipherKeys));

	// Profile structure
	if (this->m_Profile)
	{
		delete this->m_Profile;
		this->m_Profile = nullptr;
	}

	// todo Profile state observer
	if (this->m_ProfileStateObserver)
	{
		delete this->m_ProfileStateObserver;
		this->m_ProfileStateObserver = nullptr;
	}

	// Authorization token structure
	if (this->m_AuthToken)
	{
		delete this->m_AuthToken;
		this->m_AuthToken = nullptr;
	}

	// Communication options structure
	if (this->m_Options)
	{
		delete this->m_Options;
		this->m_Options = nullptr;
	}
}

bool SvClient::CanReadFrom(bool *ErrorResult)
{
	// Timeout time of 10 milliseconds
	static timeval waitd = {0, 10000};

	// Check for an incoming connection
	fd_set readFlags;

	FD_ZERO(&readFlags);
	FD_SET(this->m_Socket, &readFlags);

	// Query the socket
	int result = select(0, &readFlags, nullptr, nullptr, &waitd);

	if (result < 0)
	{
		if (ErrorResult)
			*ErrorResult = true;

		return false;
	}

	// Check if the socket can be read from
	if (!FD_ISSET(this->m_Socket, &readFlags))
		return false;

	// Clear the read flag
	FD_CLR(this->m_Socket, &readFlags);

	return true;
}

//
// Begin SvClientManager
//
SvClientManager::SvClientManager()
{
	this->m_ThreadHandle			= nullptr;
	this->m_DataReceivedCallback	= nullptr;
	this->m_Clients					= nullptr;
	this->m_ClientCount				= 0;
	this->m_ClientMaxCount			= WIC_SERVER_MAX_CLIENTS;
}

SvClientManager::~SvClientManager()
{
	if (this->m_ThreadHandle)
	{
		TerminateThread(this->m_ThreadHandle, 0);
		CloseHandle(this->m_ThreadHandle);

		this->m_ThreadHandle = nullptr;
	}

	if (this->m_Clients)
	{
		VirtualFree(this->m_Clients, 0, MEM_RELEASE);
		this->m_Clients = nullptr;
	}
}

bool SvClientManager::Start()
{
	// Allocate an array for all clients
	this->m_Clients = (SvClient *)VirtualAlloc(nullptr, this->m_ClientMaxCount * sizeof(SvClient), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	if (!this->m_Clients)
		return false;

	// Create the network handler thread
	this->m_ThreadHandle = CreateThread(nullptr, 0, MainThread, this, 0, nullptr);

	if (this->m_ThreadHandle == nullptr)
		return false;

	return true;
}

void SvClientManager::SetDisconnectCallback(pfnDisconnectCallback aCallback)
{
	this->m_DisconnectCallback = aCallback;
}

void SvClientManager::SetDataCallback(pfnDataReceivedCallback aCallback)
{
	this->m_DataReceivedCallback = aCallback;
}

SvClient *SvClientManager::FindClient(sockaddr_in *aAddr)
{
	uint ip		= ntohl(aAddr->sin_addr.s_addr);
	uint port	= ntohs(aAddr->sin_port);

	this->m_Mutex.Lock();

	for(uint i = 0; i < this->m_ClientMaxCount; i++)
	{
		SvClient *client = &this->m_Clients[i];

		if (!client->m_Valid)
			continue;

		// Found a valid client, compare addresses
		if (client->m_IpAddress == ip && client->m_Port == port)
		{
			this->m_Mutex.Unlock();
			return client;
		}
	}

	this->m_Mutex.Unlock();

	return nullptr;
}

SvClient *SvClientManager::FindPlayerByProfileId(uint profileId)
{
	this->m_Mutex.Lock();

	for(uint i = 0; i < this->m_ClientMaxCount; i++)
	{
		SvClient *client = &this->m_Clients[i];

		if (!client->m_Valid || !client->m_LoggedIn || !client->m_IsPlayer)
			continue;

		if (client->m_Profile && client->m_Profile->m_ProfileId == profileId)
		{
			this->m_Mutex.Unlock();
			return client;
		}
	}

	this->m_Mutex.Unlock();

	return nullptr;
}

bool SvClientManager::AccountInUse(uint accountId)
{
	this->m_Mutex.Lock();

	for(uint i = 0; i < this->m_ClientMaxCount; i++)
	{
		SvClient *client = &this->m_Clients[i];

		if (!client->m_Valid || !client->m_LoggedIn || !client->m_IsPlayer)
			continue;

		if (client->m_AuthToken && client->m_AuthToken->m_AccountId == accountId)
		{
			this->m_Mutex.Unlock();
			return true;
		}
	}

	this->m_Mutex.Unlock();

	return false;
}

bool SvClientManager::ProfileInUse(uint profileId)
{
	this->m_Mutex.Lock();

	for(uint i = 0; i < this->m_ClientMaxCount; i++)
	{
		SvClient *client = &this->m_Clients[i];

		if (!client->m_Valid || !client->m_LoggedIn || !client->m_IsPlayer)
			continue;

		if (client->m_Profile && client->m_Profile->m_ProfileId == profileId)
		{
			this->m_Mutex.Unlock();
			return true;
		}
	}

	this->m_Mutex.Unlock();

	return false;
}

SvClient *SvClientManager::ConnectClient(SOCKET aSocket, sockaddr_in *aAddr)
{
	return this->AddClient(ntohl(aAddr->sin_addr.s_addr), ntohs(aAddr->sin_port), aSocket);
}

void SvClientManager::DisconnectClient(SvClient *aClient)
{
	if (!aClient->m_Valid)
		return;

	this->RemoveClient(aClient);
}

void SvClientManager::EmergencyDisconnectAll()
{
	for(uint i = 0; i < this->m_ClientMaxCount; i++)
		this->DisconnectClient(&this->m_Clients[i]);
}

bool SvClientManager::SendMessageToOnlinePlayers(MN_WriteMessage *aMessage)
{
	this->m_Mutex.Lock();

	// NOTE: This skips dedicated servers
	for(uint i = 0; i < this->m_ClientMaxCount; i++)
	{
		SvClient *client = &this->m_Clients[i];

		if (!client->m_Valid || !client->m_LoggedIn || !client->m_IsPlayer)
			continue;
		
		// Send message without clearing data
		if (!aMessage->SendMe(client->m_Socket, false))
			continue;
	}

	this->m_Mutex.Unlock();

	return true;
}

uint SvClientManager::GetClientCount()
{
	return this->m_ClientCount;
}

SvClient *SvClientManager::AddClient(uint aIpAddr, uint aPort, SOCKET aSocket)
{
	SvClient *slot = nullptr;

	this->m_Mutex.Lock();

	for(uint i = 0; i < this->m_ClientMaxCount; i++)
	{
		SvClient *client = &this->m_Clients[i];

		// Found unused slot
		if (!client->m_Valid)
		{
			client->m_Index = i;
			slot = client;

			break;
		}
	}

	if (slot)
	{
		slot->m_ProfileStateObserver = new MMG_Messaging::ProfileStateObserver();
		slot->m_Profile		= new MMG_Profile();
		slot->m_AuthToken	= new MMG_AuthToken();
		slot->m_Options		= new MMG_Options();
		slot->m_Socket		= aSocket;
		slot->m_IpAddress	= aIpAddr;
		slot->m_Port		= aPort;

		// Automatically set socket to non-blocking
		u_long sockMode = 1;
		ioctlsocket(aSocket, FIONBIO, &sockMode);

		slot->SetTimeout(WIC_DEFAULT_NET_TIMEOUT);
		slot->UpdateActivity();

		slot->m_Valid = true;

		// Update the total server client count
		InterlockedIncrement((volatile LONG *)&this->m_ClientCount);
	}

	this->m_Mutex.Unlock();

	return slot;
}

void SvClientManager::RemoveClient(SvClient *aClient)
{
	this->m_Mutex.Lock();

	// Notify external code
	if (this->m_DisconnectCallback)
		this->m_DisconnectCallback(aClient);

	// Decrement the total client count
	InterlockedDecrement((volatile LONG *)&this->m_ClientCount);

	// Reset variables
	aClient->Reset();

	this->m_Mutex.Unlock();
}

DWORD WINAPI SvClientManager::MainThread(LPVOID lpArg)
{
	SvClientManager *myManager = (SvClientManager *)lpArg;

	// Data buffer
	char recvBuffer[16384];
	memset(recvBuffer, 0, sizeof(recvBuffer));

	// Loop forever
	for(uint droppedClients = 0;; droppedClients = 0)
	{
		myManager->m_Mutex.Lock();

		for(uint i = 0; i < myManager->m_ClientMaxCount; i++)
		{
			SvClient *client = &myManager->m_Clients[i];

			if (!client->m_Valid)
				continue;

			// Check if the client timed out from the last response
			if (!client->IsActive())
			{
				myManager->RemoveClient(client);
				droppedClients++;

				continue;
			}

			// Set valid status before querying socket data
			bool socketError = false;

			if (client->CanReadFrom(&socketError))
			{
				int result = recv(client->GetSocket(), recvBuffer, sizeof(recvBuffer), 0);

				// Check if an internal error occurred or the socket disconnected
				if (result == SOCKET_ERROR || result == 0)
				{
					// Drop client asap
					socketError = true;
				}
				else
				{
					if (myManager->m_DataReceivedCallback)
						myManager->m_DataReceivedCallback(client, recvBuffer, result);
				}
			}

			// Something went wrong and the client needs to be removed
			if (socketError)
				myManager->RemoveClient(client);
		}

		myManager->m_Mutex.Unlock();

		if (droppedClients > 0)
			DebugLog(L_INFO, "%s(): Dropped %i client(s).", __FUNCTION__, droppedClients);

		// Prevent 100% CPU usage
		Sleep(10);
	}

	return 0;
}