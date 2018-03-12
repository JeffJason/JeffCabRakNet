#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"
#include <iostream>
#include <string>
#include <string.h>
#include <thread>         // std::thread
#include <chrono>
#include <map>
#include <algorithm>
#include <cstring>


static int SERVER_PORT = 65000;
static int CLIENT_PORT = 65001;
static int MAX_CONNECTIONS = 3;

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;

bool isServer = false;
bool isRunning = true;
bool signingIn = false;
bool isReady = false;
bool pickClass = false;
bool myTurn = false;
bool isDead = false;

unsigned short g_totalPlayers = 0;

int numReady = 0;
int pickAClass = 0;
int currentPlayerIndex = 0;
int playersAlive = 0;

enum {
	ID_THEGAME_LOBBY = ID_USER_PACKET_ENUM,
	ID_THEGAME_READY,
	ID_THEGAME_CHOOSECLASS,
	ID_THEGAME_ACTION,
	ID_CHANGE_TO_SELECT,
	ID_CHANGE_TO_PLAY,
	ID_REQUEST_STATS,
	ID_GET_TARGETS,
	ID_NEXT_TURN,
	ID_HEAL_SELF,
	ID_ATTACK,
	ID_PRINT_THIS,
	ID_TURN_TRUE,
	ID_TURN_FALSE,
	ID_DEATH,
	ID_GAMEOVER
};

class m_class
{
public:
	std::string className;
	int health;
	int strength;

	m_class(int h, int s, std::string name) {
		className = name;
		health = h;
		strength = s;
	}

	void Damaging(int damage) {
		health -= damage;
	}

	int Attacking() {
		int damage = rand() % (strength / 2);

		return damage;
	}

	int Healing() {
		int healAmount = rand() % (health / 2);
		health += healAmount;
		return healAmount;
	}

	bool isDead() {
		if (health <= 0) {
			return true;
		}
		else {
			return false;
		}
	}

	void printClass() {
		std::cout << "Chosen class: " << std::endl;
		std::cout << className << "; Strength " << strength << ", Health " << health << std::endl;
	}
};

m_class warrior = m_class(100, 30, "Warrior");
m_class archer = m_class(50, 50, "Archer");
m_class mage = m_class(30, 100, "Mage");

struct SPlayer
{
	std::string name;
	RakNet::SystemAddress address;
	int playerIndex;
	m_class playerClass = m_class(0, 0, "");
	bool isAlive = true;
};

RakNet::SystemAddress g_serverAddress;

std::map<unsigned long, SPlayer> m_playerMap;


enum NetworkStates
{
	NS_Decision = 0,
	NS_CreateSocket,
	NS_PendingConnection,
	NS_Connected,
	NS_Running,
	NS_Lobby,
	NS_CharSelect,
	NS_Game,
	NS_Dead,
	NS_GameOver
};

void sendPacketsToClients(RakNet::MessageID id, std::string text)
{
	//send packet back
	std::string textIn = text;
	RakNet::BitStream myBitStream;
	//first thing to write, is packet message identifier
	myBitStream.Write(id);
	RakNet::RakString name(textIn.c_str());
	myBitStream.Write(name);

	for (auto const& x : m_playerMap)
	{
		g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, x.second.address, false);
	}

}

NetworkStates g_networkState = NS_Decision;

void OnIncomingConnection(RakNet::Packet* packet)
{
	if (!isServer)
	{
		assert(0);
	}
	g_totalPlayers++;

	m_playerMap.insert(std::make_pair(RakNet::RakNetGUID::ToUint32(packet->guid), SPlayer()));

	unsigned short numConnections = g_rakPeerInterface->NumberOfConnections();
	std::cout << "Total Players: " << m_playerMap.size() << ". Num Connection: " << numConnections << std::endl;
}

void OnConnectionAccepted(RakNet::Packet* packet)
{
	if (isServer)
	{
		//server should never request connections, only clients do
		assert(0);
	}
	//we have successfully connected, go to lobby
	g_networkState = NS_Lobby;
	g_serverAddress = packet->systemAddress;
}

void InputHandler()
{
	while (isRunning)
	{
		char userInput[255];
		if (g_networkState == NS_Decision)
		{
			std::cout << "Press (s) for server, (c) for client" << std::endl;
			std::cin >> userInput;
			isServer = userInput[0] == 's';
			g_networkState = NS_CreateSocket;
		}
		else if (g_networkState == NS_CreateSocket)
		{
			if (isServer)
			{
				std::cout << "Server creating socket..." << std::endl;
			}
			else
			{
				std::cout << "Client creating socket..." << std::endl;
			}
		}
		else if (g_networkState == NS_Lobby)
		{
			if (signingIn == false) {
				std::cout << "If you would like to play this game, enter your name" << std::endl;
				std::cout << "if you want to quit, type quit. " << std::endl;
				std::cin >> userInput;
				if (strcmp(userInput, "quit") == 0)
				{
					//heartbreaking
					assert(0);
				}
				else
				{
					//send our first packet
					RakNet::BitStream myBitStream;
					//first thing to write, is packet message identifier
					myBitStream.Write((RakNet::MessageID)ID_THEGAME_LOBBY);
					RakNet::RakString name(userInput);
					myBitStream.Write(name);
					//virtual uint32_t Send(const RakNet::BitStream * bitStream, PacketPriority priority, PacketReliability reliability, char orderingChannel, const AddressOrGUID systemIdentifier, bool broadcast, uint32_t forceReceiptNumber = 0) = 0;
					g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);

					signingIn = true;

				}
			}
			else if (signingIn == true && isReady == false)
			{
				std::cout << "Enter ready before starting the game." << std::endl;
				std::cout << "If you want to quit, type quit. " << std::endl;
				std::cin >> userInput;
				if (strcmp(userInput, "quit") == 0)
				{
					//heartbreaking
					assert(0);
				}
				else if (strcmp(userInput, "ready") == 0)
				{
					//send our first packet
					RakNet::BitStream myBitStream;
					//first thing to write, is packet message identifier
					myBitStream.Write((RakNet::MessageID)ID_THEGAME_READY);
					RakNet::RakString name(userInput);
					myBitStream.Write(name);
					//virtual uint32_t Send(const RakNet::BitStream * bitStream, PacketPriority priority, PacketReliability reliability, char orderingChannel, const AddressOrGUID systemIdentifier, bool broadcast, uint32_t forceReceiptNumber = 0) = 0;
					g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);

					isReady = true;
					std::cout << "Waiting for all players to be ready. . ." << std::endl;
				}
			}
		}
		else if (g_networkState == NS_CharSelect)
		{
			if (pickClass == false) {
				std::cout << "Choose your class!" << std::endl;
				std::cout << "To choose enter the name of one of the following classes:" << std::endl;
				std::cout << "Warrior; Strength 30, Health 100" << std::endl;
				std::cout << "Archer; Strength 50, Health 50" << std::endl;
				std::cout << "Mage; Strength 100, Health 30" << std::endl;
				std::cin >> userInput;

				if (strcmp(userInput, "Warrior") == 0 || strcmp(userInput, "Archer") == 0 || strcmp(userInput, "Mage") == 0)
				{
					//send our first packet
					RakNet::BitStream myBitStream;
					//first thing to write, is packet message identifier
					myBitStream.Write((RakNet::MessageID)ID_THEGAME_CHOOSECLASS);
					RakNet::RakString name(userInput);
					myBitStream.Write(name);
					//virtual uint32_t Send(const RakNet::BitStream * bitStream, PacketPriority priority, PacketReliability reliability, char orderingChannel, const AddressOrGUID systemIdentifier, bool broadcast, uint32_t forceReceiptNumber = 0) = 0;
					g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);

					pickClass = true;
					std::cout << "Waiting for remaining players. . ." << std::endl;
				}
			}
		}
		else if (g_networkState == NS_Game)
		{
			if (isDead == false) {
				if (myTurn == true) {
					std::cout << "\nIt is your turn." << std::endl;
					std::cout << "You can attack, or heal." << std::endl;
					//Getting attack targets
					//send our first packet
					RakNet::BitStream myBitStream;
					//first thing to write, is packet message identifier
					myBitStream.Write((RakNet::MessageID)ID_GET_TARGETS);
					RakNet::RakString name(userInput);
					myBitStream.Write(name);
					g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);


					std::cout << "Enter in a player's name to attack them." << std::endl;
					std::cout << "Enter in 'heal' to heal yourself." << std::endl;
					std::cout << "Enter 'stats' to get your class stats!\n" << std::endl;
					std::cin >> userInput;

					if (strcmp(userInput, "stats") == 0)
					{
						//send our first packet
						RakNet::BitStream myBitStream;
						//first thing to write, is packet message identifier
						myBitStream.Write((RakNet::MessageID)ID_REQUEST_STATS);
						RakNet::RakString name(userInput);
						myBitStream.Write(name);
						g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
					}
					else if (strcmp(userInput, "heal") == 0)
					{
						RakNet::BitStream myBitStream;
						myBitStream.Write((RakNet::MessageID)ID_HEAL_SELF);
						RakNet::RakString name(userInput);
						myBitStream.Write(name);
						g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
						myTurn = false;

						//Sending turn swap packet to server
						RakNet::BitStream myBitStream2;
						myBitStream2.Write((RakNet::MessageID)ID_NEXT_TURN);
						RakNet::RakString name2(userInput);
						myBitStream2.Write(name2);
						g_rakPeerInterface->Send(&myBitStream2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
					}
					else
					{
						RakNet::BitStream myBitStream;
						myBitStream.Write((RakNet::MessageID)ID_ATTACK);
						RakNet::RakString name(userInput);
						myBitStream.Write(name);
						g_rakPeerInterface->Send(&myBitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
						myTurn = false;

						//Sending turn swap packet to server
						RakNet::BitStream myBitStream2;
						myBitStream2.Write((RakNet::MessageID)ID_NEXT_TURN);
						RakNet::RakString name2(userInput);
						myBitStream2.Write(name2);
						g_rakPeerInterface->Send(&myBitStream2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
					}
				}
			}
		}
		else if (g_networkState == NS_Game)
		{
			if (isDead == true)
			{
				std::cout << "You Lose!" << std::endl;
			}
			else if (isDead == false)
			{
				std::cout << "You Win!" << std::endl;
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

unsigned char GetPacketIdentifier(RakNet::Packet *packet)
{
	if (packet == nullptr)
		return 255;

	if ((unsigned char)packet->data[0] == ID_TIMESTAMP)
	{
		RakAssert(packet->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)packet->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	}
	else
		return (unsigned char)packet->data[0];
}

bool HandleLowLevelPacket(RakNet::Packet* packet)
{
	bool isHandled = true;
	unsigned char packetIdentifier = GetPacketIdentifier(packet);
	switch (packetIdentifier)
	{
	case ID_DISCONNECTION_NOTIFICATION:
		// Connection lost normally
		printf("ID_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_ALREADY_CONNECTED:
		// Connection lost normally
		printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", packet->guid);
		break;
	case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
		break;
	case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_CONNECTION_LOST\n");
		break;
	case ID_NEW_INCOMING_CONNECTION:
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
		OnIncomingConnection(packet);
		break;
	case ID_CONNECTION_BANNED: // Banned from this server
		printf("We are banned from this server.\n");
		break;
	case ID_CONNECTION_ATTEMPT_FAILED:
		printf("Connection attempt failed\n");
		break;
	case ID_NO_FREE_INCOMING_CONNECTIONS:
		// Sorry, the server is full.  I don't do anything here but
		// A real app should tell the user
		printf("ID_NO_FREE_INCOMING_CONNECTIONS(Server Full)\n");
		break;
	case ID_INVALID_PASSWORD:
		printf("ID_INVALID_PASSWORD\n");
		break;
	case ID_CONNECTION_LOST:
		// Couldn't deliver a reliable packet - i.e. the other system was abnormally
		// terminated
		printf("ID_CONNECTION_LOST\n");
		break;
	case ID_CONNECTION_REQUEST_ACCEPTED:
		// This tells the client they have connected
		printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		printf("My external address is %s\n", g_rakPeerInterface->GetExternalID(packet->systemAddress).ToString(true));
		OnConnectionAccepted(packet);
		break;
	case ID_CONNECTED_PING:
	case ID_UNCONNECTED_PING:
		printf("Ping from %s\n", packet->systemAddress.ToString(true));
		break;

	case ID_CHANGE_TO_SELECT:
		g_networkState = NS_CharSelect;
		break;

	case ID_CHANGE_TO_PLAY:
		g_networkState = NS_Game;
		break;

	case ID_PRINT_THIS://Client Printing
	{
		RakNet::BitStream myBitStream(packet->data, packet->length, false); // The false is for efficiency so we don't make a copy of the passed data
		RakNet::MessageID messageID;
		myBitStream.Read(messageID);
		RakNet::RakString input;
		myBitStream.Read(input);

		std::cout << input << std::endl;

		break;
	}

	case ID_TURN_TRUE:
	{
		myTurn = true;
		break;
	}

	case ID_TURN_FALSE:
	{
		myTurn = false;
		break;
	}

	case ID_DEATH:
	{
		isDead = true;
		break;
	}

	case ID_GAMEOVER:
	{
		std::cout << "\nGame Over" << std::endl;

		if (isDead == true)
		{
			std::cout << "You Lose!" << std::endl;
		}
		else if (isDead == false)
		{
			std::cout << "You Win!" << std::endl;
		}

		g_networkState = NS_GameOver;
		break;
	}

	default:
		isHandled = false;
		break;
	}
	return isHandled;
}

void PacketHandler()
{
	while (isRunning)
	{
		for (RakNet::Packet* packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
			// We got a packet, get the identifier with our handy function

			if (!HandleLowLevelPacket(packet))
			{
				unsigned char packetIdentifier = GetPacketIdentifier(packet);
				switch (packetIdentifier)
				{
				case ID_THEGAME_LOBBY:
				{
					RakNet::BitStream myBitStream(packet->data, packet->length, false); // The false is for efficiency so we don't make a copy of the passed data
					RakNet::MessageID messageID;
					myBitStream.Read(messageID);
					RakNet::RakString userName;
					myBitStream.Read(userName);

					//storing player guid, name, and adress
					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
					m_playerMap.find(guid)->second.name = userName;
					m_playerMap.find(guid)->second.address = packet->systemAddress;
					m_playerMap.find(guid)->second.playerIndex = g_totalPlayers;

					//example of sending using player map
					//g_rakPeerInterface->Send(&myBitStream2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guid)->second.address, false);

					std::cout << userName << " is logged in! " << std::endl;
					break;
				}


				case ID_THEGAME_READY:
				{
					RakNet::BitStream myBitStream(packet->data, packet->length, false); // The false is for efficiency so we don't make a copy of the passed data
					RakNet::MessageID messageID;
					myBitStream.Read(messageID);
					RakNet::RakString userName;
					myBitStream.Read(userName);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
					std::string playerName = m_playerMap.find(guid)->second.name;
					m_playerMap.find(guid)->second.playerIndex = numReady;

					std::cout << playerName.c_str() << " is ready to play! " << std::endl;

					numReady++;

					if (numReady >= g_totalPlayers) {
						playersAlive = g_totalPlayers;
						std::cout << "Choosing a CLASS" << std::endl;
						sendPacketsToClients(ID_CHANGE_TO_SELECT, "null");
					}
					break;
				}

				case ID_THEGAME_CHOOSECLASS:
				{
					RakNet::BitStream myBitStream(packet->data, packet->length, false); // The false is for efficiency so we don't make a copy of the passed data
					RakNet::MessageID messageID;
					myBitStream.Read(messageID);
					RakNet::RakString className;
					myBitStream.Read(className);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);

					if (className == "Warrior")
					{
						m_playerMap.find(guid)->second.playerClass = warrior;
					}

					if (className == "Archer")
					{
						m_playerMap.find(guid)->second.playerClass = archer;
					}

					if (className == "Mage")
					{
						m_playerMap.find(guid)->second.playerClass = mage;
					}

					pickAClass++;

					if (pickAClass >= g_totalPlayers)
					{
						unsigned long guidTemp;
						for (auto const& x : m_playerMap)
						{
							if (x.second.playerIndex == currentPlayerIndex)
							{
								guidTemp = x.first;
							}
						}
						std::string nullTxt;
						RakNet::BitStream myBitStreamSetTurn;
						myBitStreamSetTurn.Write((RakNet::MessageID)ID_TURN_TRUE);
						RakNet::RakString writerturn(nullTxt.c_str());
						myBitStreamSetTurn.Write(writerturn);

						g_rakPeerInterface->Send(&myBitStreamSetTurn, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guidTemp)->second.address, false);

						//Startup messages to every player
						std::string input = "Game Started!\n";

						RakNet::BitStream myBitStreamOut;
						myBitStreamOut.Write((RakNet::MessageID)ID_PRINT_THIS);
						RakNet::RakString writer(input.c_str());
						myBitStreamOut.Write(writer);

						g_rakPeerInterface->Send(&myBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

						//Telling who's turn it is
						std::string input2 = "\nIt is " + m_playerMap.find(guidTemp)->second.name + "'s turn!\n";

						RakNet::BitStream myBitStreamOutSecond;
						myBitStreamOutSecond.Write((RakNet::MessageID)ID_PRINT_THIS);
						RakNet::RakString writer2nd(input2.c_str());
						myBitStreamOutSecond.Write(writer2nd);

						g_rakPeerInterface->Send(&myBitStreamOutSecond, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guidTemp)->second.address, true);
						std::cout << "GameState: Game Loop" << std::endl;

						//Starting Play
						sendPacketsToClients(ID_CHANGE_TO_PLAY, "null");
					}
					break;
				}

				case ID_GET_TARGETS:
				{
					RakNet::BitStream myBitStream(packet->data, packet->length, false); // The false is for efficiency so we don't make a copy of the passed data
					RakNet::MessageID messageID;
					myBitStream.Read(messageID);
					RakNet::RakString nullIn;
					myBitStream.Read(nullIn);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);

					std::string input = "Possible attack targets:";

					RakNet::BitStream myBitStreamOut;
					myBitStreamOut.Write((RakNet::MessageID)ID_PRINT_THIS);
					RakNet::RakString writer(input.c_str());
					myBitStreamOut.Write(writer);

					g_rakPeerInterface->Send(&myBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

					for (auto const& x : m_playerMap)
					{
						if (x.first != guid)
						{
							if (x.second.isAlive == true)
							{
								std::string input = x.second.name;

								//Name of Class
								RakNet::BitStream myBitStreamOut;
								myBitStreamOut.Write((RakNet::MessageID)ID_PRINT_THIS);
								RakNet::RakString writer(input.c_str());
								myBitStreamOut.Write(writer);

								g_rakPeerInterface->Send(&myBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
							}
						}
					}

					//Formatting the new line
					std::string input2 = "\n";

					RakNet::BitStream myBitStreamOutSecond;
					myBitStreamOutSecond.Write((RakNet::MessageID)ID_PRINT_THIS);
					RakNet::RakString writer2nd(input2.c_str());
					myBitStreamOutSecond.Write(writer2nd);

					g_rakPeerInterface->Send(&myBitStreamOutSecond, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

					break;
				}

				case ID_REQUEST_STATS:
				{
					RakNet::BitStream myBitStream(packet->data, packet->length, false); // The false is for efficiency so we don't make a copy of the passed data
					RakNet::MessageID messageID;
					myBitStream.Read(messageID);
					RakNet::RakString nullIn;
					myBitStream.Read(nullIn);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);

					//send packets back
					//Data
					std::string input = "Class: " + m_playerMap.find(guid)->second.playerClass.className;
					std::string input2 = "Health: " + std::to_string(m_playerMap.find(guid)->second.playerClass.health);
					std::string input3 = "Strength: " + std::to_string(m_playerMap.find(guid)->second.playerClass.strength) + "\n";

					//Name of Class
					RakNet::BitStream myBitStreamOut;
					myBitStreamOut.Write((RakNet::MessageID)ID_PRINT_THIS);
					RakNet::RakString writer(input.c_str());
					myBitStreamOut.Write(writer);

					g_rakPeerInterface->Send(&myBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

					//Health of Class
					RakNet::BitStream myBitStreamOutSecond;
					myBitStreamOutSecond.Write((RakNet::MessageID)ID_PRINT_THIS);
					RakNet::RakString writer2nd(input2.c_str());
					myBitStreamOutSecond.Write(writer2nd);

					g_rakPeerInterface->Send(&myBitStreamOutSecond, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);

					//Strength of Class
					RakNet::BitStream myBitStreamOutThird;
					myBitStreamOutThird.Write((RakNet::MessageID)ID_PRINT_THIS);
					RakNet::RakString writer3rd(input3.c_str());
					myBitStreamOutThird.Write(writer3rd);

					g_rakPeerInterface->Send(&myBitStreamOutThird, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);


					break;
				}

				case ID_NEXT_TURN:
				{
					if (playersAlive > 1) {
						unsigned long guidTemp;
						for (auto const& x : m_playerMap)
						{
							if (x.second.playerIndex == currentPlayerIndex)
							{
								if (x.second.isAlive == false)
								{
									currentPlayerIndex++;
									if (currentPlayerIndex >= g_totalPlayers) {
										currentPlayerIndex = 0;
									}
								}
								else
								{
									guidTemp = x.first;
								}
							}
						}
						std::string nullTxt;
						RakNet::BitStream myBitStreamSetTurn;
						myBitStreamSetTurn.Write((RakNet::MessageID)ID_TURN_TRUE);
						RakNet::RakString writerturn(nullTxt.c_str());
						myBitStreamSetTurn.Write(writerturn);

						g_rakPeerInterface->Send(&myBitStreamSetTurn, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guidTemp)->second.address, false);

						//Telling who's turn it is
						std::string input2 = "\nIt is " + m_playerMap.find(guidTemp)->second.name + "'s turn!\n";

						RakNet::BitStream myBitStreamOutSecond;
						myBitStreamOutSecond.Write((RakNet::MessageID)ID_PRINT_THIS);
						RakNet::RakString writer2nd(input2.c_str());
						myBitStreamOutSecond.Write(writer2nd);

						g_rakPeerInterface->Send(&myBitStreamOutSecond, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guidTemp)->second.address, true);
					}
					break;
				}

				case ID_HEAL_SELF:
				{
					RakNet::BitStream myBitStream(packet->data, packet->length, false); // The false is for efficiency so we don't make a copy of the passed data
					RakNet::MessageID messageID;
					myBitStream.Read(messageID);
					RakNet::RakString nullIn;
					myBitStream.Read(nullIn);

					unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);


					//sending heal amount to player who healed
					int healAmount = m_playerMap.find(guid)->second.playerClass.Healing();
					std::string input = "\n You healed for: " + std::to_string(healAmount) + ".\n You now have: " + std::to_string(m_playerMap.find(guid)->second.playerClass.health) + " health.";

					RakNet::BitStream myBitStreamOut;
					myBitStreamOut.Write((RakNet::MessageID)ID_PRINT_THIS);
					RakNet::RakString writer(input.c_str());
					myBitStreamOut.Write(writer);

					g_rakPeerInterface->Send(&myBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guid)->second.address, false);

					//informing other client's of player's action
					std::string input2 = "    " + m_playerMap.find(guid)->second.name + " healed for: " + std::to_string(healAmount) + ".\n They now have: " + std::to_string(m_playerMap.find(guid)->second.playerClass.health) + " health.";

					RakNet::BitStream myBitStreamOutSecond;
					myBitStreamOutSecond.Write((RakNet::MessageID)ID_PRINT_THIS);
					RakNet::RakString writer2nd(input2.c_str());
					myBitStreamOutSecond.Write(writer2nd);

					g_rakPeerInterface->Send(&myBitStreamOutSecond, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guid)->second.address, true);

					//iterating up the player index
					currentPlayerIndex++;
					if (currentPlayerIndex >= g_totalPlayers) {
						currentPlayerIndex = 0;
					}
					break;
				}

				case ID_ATTACK:
				{
					RakNet::BitStream myBitStream(packet->data, packet->length, false); // The false is for efficiency so we don't make a copy of the passed data
					RakNet::MessageID messageID;
					myBitStream.Read(messageID);
					RakNet::RakString name;
					myBitStream.Read(name);

					unsigned long inputGuid = RakNet::RakNetGUID::ToUint32(packet->guid);

					unsigned long guidTemp;
					for (auto const& x : m_playerMap)
					{
						std::cout << x.second.name.c_str() << ", input: " << name << std::endl;

						if (strcmp(x.second.name.c_str(), name) == 0) {
							guidTemp = x.first;
						}
					}

					if (m_playerMap.find(guidTemp)->second.isAlive == true)
					{

						int attackDamage = m_playerMap.find(inputGuid)->second.playerClass.Attacking();
						m_playerMap.find(guidTemp)->second.playerClass.Damaging(attackDamage);

						//sending heal amount to player who healed
						std::string input = "\n You dealt " + std::to_string(attackDamage) + " damage to " + m_playerMap.find(guidTemp)->second.name +
							".\n They now have: " + std::to_string(m_playerMap.find(guidTemp)->second.playerClass.health) + " health.";

						RakNet::BitStream myBitStreamOut;
						myBitStreamOut.Write((RakNet::MessageID)ID_PRINT_THIS);
						RakNet::RakString writer(input.c_str());
						myBitStreamOut.Write(writer);

						g_rakPeerInterface->Send(&myBitStreamOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(inputGuid)->second.address, false);

						//informing other client's of player's action
						std::string input2 = "      " + m_playerMap.find(inputGuid)->second.name + " delt " + std::to_string(attackDamage) + " damage to " + m_playerMap.find(guidTemp)->second.name +
							".\n  " + m_playerMap.find(guidTemp)->second.name + " now has: " + std::to_string(m_playerMap.find(guidTemp)->second.playerClass.health) + " health.";

						RakNet::BitStream myBitStreamOutSecond;
						myBitStreamOutSecond.Write((RakNet::MessageID)ID_PRINT_THIS);
						RakNet::RakString writer2nd(input2.c_str());
						myBitStreamOutSecond.Write(writer2nd);

						g_rakPeerInterface->Send(&myBitStreamOutSecond, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(inputGuid)->second.address, true);

						//Death Check
						if (m_playerMap.find(guidTemp)->second.playerClass.isDead())
						{
							//Sending death alert
							RakNet::BitStream myBitStreamDeath;
							myBitStreamDeath.Write((RakNet::MessageID)ID_DEATH);
							RakNet::RakString writer(input2.c_str());
							myBitStreamDeath.Write(writer);

							g_rakPeerInterface->Send(&myBitStreamDeath, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guidTemp)->second.address, false);

							m_playerMap.find(guidTemp)->second.isAlive = false;

							playersAlive--;

							//Sending other client of death alert
							std::string input = "      " + m_playerMap.find(guidTemp)->second.name + " has died!";

							RakNet::BitStream myBitStreamOutSecond;
							myBitStreamOutSecond.Write((RakNet::MessageID)ID_PRINT_THIS);
							RakNet::RakString writer2nd(input.c_str());
							myBitStreamOutSecond.Write(writer2nd);

							g_rakPeerInterface->Send(&myBitStreamOutSecond, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guidTemp)->second.address, true);

							//Player death info
							std::string input2 = "      You have died!";

							RakNet::BitStream myBitStreamOutThird;
							myBitStreamOutThird.Write((RakNet::MessageID)ID_PRINT_THIS);
							RakNet::RakString writer3rd(input2.c_str());
							myBitStreamOutThird.Write(writer3rd);

							g_rakPeerInterface->Send(&myBitStreamOutThird, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(guidTemp)->second.address, false);
						}
					}
					else //If player is dead
					{
						std::string input2 = "      That Target is already dead! Next turn.\n";

						RakNet::BitStream myBitStreamOutFourth;
						myBitStreamOutFourth.Write((RakNet::MessageID)ID_PRINT_THIS);
						RakNet::RakString writer2nd(input2.c_str());
						myBitStreamOutFourth.Write(writer2nd);

						g_rakPeerInterface->Send(&myBitStreamOutFourth, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(inputGuid)->second.address, true);
						g_rakPeerInterface->Send(&myBitStreamOutFourth, HIGH_PRIORITY, RELIABLE_ORDERED, 0, m_playerMap.find(inputGuid)->second.address, false);
					}

					if (playersAlive > 1) {
					
						currentPlayerIndex++;
						if (currentPlayerIndex >= g_totalPlayers) {
							currentPlayerIndex = 0;
						}
					}
					else //End Game
					{
						sendPacketsToClients(ID_GAMEOVER, "null");
						std::cout << "GameState: Game End" << std::endl;
					}
					break;
				}

				default:
					// It's a client, so just show the message
					printf("%s\n", packet->data);
					break;
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}//while isRunning
}

int main()
{
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

	std::thread inputHandler(InputHandler);
	std::thread packetHandler(PacketHandler);


	while (isRunning)
	{
		if (g_networkState == NS_CreateSocket)
		{
			if (isServer)
			{
				//opening up the server socket
				RakNet::SocketDescriptor socketDescriptors[1];
				socketDescriptors[0].port = SERVER_PORT;
				socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4
				bool isSuccess = g_rakPeerInterface->Startup(MAX_CONNECTIONS, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
				assert(isSuccess);
				g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
				g_networkState = NS_PendingConnection;
				std::cout << "Server waiting on connections.." << std::endl;
			}
			else
			{

				//creating a socket for communication
				RakNet::SocketDescriptor socketDescriptor(CLIENT_PORT, nullptr);
				socketDescriptor.socketFamily = AF_INET;

				while (RakNet::IRNS2_Berkley::IsPortInUse(socketDescriptor.port, socketDescriptor.hostAddress, socketDescriptor.socketFamily, SOCK_DGRAM) == true)
					socketDescriptor.port++;

				g_rakPeerInterface->Startup(8, &socketDescriptor, 1);

				//client connection
				//127.0.0.1 is localhost aka yourself
				RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect("127.0.0.1", SERVER_PORT, nullptr, 0);
				RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
				std::cout << "client attempted connection..waiting for response" << std::endl;
				g_networkState = NS_PendingConnection;
			}
		}

	}

	inputHandler.join();
	packetHandler.join();
	return 0;
}