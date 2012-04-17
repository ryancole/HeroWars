#include "InGameState.h"

#include "StateManager.h"
InGameState::InGameState(StateManager* stateManager, shared_ptr<Party> currentParty, shared_ptr<PacketManager> packetManager) : GameState(stateManager,currentParty, packetManager)
{

}

InGameState::~InGameState()
{

}

void InGameState::initialize()
{
	GameState::initialize();
	Log::getMainInstance()->writeLine("Entering InGameState\n");
	_packetManager->registerIPacketHandler(this);
}

void InGameState::release()
{
	GameState::release();
	Log::getMainInstance()->writeLine("Leaving InGameState\n");
}

void InGameState::update(float alphaTime)
{
	GameState::update(alphaTime);
	//!TODO main loop everything will be done there : IA , towers, players, sbires and even the almighty nashor!!!
}

bool InGameState::processPacket(HANDLE_ARGS)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(packet->data);
	switch(header->cmd)
	{
	case PKT_C2S_Loaded:
		if(channelID == CHL_C2S) return handleInit(peer,packet);
			break;
	case PKT_C2S_ViewReq:
		if(channelID == CHL_C2S) return handleView(peer,packet);
			break;
	case PKT_C2S_AttentionPing:
		if(channelID == CHL_C2S) return handleAttentionPing(peer,packet);
			break;
	case PKT_ChatBoxMessage:
		if(channelID == CHL_COMMUNICATION) return handleChatBoxMessage(peer,packet);
			break;
	case PKT_C2S_MoveReq:
		if(channelID == CHL_C2S) return handleMove(peer,packet);
			break;
	case PKT_C2S_SkillUp:
		if(channelID == CHL_C2S) return handleSkillUp(peer,packet);
			break;
	}
	return false;
	return false;
}

bool InGameState::handleMap(ENetPeer *peer, ENetPacket *packet)
{
	LoadScreenPlayer *playerName = LoadScreenPlayer::create(PKT_S2C_LoadName, peerInfo(peer)->name, peerInfo(peer)->nameLen);
	playerName->userId = peerInfo(peer)->userId;

	LoadScreenPlayer *playerHero = LoadScreenPlayer::create(PKT_S2C_LoadHero,  peerInfo(peer)->type, peerInfo(peer)->typeLen);
	playerHero->userId = peerInfo(peer)->userId;
	playerHero->skinId = 4;

	//Builds team info
	LoadScreenInfo screenInfo;
	screenInfo.bluePlayerNo = 1;
	screenInfo.redPlayerNo = 0;

	screenInfo.bluePlayerIds[0] = peerInfo(peer)->userId;
	
	bool pInfo = _packetManager->sendPacket(peer, reinterpret_cast<uint8*>(&screenInfo), sizeof(LoadScreenInfo), CHL_LOADING_SCREEN);

	//For all players send this info
	bool pName = _packetManager->sendPacket(peer, reinterpret_cast<uint8*>(playerName), playerName->getPacketLength(), CHL_LOADING_SCREEN);
	bool pHero = _packetManager->sendPacket(peer, reinterpret_cast<uint8*>(playerHero), playerHero->getPacketLength(), CHL_LOADING_SCREEN);

	//cleanup
	LoadScreenPlayer::destroy(playerName);
	LoadScreenPlayer::destroy(playerHero);

	return (pInfo && pName && pHero);

}


bool InGameState::handleInit(ENetPeer *peer, ENetPacket *packet)
{
	uint8 resp[] = {0xff, 0x18, 0x06, 0x60, 0x00, 0x00, 0x00, 0x00, 0x24, 0x12, 0xca, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x50, 0xc0, 0x01, 0x00, 0x00, 0x40, 0x00, 0x01, 0x01, 0x00, 0x43, 0xac, 0xd4, 0x0c, 0x00, 0x00,\
		0x00, 0x00, 0xcd, 0x0c, 0xf0, 0x43, 0x01, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00,\
		0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd2, 0x43, 0x02, 0x00, 0x00, 0x40,\
		0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x70, 0x44, 0x03, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x43, 0xac, 0xd4, 0x0c,\
		0x00, 0x00, 0x00, 0x00, 0xcd, 0x0c, 0xf0, 0x43, 0x04, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01,\
		0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd2, 0x43, 0x05, 0x00,\
		0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x70, 0x44, 0x06, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x43, 0xac,\
		0xd4, 0x0c, 0x00, 0x00, 0x00, 0x00, 0xcd, 0x0c, 0xf0, 0x43, 0x07, 0x00, 0x00, 0x40, 0x53, 0x01,\
		0x00, 0x01, 0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd2, 0x43,\
		0x08, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x70, 0x44, 0x09, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00,\
		0x43, 0xac, 0xd4, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x81, 0x0e, 0x45, 0x0a, 0x00, 0x00, 0x40,\
		0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x43, 0xac, 0xd4, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x81,\
		0x0e, 0x45, 0x0b, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x43, 0xac, 0xd4, 0x0c,\
		0x00, 0x00, 0x00, 0x00, 0xcd, 0x0c, 0xf0, 0x43, 0x0c, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01,\
		0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd2, 0x43, 0x0d, 0x00,\
		0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x70, 0x44, 0x0e, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x43, 0xac,\
		0xd4, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x81, 0x0e, 0x45, 0x0f, 0x00, 0x00, 0x40, 0x53, 0x01,\
		0x00, 0x01, 0x01, 0x00, 0x43, 0xac, 0xd4, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x81, 0x0e, 0x45,\
		0x10, 0x00, 0x00, 0x40, 0x53, 0x02, 0x00, 0x01, 0x01, 0x00, 0x43, 0xac, 0xd4, 0x0c, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x50, 0xc3, 0x46, 0x12, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00,\
		0x43, 0xac, 0xd4, 0x0c, 0x00, 0x00, 0x00, 0x00, 0xcd, 0x0c, 0xf0, 0x43, 0x13, 0x00, 0x00, 0x40,\
		0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0xd2, 0x43, 0x14, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x03, 0xa9, 0x21, 0x04,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x44, 0x15, 0x00, 0x00, 0x40, 0x53, 0x01, 0x00, 0x01,\
		0x01, 0x00, 0x43, 0xac, 0xd4, 0x0c, 0x00, 0x00, 0x00, 0x00, 0xcd, 0x0c, 0xf0, 0x43, 0x16, 0x00,\
		0x00, 0x40, 0x53, 0x01, 0x00, 0x01, 0x01, 0x00, 0x03, 0xa9, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0xd2, 0x43, 0x17, 0x00, 0x00, 0x40};

	_packetManager->sendPacket(peer, reinterpret_cast<uint8*>(resp), sizeof(resp), 3);
	//sendPacket(peer, reinterpret_cast<uint8*>(fog), sizeof(fog), 4);
	
	return true;

}

bool InGameState::handleAttentionPing(ENetPeer *peer, ENetPacket *packet)
{
	AttentionPing *ping = reinterpret_cast<AttentionPing*>(packet->data);
	AttentionPingAns response(ping);

	Log::getMainInstance()->writeLine("Plong x: %f, y: %f, z: %f, type: %i\n", ping->x, ping->y, ping->z, ping->type);

	return _packetManager->broadcastPacket(reinterpret_cast<uint8*>(&response), sizeof(AttentionPing), 3);
}

bool InGameState::handleView(ENetPeer *peer, ENetPacket *packet)
{
	ViewReq *request = reinterpret_cast<ViewReq*>(packet->data);

	Log::getMainInstance()->writeLine("View (%i), x:%f, y:%f, zoom: %f\n", request->requestNo, request->x, request->y, request->zoom);

	ViewAns answer;
	answer.requestNo = request->requestNo;
	return _packetManager->sendPacket(peer, reinterpret_cast<uint8*>(&answer), sizeof(ViewAns), 3, UNRELIABLE);
}

bool InGameState::handleMove(ENetPeer *peer, ENetPacket *packet)
{
	MoveReq *request = reinterpret_cast<MoveReq*>(packet->data);

	Log::getMainInstance()->writeLine("Move to: x(left->right):%f, y(height):%f, z(bot->top): %f\n", request->x1, request->y1, request->z1);
	return true;
}

bool InGameState::handleChatBoxMessage(ENetPeer *peer, ENetPacket *packet)
{
	ChatBoxMessage* message = reinterpret_cast<ChatBoxMessage*>(packet->data);

	switch(message->cmd)
	{
	case CMT_ALL:
	//!TODO make a player class and foreach player in game send the message
		return _packetManager->sendPacket(peer,packet->data,packet->dataLength,CHL_COMMUNICATION);
		break;
	case CMT_TEAM:
	//!TODO make a team class and foreach player in the team send the message
		return _packetManager->sendPacket(peer,packet->data,packet->dataLength,CHL_COMMUNICATION);
		break;
	default:
		Log::getMainInstance()->errorLine("Unknow ChatMessageType");
		break;
	}
	return false;
}

bool InGameState::handleSkillUp(ENetPeer *peer, ENetPacket *packet) {

	SkillUpPacket* skillUpPacket = reinterpret_cast<SkillUpPacket*>(packet->data);
	//!TODO Check if can up skill? :)
	SkillUpResponse skillUpResponse;
	
	skillUpResponse.skill = skillUpPacket->skill;
	skillUpResponse.level = 0x0001;

	return _packetManager->sendPacket(peer, reinterpret_cast<uint8*>(&skillUpResponse),sizeof(skillUpResponse),CHL_GAMEPLAY);

}