#define NOMINMAX
// API Abstraction
#include "PrimeEngine/APIAbstraction/APIAbstractionDefines.h"

#include "ServerNetworkManager.h"

// Outer-Engine includes

// Inter-Engine includes

#include "PrimeEngine/Lua/LuaEnvironment.h"

// additional lua includes needed
extern "C"
{
#include "PrimeEngine/../luasocket_dist/src/socket.h"
#include "PrimeEngine/../luasocket_dist/src/inet.h"
};

#include "PrimeEngine/../../GlobalConfig/GlobalConfig.h"

#include "PrimeEngine/GameObjectModel/GameObjectManager.h"
#include "PrimeEngine/Events/StandardEvents.h"
#include "PrimeEngine/Scene/DebugRenderer.h"

#include "PrimeEngine/Networking/StreamManager.h"
#include "PrimeEngine/Networking/EventManager.h"

#include <string>
#include <sstream>

// Sibling/Children includes
#include "ServerConnectionManager.h"

using namespace PE::Events;

namespace PE {

namespace Components {

PE_IMPLEMENT_CLASS1(ServerNetworkManager, NetworkManager);

ServerNetworkManager::ServerNetworkManager(PE::GameContext &context, PE::MemoryArena arena, Handle hMyself)
: NetworkManager(context, arena, hMyself)
, m_clientConnections(context, arena, PE_SERVER_MAX_CONNECTIONS)
{
	m_state = ServerState_Uninitialized;
}

ServerNetworkManager::~ServerNetworkManager()
{
	if (m_state != ServerState_Uninitialized)
		socket_destroy(&m_sock);
}

void ServerNetworkManager::addDefaultComponents()
{
	NetworkManager::addDefaultComponents();

	// no need to register handler as parent class already has this method registered
	// PE_REGISTER_EVENT_HANDLER(Events::Event_UPDATE, ServerConnectionManager::do_UPDATE);
}

void ServerNetworkManager::initNetwork()
{
	NetworkManager::initNetwork();

	serverOpenUDPSocket();
}

void ServerNetworkManager::serverOpenUDPSocket()
{
	bool created = false;
	int numTries = 0;
	int port = PE_SERVER_PORT;

	//UDP socket
	while (!created)
	{
		const char* err = /*luasocket::*/inet_trycreate(&m_sock, SOCK_DGRAM);
		if (err)
		{
			assert(!"error creating socket occurred");
			break;
		}

		const char* tryBind = inet_trybind(&m_sock, "0.0.0.0", (unsigned short)(port)); // leaves socket non-blocking
		numTries++;
		if (tryBind)
		{
			if (numTries >= 10)
				break; // give up
			port++;
		}
		else
		{
			created = true;

			m_state = ServerState_ConnectionListening;
			m_serverPort = port;
			//getSocketAddress();
			break;
		}
	}
}




void ServerNetworkManager::createNetworkConnectionContext(t_socket sock,  int clientId, PE::NetworkContext *pNetContext)
{
	
	pNetContext->m_clientId = clientId;

	NetworkManager::createNetworkConnectionContext(sock, pNetContext);

	{
		pNetContext->m_pConnectionManager = new (m_arena) ServerConnectionManager(*m_pContext, m_arena, *pNetContext, Handle());
		pNetContext->getConnectionManager()->addDefaultComponents();
	}

	{
		pNetContext->m_pStreamManager = new (m_arena) StreamManager(*m_pContext, m_arena, *pNetContext, Handle());
		pNetContext->getStreamManager()->addDefaultComponents();
	}

	{
		pNetContext->m_pEventManager = new (m_arena) EventManager(*m_pContext, m_arena, *pNetContext, Handle());
		pNetContext->getEventManager()->addDefaultComponents();
	}

	pNetContext->getConnectionManager()->initializeConnected(sock);

	addComponent(pNetContext->getConnectionManager()->getHandle());
	addComponent(pNetContext->getStreamManager()->getHandle());
}



void ServerNetworkManager::do_UPDATE(Events::Event *pEvt)
{
	NetworkManager::do_UPDATE(pEvt);

	t_timeout timeoutRecv;
	timeoutRecv.block = PE_SOCKET_RECEIVE_TIMEOUT;
	timeoutRecv.total = -1.0;
	timeoutRecv.start = 0;

	size_t bytesRecv;
	sockaddr_in messageOrigin;
	socklen_t len = sizeof(messageOrigin);

	char buff[1024];
	int err2 = socket_recvfrom(&m_sock, buff, 1024, &bytesRecv, (SA*)&messageOrigin, &len, &timeoutRecv);

	assert(bytesRecv <= 1024);
	if (err2 == 0 ) {

		struct sockaddr_in* ipv4 = (struct sockaddr_in*)&messageOrigin;
		char originAddress[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(ipv4->sin_addr), originAddress, INET_ADDRSTRLEN);
		int originPort = ntohs(ipv4->sin_port);

		t_socket serverSock;
		const char* err3 = inet_trycreate(&serverSock, SOCK_DGRAM);
		const char* err4 = inet_trybind(&serverSock, "127.0.0.1", 0);
		if (err3 == 0 && err4 == 0) {


			t_timeout timeout;
			timeout.block = 0;
			timeout.total = -1.0;
			timeout.start = 0;

			struct sockaddr_in addr;
			socklen_t len = sizeof(addr);

			if (getsockname(serverSock, (struct sockaddr*)&addr, &len) == -1) {
				perror("getsockname");
				PEINFO("errno: %d\n", errno);
				return;
			}

			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);


			const char* sockErr = inet_tryconnect(&serverSock, originAddress, originPort, &timeout);
			if (sockErr == 0) {
			
				std::string clientConnectionMessage = "IP: " + std::string(ip) + " PORT: " + std::to_string(ntohs(addr.sin_port))+"\0";
				clientConnectionMessage.push_back('\0');

		
				char sendBuff[512];
				if (clientConnectionMessage.length() < 511) {
					strcpy(sendBuff, clientConnectionMessage.c_str());
				}
				else {
					PEINFO("Buffer overflow");
				}

				size_t step;
				int send = socket_sendto(&m_sock, sendBuff,	512, &step, (SA*)&messageOrigin, len, &timeoutRecv);
				if (send == 0) {
					
					PEINFO("SENT ACK");
					m_connectionsMutex.lock();
					m_clientConnections.add(NetworkContext());
					int clientIndex = m_clientConnections.m_size-1;
					std::string cliData = "Client " + std::to_string(clientIndex) + ": " + originAddress + " : " + std::to_string(originPort) + "\0";
					m_clientData.push_back(cliData);
					NetworkContext& netContext = m_clientConnections[clientIndex];

					createNetworkConnectionContext(serverSock, clientIndex, &netContext);
					m_connectionsMutex.unlock();

					PE::Events::Event_SERVER_CLIENT_CONNECTION_ACK evt(*m_pContext);
					evt.m_clientId = clientIndex;
					netContext.getEventManager()->scheduleEvent(&evt, m_pContext->getGameObjectManager(), true);
				}
			}
			else {
				PEINFO("FAILED TO CONNECT");
			}
		}
	}
	//else {
	//	//PEINFO("No conn req");
	//}

}

void ServerNetworkManager::debugRender(int &threadOwnershipMask, float xoffset /* = 0*/, float yoffset /* = 0*/)
{
	sprintf(PEString::s_buf, "Server: Port %d %d Connections", m_serverPort, m_clientConnections.m_size);
	DebugRenderer::Instance()->createTextMesh(
		PEString::s_buf, true, false, false, false, 0,
		Vector3(xoffset, yoffset, 0), 1.0f, threadOwnershipMask);
	//char clientData = ;
	float dy = 0.025f;
	float dx = 0.01;
	float evtManagerDy = 0.15f;
	// debug render all networking contexts
	m_connectionsMutex.lock();

	if (m_clientConnections.m_size == 1) {
		sprintf(PEString::s_buf,"Please wait for the other client to connect.");
		DebugRenderer::Instance()->createTextMesh(
			PEString::s_buf, true, false, false, false, 0,
			Vector3(0.3, 0.5, 0), 1.0f, threadOwnershipMask);

	}

	for (unsigned int i = 0; i < m_clientConnections.m_size; ++i)
	{
		sprintf(PEString::s_buf, "Connection[%d]:", i);
	
		DebugRenderer::Instance()->createTextMesh(
		PEString::s_buf, true, false, false, false, 0,
		Vector3(xoffset, yoffset + dy + evtManagerDy * i, 0), 1.0f, threadOwnershipMask);

		/*sprintf(PEString::s_buf, m_clientData[i].c_str());
		DebugRenderer::Instance()->createTextMesh(
			PEString::s_buf, true, false, false, false, 0,
			Vector3(xoffset, 1- (yoffset + dy + evtManagerDy * i), 0), 1.0f, threadOwnershipMask);*/

		NetworkContext &netContext = m_clientConnections[i];
		netContext.getEventManager()->debugRender(threadOwnershipMask, xoffset + dx, yoffset + dy * 2.0f + evtManagerDy * i);
	}
	m_connectionsMutex.unlock();
}

void ServerNetworkManager::scheduleEventToAllExcept(PE::Networkable *pNetworkable, PE::Networkable *pNetworkableTarget, int exceptClient)
{
	for (unsigned int i = 0; i < m_clientConnections.m_size; ++i)
	{
		if ((int)(i) == exceptClient)
			continue;

		NetworkContext &netContext = m_clientConnections[i];
		netContext.getEventManager()->scheduleEvent(pNetworkable, pNetworkableTarget, true);
	}
}



void ConnectionManager::SetLuaFunctions(PE::Components::LuaEnvironment *pLuaEnv, lua_State *luaVM)
{
	

	
	lua_register(luaVM, "l_clientConnectToUDPServer", l_clientConnectToUDPServer);


	// run a script to add additional functionality to Lua side of Skin
	// that is accessible from Lua
// #if APIABSTRACTION_IOS
// 	LuaEnvironment::Instance()->runScriptWorkspacePath("Code/PrimeEngine/Scene/Skin.lua");
// #else
// 	LuaEnvironment::Instance()->runScriptWorkspacePath("Code\\PrimeEngine\\Scene\\Skin.lua");
// #endif

}

int ConnectionManager::l_clientConnectToUDPServer(lua_State *luaVM)
{
	lua_Number lPort = lua_tonumber(luaVM, -1);
	int port = (int)(lPort);

	const char *strAddr = lua_tostring(luaVM, -2);

	GameContext *pContext = (GameContext *)(lua_touserdata(luaVM, -3));

	lua_pop(luaVM, 3);

	pContext->getConnectionManager()->clientConnectToUDPServer(strAddr, port);

	return 0; // no return values
}


}; // namespace Components
}; // namespace PE
