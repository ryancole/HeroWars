#include "LoadingState.h"

#include "StateManager.h"
#include "InGameState.h"

LoadingState::LoadingState(StateManager* stateManager, shared_ptr<Party> currentParty, shared_ptr<PacketManager> packetManager) 
	: GameState(stateManager,currentParty,packetManager)
{
	_loadingFinished = false;
}

LoadingState::~LoadingState()
{

}

void LoadingState::initialize()
{
	GameState::initialize();
	Log::getMainInstance()->writeLine("Entering LoadingState\n");
	_packetManager->registerIPacketHandler(this); //yeah we want to handle packets
}

void LoadingState::release()
{
	GameState::release();
	Log::getMainInstance()->writeLine("Leaving LoadingState\n");
}

void LoadingState::update(float alphaTime)
{
	GameState::update(alphaTime);
	if(_loadingFinished)
	{
		//if almost everyone logged

	}
	else
	{
		//here we handle someone that try to reconnect

	}
}

bool LoadingState::processPacket(HANDLE_ARGS){
	
	PacketHeader* header = reinterpret_cast<PacketHeader*>(packet->data);
	switch(header->cmd)
	{
	case PKT_KeyCheck:
		if(channelID == CHL_HANDSHAKE) return handleKeyCheck(peer,packet);
			break;
	case PKT_C2S_Ping_Load_Info:
		if(channelID == CHL_C2S) return handleLoadPing(peer,packet);
			break;
	case PKT_C2S_ClientReady:
		if(channelID == CHL_LOADING_SCREEN) return handleMap(peer,packet);
			break;
	case PKT_C2S_SynchVersion:
		if(channelID == CHL_C2S) return handleSynch(peer,packet);
			break;
	case PKT_C2S_GameNumberReq:
		if(channelID == CHL_C2S) return handleGameNumber(peer,packet);
			break;
	case PKT_C2S_QueryStatusReq:
		if(channelID == CHL_C2S) return handleQueryStatus(peer,packet);
			break;
	case PKT_C2S_CharLoaded:
		if(channelID == CHL_C2S) return handleSpawn(peer,packet);
			break;
	}
	return false;
}

bool LoadingState::handleKeyCheck(ENetPeer *peer, ENetPacket *packet)
{
	KeyCheck *keyCheck = (KeyCheck*)packet->data;
	uint64 userId = _packetManager->getBlowFish()->Decrypt(keyCheck->checkId);

	if(userId == keyCheck->userId)
	{
		PDEBUG_LOG_LINE(Log::getMainInstance()," User got the same key as i do, go on!\n");
		peerInfo(peer)->keyChecked = true;
		peerInfo(peer)->userId = userId;
	}
	else
	{
		Log::getMainInstance()->errorLine(" WRONG KEY, GTFO!!!\n");
		return false;
	}

	//Send response as this is correct (OFC DO SOME ID CHECKS HERE!!!)
	KeyCheck response;
	response.userId = keyCheck->userId;
	response.netId = 0;
	
	return _packetManager->sendPacket(peer, reinterpret_cast<uint8*>(&response), sizeof(KeyCheck), CHL_HANDSHAKE);
}

bool LoadingState::handleGameNumber(ENetPeer *peer, ENetPacket *packet)
{
	WorldSendGameNumber world;
	world.gameId = 1;
	memcpy(world.data, peerInfo(peer)->name, peerInfo(peer)->nameLen);

	return _packetManager->sendPacket(peer, reinterpret_cast<uint8*>(&world), sizeof(WorldSendGameNumber), CHL_S2C);
}

bool LoadingState::handleSynch(ENetPeer *peer, ENetPacket *packet)
{
	SynchVersion *version = reinterpret_cast<SynchVersion*>(packet->data);
	Log::getMainInstance()->writeLine("HeroWars version: %s\n", version->version);

	SynchVersionAns answer;
	answer.mapId = 1;
	//exhaust = 0x08A8BAE4, Cleanse = 0x064D2094, flash = 0x06496EA8
	answer.players[0].userId = peerInfo(peer)->userId;
	answer.players[0].skill1 = SPL_Exhaust;
	answer.players[0].skill2 = SPL_Cleanse;

	_packetManager->sendPacket(peer, reinterpret_cast<uint8*>(&answer), sizeof(SynchVersionAns), CHL_S2C);

	return true;
}

bool LoadingState::handleMap(ENetPeer *peer, ENetPacket *packet)
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

bool LoadingState::handleLoadPing(ENetPeer *peer, ENetPacket *packet)
{
	PingLoadInfo *loadInfo = reinterpret_cast<PingLoadInfo*>(packet->data);

	PingLoadInfo response;
	memcpy(&response, packet->data, sizeof(PingLoadInfo));
	response.header.cmd = PKT_S2C_Ping_Load_Info;
	response.userId = peerInfo(peer)->userId;


	Log::getMainInstance()->writeLine("Loading: loaded: %f, ping: %f, %i, %f\n", loadInfo->loaded, loadInfo->ping, loadInfo->unk4, loadInfo->f3);
	if(100.0f - (loadInfo->loaded) < FLT_EPSILON)
	{
		//!TODO here we should check that everyone is logged
		_loadingFinished = true;
	}
	return _packetManager->broadcastPacket(reinterpret_cast<uint8*>(&response), sizeof(PingLoadInfo), 4, UNRELIABLE);
}

bool LoadingState::handleQueryStatus(ENetPeer *peer, ENetPacket *packet)
{
	QueryStatus response;
	return _packetManager->sendPacket(peer, reinterpret_cast<uint8*>(&response), sizeof(QueryStatus), 3);
}

//building the map
bool LoadingState::handleSpawn(ENetPeer *peer, ENetPacket *packet)
{
	//!TODO THESE PACKETS SHOULD BE SEND AT THE BEGGINING OF INGAMESTATE TO EVERY LOGGED PLAYERS AND NOT THERE. I PUT IT HERE TO HELP YOU DEBUG RIGHT NOW.
	_stateManager->pushState(shared_ptr<InGameState>(new InGameState(_stateManager,_currentParty,_packetManager)));

	uint8 bounds[] = {0x2c, 0x19, 0x00, 0x00, 0x40, 0x99, 0x14, 0x00, 0x00, 0x99, 0x14, 0x00, 0x00, 0x7f, 0x14, 0x00,\
		0x00, 0x7f, 0x14, 0x00, 0x00, 0x7f, 0x14, 0x00, 0x00, 0x7f, 0x14, 0x00, 0x00, 0x7f, 0x14, 0x00,\
		0x00, 0xe1, 0x14, 0x00, 0x00, 0xe1, 0x14, 0x00, 0x00, 0xe1, 0x14, 0x00, 0x00, 0x99, 0x14, 0x00,\
		0x00, 0x99, 0x14, 0x00, 0x00, 0xa9, 0x14, 0x00, 0x00, 0xa9, 0x14, 0x00, 0x00, 0xa9, 0x14, 0x00,\
		0x00, 0xa9, 0x14, 0x00, 0x00, 0xa9, 0x14, 0x00, 0x00, 0xc5, 0x14, 0x00, 0x00, 0xc5, 0x14, 0x00,\
		0x00, 0xc5, 0x14, 0x00, 0x00, 0xc5, 0x14, 0x00, 0x00, 0xa9, 0x14, 0x00, 0x00, 0xc5, 0x14, 0x00,\
		0x00, 0xa9, 0x14, 0x00, 0x00, 0xc5, 0x14, 0x00, 0x00, 0xa9, 0x14, 0x00, 0x00, 0xc5, 0x14, 0x00,\
		0x00, 0xa9, 0x14, 0x00, 0x00, 0xc5, 0x14, 0x00, 0x00, 0xc5, 0x14, 0x00, 0x00, 0x95, 0x86, 0x5e,\
		0x06, 0xa8, 0x6e, 0x49, 0x06, 0x64, 0x37, 0x00, 0x00, 0x01, 0x44, 0x36, 0x00, 0x00, 0x02, 0x41,\
		0x36, 0x00, 0x00, 0x01, 0x52, 0x37, 0x00, 0x00, 0x04, 0x71, 0x36, 0x00, 0x00, 0x01, 0x43, 0x36,\
		0x00, 0x00, 0x03, 0x42, 0x36, 0x00, 0x00, 0x03, 0x41, 0x37, 0x00, 0x00, 0x01, 0x64, 0x36, 0x00,\
		0x00, 0x01, 0x92, 0x36, 0x00, 0x00, 0x01, 0x52, 0x36, 0x00, 0x00, 0x04, 0x43, 0x37, 0x00, 0x00,\
		0x03, 0x61, 0x36, 0x00, 0x00, 0x01, 0x82, 0x36, 0x00, 0x00, 0x03, 0x62, 0x36, 0x00, 0x00, 0x01,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x1e};

	uint8 turrets1[] = {0xff, 0x06, 0x4f, 0xa6, 0x01, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72,\
		0x72, 0x65, 0x74, 0x5f, 0x54, 0x31, 0x5f, 0x52, 0x5f, 0x30, 0x33, 0x5f, 0x41, 0x00, 0x74, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5,\
		0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04,\
		0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00,\
		0x80, 0x01, 0xff, 0x01, 0x4a, 0x02, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74,\
		0x5f, 0x54, 0x31, 0x5f, 0x52, 0x5f, 0x30, 0x32, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76,\
		0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00,\
		0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff,\
		0x01, 0x4a, 0x03, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x31,\
		0x5f, 0x43, 0x5f, 0x30, 0x37, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59,\
		0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00,\
		0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x04,\
		0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x52, 0x5f,\
		0x30, 0x33, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02,\
		0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9,\
		0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8,\
		0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x05, 0x00, 0x00, 0x40,\
		0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x52, 0x5f, 0x30, 0x32, 0x5f,\
		0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03,\
		0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2,\
		0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38,\
		0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x06, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75,\
		0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x52, 0x5f, 0x30, 0x31, 0x5f, 0x41, 0x00, 0x74,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00,\
		0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74,\
		0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00,\
		0x00, 0x80, 0x01, 0x12, 0x00, 0x00, 0x00, 0x00};

	uint8 turrets2[] = {0xff, 0x06, 0x4f, 0xa6, 0x07, 0x00, 0x00, 0x40, 0x07, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72,\
		0x72, 0x65, 0x74, 0x5f, 0x54, 0x31, 0x5f, 0x43, 0x5f, 0x30, 0x35, 0x5f, 0x41, 0x00, 0x74, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5,\
		0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04,\
		0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00,\
		0x80, 0x01, 0xff, 0x01, 0x4a, 0x08, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74,\
		0x5f, 0x54, 0x31, 0x5f, 0x43, 0x5f, 0x30, 0x34, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76,\
		0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00,\
		0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff,\
		0x01, 0x4a, 0x09, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x31,\
		0x5f, 0x43, 0x5f, 0x30, 0x33, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59,\
		0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00,\
		0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x0a,\
		0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x31, 0x5f, 0x43, 0x5f,\
		0x30, 0x31, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02,\
		0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9,\
		0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8,\
		0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x0b, 0x00, 0x00, 0x40,\
		0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x31, 0x5f, 0x43, 0x5f, 0x30, 0x32, 0x5f,\
		0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03,\
		0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2,\
		0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38,\
		0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x0c, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75,\
		0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x43, 0x5f, 0x30, 0x35, 0x5f, 0x41, 0x00, 0x74,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00,\
		0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74,\
		0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00,\
		0x00, 0x80, 0x01};

	uint8 turrets3[] = {0xff, 0x06, 0x4f, 0xa6, 0x0d, 0x00, 0x00, 0x40, 0x0d, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72,\
		0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x43, 0x5f, 0x30, 0x34, 0x5f, 0x41, 0x00, 0x74, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5,\
		0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04,\
		0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00,\
		0x80, 0x01, 0xff, 0x01, 0x4a, 0x0e, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74,\
		0x5f, 0x54, 0x32, 0x5f, 0x43, 0x5f, 0x30, 0x33, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76,\
		0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00,\
		0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff,\
		0x01, 0x4a, 0x0f, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32,\
		0x5f, 0x43, 0x5f, 0x30, 0x31, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0xa0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59,\
		0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00,\
		0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x10,\
		0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x43, 0x5f,\
		0x30, 0x32, 0x5f, 0x41, 0x00, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x02,\
		0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9,\
		0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8,\
		0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x11, 0x00, 0x00, 0x40,\
		0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x4f, 0x72, 0x64, 0x65, 0x72, 0x54, 0x75, 0x72,\
		0x72, 0x65, 0x74, 0x53, 0x68, 0x72, 0x69, 0x6e, 0x65, 0x5f, 0x41, 0x00, 0x02, 0x00, 0x00, 0x03,\
		0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2,\
		0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38,\
		0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x12, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75,\
		0x72, 0x72, 0x65, 0x74, 0x5f, 0x43, 0x68, 0x61, 0x6f, 0x73, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74,\
		0x53, 0x68, 0x72, 0x69, 0x6e, 0x65, 0x5f, 0x41, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00,\
		0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74,\
		0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00,\
		0x00, 0x80, 0x01};

uint8 turrets4[] = {0xff, 0x07, 0x4f, 0xa6, 0x13, 0x00, 0x00, 0x40, 0x13, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72,\
		0x72, 0x65, 0x74, 0x5f, 0x54, 0x31, 0x5f, 0x4c, 0x5f, 0x30, 0x33, 0x5f, 0x41, 0x00, 0x74, 0x53,\
		0x68, 0x72, 0x69, 0x6e, 0x65, 0x5f, 0x41, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5,\
		0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04,\
		0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00,\
		0x80, 0x01, 0xff, 0x01, 0x4a, 0x14, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74,\
		0x5f, 0x54, 0x31, 0x5f, 0x4c, 0x5f, 0x30, 0x32, 0x5f, 0x41, 0x00, 0x74, 0x53, 0x68, 0x72, 0x69,\
		0x6e, 0x65, 0x5f, 0x41, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76,\
		0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00,\
		0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff,\
		0x01, 0x4a, 0x15, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x31,\
		0x5f, 0x43, 0x5f, 0x30, 0x36, 0x5f, 0x41, 0x00, 0x74, 0x53, 0x68, 0x72, 0x69, 0x6e, 0x65, 0x5f,\
		0x41, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59,\
		0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00,\
		0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x16,\
		0x00, 0x00, 0x40, 0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x4c, 0x5f,\
		0x30, 0x33, 0x5f, 0x41, 0x00, 0x74, 0x53, 0x68, 0x72, 0x69, 0x6e, 0x65, 0x5f, 0x41, 0x00, 0x02,\
		0x00, 0x00, 0x03, 0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9,\
		0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8,\
		0x18, 0x00, 0x38, 0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x17, 0x00, 0x00, 0x40,\
		0x40, 0x54, 0x75, 0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x4c, 0x5f, 0x30, 0x32, 0x5f,\
		0x41, 0x00, 0x74, 0x53, 0x68, 0x72, 0x69, 0x6e, 0x65, 0x5f, 0x41, 0x00, 0x02, 0x00, 0x00, 0x03,\
		0x00, 0x1f, 0x00, 0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2,\
		0xb9, 0xa8, 0x74, 0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38,\
		0x14, 0x68, 0x00, 0x00, 0x80, 0x01, 0xff, 0x01, 0x4a, 0x18, 0x00, 0x00, 0x40, 0x40, 0x54, 0x75,\
		0x72, 0x72, 0x65, 0x74, 0x5f, 0x54, 0x32, 0x5f, 0x4c, 0x5f, 0x30, 0x31, 0x5f, 0x41, 0x00, 0x74,\
		0x53, 0x68, 0x72, 0x69, 0x6e, 0x65, 0x5f, 0x41, 0x00, 0x02, 0x00, 0x00, 0x03, 0x00, 0x1f, 0x00,\
		0xc5, 0x16, 0x68, 0x76, 0xdd, 0x46, 0x59, 0x5d, 0xe2, 0xf9, 0x33, 0x77, 0xf2, 0xb9, 0xa8, 0x74,\
		0x04, 0x01, 0x00, 0x00, 0x04, 0x01, 0x00, 0x00, 0xe8, 0xf8, 0x18, 0x00, 0x38, 0x14, 0x68, 0x00,\
		0x00, 0x80, 0x01, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00};

	HeroSpawnPacket *heroSpawn = HeroSpawnPacket::create(PKT_HeroSpawn,  peerInfo(peer)->name, peerInfo(peer)->nameLen, peerInfo(peer)->type, peerInfo(peer)->typeLen);

	_packetManager->sendPacket(peer, reinterpret_cast<uint8*>(heroSpawn), heroSpawn->getPacketLength(), 3);
	_packetManager->sendPacket(peer, reinterpret_cast<uint8*>(bounds), sizeof(bounds), 3);
	_packetManager->sendPacket(peer, reinterpret_cast<uint8*>(turrets1), sizeof(turrets1), 3);
	_packetManager->sendPacket(peer, reinterpret_cast<uint8*>(turrets2), sizeof(turrets2), 3);
	_packetManager->sendPacket(peer, reinterpret_cast<uint8*>(turrets3), sizeof(turrets3), 3);
	_packetManager->sendPacket(peer, reinterpret_cast<uint8*>(turrets4), sizeof(turrets4), 3);

	
	return true;
}