#include "TCPChatServer.h"

TCPChatServer::TCPChatServer(ChatLobby& chat_int) : chat_interface(chat_int)
{

}
TCPChatServer::~TCPChatServer(void)
{

}
bool TCPChatServer::init(uint16_t port)
{
	m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	m_numClients = 0;
	m_freeID = 0;

	if (m_serverSocket == INVALID_SOCKET)
		return false;

	SOCKADDR_IN sAddress;
	sAddress.sin_family = AF_INET;
	sAddress.sin_port = htons(port);
	sAddress.sin_addr.s_addr = INADDR_ANY;
	int test;

	if (bind(m_serverSocket, (SOCKADDR*)&sAddress, sizeof(sAddress)) == SOCKET_ERROR)
	{
		return false;
	}

	if (listen(m_serverSocket, 5) == SOCKET_ERROR)
		return false;

	FD_ZERO(&m_sockets);
	FD_SET(m_serverSocket, &m_sockets);

	chat_interface.DisplayString("SERVER: NOW HOSTING SERVER");

	return true;
}
bool TCPChatServer::run(void)
{
	fd_set readSockets = m_sockets;
	int readableSockets = select(0, &readSockets, NULL, NULL, NULL);

	if (FD_ISSET(m_serverSocket, &readSockets))
	{
		SOCKET clientSocket = accept(m_serverSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET)
			return false;
		FD_SET(clientSocket, &m_sockets);
		if (m_freeID > 4 || m_numClients == MAX_CLIENTS) m_freeID = 4;
		m_clSockets[m_freeID++] = clientSocket;
		m_numClients++;
		readableSockets--;
		//return true;
	}

	uint16_t length;
	uint8_t type;

	for (size_t i = 0; i < MAX_CLIENTS + 1 && readableSockets > 0; i++)
	{				

		if (FD_ISSET(m_clSockets[i], &readSockets))
		{
			int result;
			if (recv(m_clSockets[i], (char*)&length, sizeof(uint16_t), 0) == SOCKET_ERROR ||
				recv(m_clSockets[i], (char*)&type, sizeof(uint8_t), 0) == SOCKET_ERROR)
			{
				result = WSAGetLastError();
				return false;
			}
			readableSockets--;

			switch (type)
			{
				case cl_reg:
				{
					if (m_numClients <= MAX_CLIENTS)
					{
						char buffer[17];
						length = 2;
						type = sv_cnt;

						if (recv(m_clSockets[i], (char*)&buffer, sizeof(buffer), 0) == SOCKET_ERROR)
							return false;

						tcpclient newClient;

						newClient.ID = i;
						strcpy(newClient.name, buffer);
						uint8_t ID = newClient.ID;

						//Register message
						char message[4];
						memcpy(message, (char*)&length, sizeof(uint16_t));
						memcpy(message + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));
						memcpy(message + sizeof(uint16_t)+sizeof(uint8_t), (char*)&ID, sizeof(uint8_t));
					
						if (send(m_clSockets[i], (char*)&message, sizeof(message), 0) == SOCKET_ERROR)
							return false;

						//Add message
						const unsigned int msgLength = sizeof(buffer)+sizeof(uint16_t)+(sizeof(uint8_t)* 2);
						uint16_t length = msgLength - 2;
						type = sv_add;
						char addMessage[msgLength];
						memcpy(addMessage, (char*)&length, sizeof(uint16_t));
						memcpy(addMessage + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));
						memcpy(addMessage + sizeof(uint16_t)+sizeof(uint8_t), (char*)&ID, sizeof(uint8_t));
						memcpy(addMessage + sizeof(uint16_t)+(sizeof(uint8_t) * 2), buffer, sizeof(buffer));

						//Send addMessage
						for (auto iter = m_clients.begin(); iter != m_clients.end(); iter++)
						{
							if (send(m_clSockets[iter->ID], addMessage, sizeof(addMessage), 0) == SOCKET_ERROR)
							{
								int result = WSAGetLastError();
								return false;
							}
						}

						//Add New registered client to our list of current clients
						m_clients.push_back(newClient);
					}
					else
					{	//Server Full
						m_freeID = 4;
						m_numClients--; 
						FD_CLR(m_clSockets[i], &m_sockets);
						length = 1;
						type = sv_full;
						char message[3];
						memcpy(message, (char*)&length, sizeof(uint16_t));
						memcpy(message + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));

						if (send(m_clSockets[i], (char*)&message, sizeof(message), 0) == SOCKET_ERROR)
							return false;
					}
					break;
				}
				case cl_get:
				{
					char userName[17] = {};
					uint8_t userID;
					uint8_t numInList = m_clients.size();
					length = (sizeof(uint8_t)* 2) + ((sizeof(userName)+sizeof(uint8_t)) * numInList);
					type = sv_list;

					//Message details
					char *message = new char[length + sizeof(uint16_t)];
					memcpy(message, (char*)&length, sizeof(uint16_t));
					memcpy(message + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));
					memcpy(message + sizeof(uint16_t)+sizeof(uint8_t), (char*)&numInList, sizeof(uint8_t));

					//List of clients
					unsigned int offset = sizeof(uint16_t)+(sizeof(uint8_t)* 2);
					for (size_t j = 0; j < numInList; j++)
					{
					   userID = m_clients[j].ID;
					   strcpy_s(userName, 17, m_clients[j].name);
					   memcpy(message + offset, (char*)&userID, sizeof(uint8_t));
					   offset += sizeof(uint8_t);
					   memcpy(message + offset, userName, sizeof(userName));
					   offset += sizeof(userName);
					}

					//Send the message
					int bitsSent = 0;
					int result;
					int msgLength = length + 2;
					while (bitsSent < msgLength)
					{
					   result = send(m_clSockets[i], message + bitsSent, msgLength - bitsSent, 0);
					   if (result == SOCKET_ERROR)
						   return false;
					   bitsSent += result;
					}

					delete[] message;
					break;
				}
				case sv_cl_msg:
				{
					//chat message
					uint8_t userID;
					if (tcp_recv_whole(m_clSockets[i], (char*)&userID, sizeof(uint8_t), 0) == SOCKET_ERROR)
					  return false;

					char * chatMessage = new char[length - 2];

					if (tcp_recv_whole(m_clSockets[i], chatMessage, (length - 2), 0) == SOCKET_ERROR)
					  return false;

					char * message = new char[length + 2];
					memcpy(message, (char*)&length, sizeof(uint16_t));
					memcpy(message + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));
					memcpy(message + sizeof(uint16_t)+sizeof(uint8_t), (char*)&userID, sizeof(uint8_t));
					memcpy(message + sizeof(uint16_t)+(sizeof(uint8_t)* 2), chatMessage, length - 2);

					for (auto iter = m_clients.begin(); iter != m_clients.end(); iter++)
					{
						if (send(m_clSockets[iter->ID], message, length + 2, 0) == SOCKET_ERROR)
							return false;
					}
					delete[] chatMessage;
					delete[] message;
					break;
				}
				case sv_cl_close:
				{
					uint8_t userID;

					if (recv(m_clSockets[i], (char*)&userID, sizeof(uint8_t), 0) == SOCKET_ERROR)
						return false;

					char message[4];
					length = 2;
					type = sv_remove;

					memcpy(message, (char*)&length, sizeof(uint16_t));
					memcpy(message + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));
					memcpy(message + sizeof(uint16_t)+sizeof(uint8_t), (char*)&userID, sizeof(uint8_t));

					//Delete client from tcp vector
					for (auto iter = m_clients.begin(); iter != m_clients.end(); iter++)
					{
						if (iter->ID == userID)
						{
							m_clients.erase(iter);
							break;
						}
					}
					//Send remove msg to all the others
					for (auto iter = m_clients.begin(); iter != m_clients.end(); iter++)
					{
						if (send(m_clSockets[iter->ID], message, length + 2, 0) == SOCKET_ERROR)
							return false;
					}

					FD_CLR(m_clSockets[i], &m_sockets);
					m_numClients--;
					m_freeID = userID;
					break;
				}
				default:
					break;
			}

		}
	}
	return true;
}
bool TCPChatServer::stop(void)
{
	uint16_t length = 2;
	uint8_t type = sv_cl_close;
	uint8_t ID = -1;

	char message[4];
	memcpy(message, (char*)&length, sizeof(uint16_t));
	memcpy(message + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));
	memcpy(message + sizeof(uint16_t)+sizeof(uint8_t), (char*)&ID, sizeof(uint8_t));

	for (auto iter = m_clients.begin(); iter != m_clients.end(); iter++)
	{
		if (send(m_clSockets[iter->ID], message, length + 2, 0) == SOCKET_ERROR)
		{
			int result = WSAGetLastError();
			return false;
		}
		shutdown(m_clSockets[iter->ID], SD_BOTH);
		closesocket(m_clSockets[iter->ID]);
	}

	m_clients.clear();
	shutdown(m_serverSocket, SD_BOTH);
	closesocket(m_serverSocket);
	return true;
}