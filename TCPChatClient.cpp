#include "TCPChatClient.h"

TCPChatClient::TCPChatClient(ChatLobby& chat_int) : chat_interface(chat_int)
{

}
TCPChatClient::~TCPChatClient(void)
{

}
bool TCPChatClient::init(std::string name, std::string ip_address, uint16_t port)
{

	m_clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (m_clientSocket == INVALID_SOCKET)
		return false;

	SOCKADDR_IN sAddress;
	sAddress.sin_family = AF_INET;
	sAddress.sin_port = htons(port);
	sAddress.sin_addr.s_addr = inet_addr(ip_address.c_str());
	if (sAddress.sin_addr.s_addr == INADDR_NONE)
		return false;

	if (connect(m_clientSocket, (SOCKADDR*)&sAddress, sizeof(sAddress)) == SOCKET_ERROR)
		return false;

	char message[20] = {};
	unsigned short length = 18;
	unsigned char type = cl_reg;

	memcpy(message, (char*)&length, sizeof(unsigned short));
	memcpy(message + sizeof(unsigned short), (char*)&type, sizeof(unsigned char));
	memcpy(message + sizeof(unsigned short)+1, (char*)name.c_str(), name.length());


	if (send(m_clientSocket, (char*)&message, sizeof(message), 0) == SOCKET_ERROR)
		return false;

	strcpy(m_client.name, name.c_str());
	chat_interface.DisplayString("CLIENT: CONNECTED TO SERVER");

	return true;
}
bool TCPChatClient::run(void)
{
	uint16_t length;
	uint8_t type;

	if (recv(m_clientSocket, (char*)&length, sizeof(uint16_t), 0) == SOCKET_ERROR)
		return false;

	if (recv(m_clientSocket, (char*)&type, sizeof(uint8_t),0) == SOCKET_ERROR)
		return false;

	switch (type)
	{
		case sv_cnt:
			length = 1;
			type = cl_get;
			char message[sizeof(uint16_t) + sizeof(uint8_t)];
			memcpy(message, (char*)&length, sizeof(uint16_t));
			memcpy(message + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));
			if (send(m_clientSocket, message, sizeof(message), 0) == SOCKET_ERROR)
				return false;

			if (recv(m_clientSocket, (char*)&m_client.ID, sizeof(unsigned char), 0) == SOCKET_ERROR)
				return false;
			chat_interface.DisplayString("CLIENT: REGISTERED IN SERVER");

			return true;

		case sv_full:
			chat_interface.DisplayString("CLIENT: SERVER FULL");
			return false;

		case sv_list:
		{
			uint8_t listSize;
			if (recv(m_clientSocket, (char*)&listSize, sizeof(uint8_t), 0) == SOCKET_ERROR)
				return false;

			char name[17];
			uint8_t ID;
			do
			{
				if (recv(m_clientSocket, (char*)&ID, sizeof(uint8_t), 0) == SOCKET_ERROR)
					return false;
				if (recv(m_clientSocket, (char*)&name, sizeof(name), 0) == SOCKET_ERROR)
					return false;

				std::string S_name;
				S_name = name;
				chat_interface.AddNameToUserList(S_name, ID);
				clients[ID] = S_name;
				listSize--;
			} while (listSize != 0);
			chat_interface.DisplayString("CLIENT: RECEIVED USER LIST");
			return true;

		}
		case sv_add:
		{
			char name[17];
			uint8_t ID;

			if (recv(m_clientSocket, (char*)&ID, sizeof(uint8_t), 0) == SOCKET_ERROR)
			   return false;
			if (recv(m_clientSocket, (char*)&name, sizeof(name), 0) == SOCKET_ERROR)
			   return false;

			std::string S_name;
			S_name = name;
			chat_interface.AddNameToUserList(S_name, ID);
			chat_interface.DisplayString("CLIENT: " + S_name + " JOINED");
			clients[ID] = S_name;
			return true;
		}

		case sv_remove:
		{
			uint8_t ID;
			std::string name;
			if (recv(m_clientSocket, (char*)&ID, sizeof(uint8_t), 0) == SOCKET_ERROR)
				return false;
			chat_interface.RemoveNameFromUserList(ID);
			chat_interface.DisplayString("CLIENT: " + clients[ID] + " LEFT");
			return true;
		}
		
		case sv_cl_msg:
		{
			uint8_t ID;
			if (recv(m_clientSocket, (char*)&ID, sizeof(uint8_t), 0) == SOCKET_ERROR)
				return false;
			
			uint16_t msgLength = length - (sizeof(uint8_t)* 2);
			char* buffer = new char[msgLength];

			if (tcp_recv_whole(m_clientSocket, buffer, msgLength, 0) == SOCKET_ERROR)
				return false;

			std::string message = buffer;
			chat_interface.AddChatMessage(message, ID);

			delete[] buffer;
			return true;
		}
	}

}
bool TCPChatClient::send_message(std::string message)
{
	uint16_t msgLength = (uint16_t)(message.size() + sizeof(uint16_t) + (sizeof(uint8_t)*2) + 1);
	uint16_t length = message.size() + 3;
	uint8_t msgType = sv_cl_msg;
	uint8_t ID = m_client.ID;
	char * buffer = new char[msgLength];
	
	memcpy(buffer, (char*)&length, sizeof(uint16_t));
	memcpy(buffer + sizeof(uint16_t), (char*)&msgType, sizeof(uint8_t));
	memcpy(buffer + sizeof(uint16_t) + sizeof(uint8_t), (char*)&ID, sizeof(uint8_t));
	memcpy(buffer + sizeof(uint16_t) + (sizeof(uint8_t) *2), message.c_str(), message.size());
	buffer[msgLength - 1] = '\0';

	int bitsSent = 0;
	int result;
	while (bitsSent < msgLength)
	{
		result = send(m_clientSocket, buffer + bitsSent, msgLength - bitsSent, 0);
		if ( result == SOCKET_ERROR)
			return false;
		bitsSent += result;
	}
	delete[] buffer;
	return true;

}
bool TCPChatClient::stop(void)
{
	uint16_t length = 2;
	uint8_t type = sv_cl_close;
	char buffer[sizeof(uint16_t)+(sizeof(uint8_t)* 2)];
	memcpy(buffer, (char*)&length, sizeof(uint16_t));
	memcpy(buffer + sizeof(uint16_t), (char*)&type, sizeof(uint8_t));
	memcpy(buffer + sizeof(uint16_t)+sizeof(uint8_t), (char*)&m_client.ID, sizeof(unsigned char));

	if (send(m_clientSocket, (char*)&buffer, sizeof(buffer),0 ) == SOCKET_ERROR)
		return false;

	shutdown(m_clientSocket, SD_BOTH);
	closesocket(m_clientSocket);

	return true;
}
