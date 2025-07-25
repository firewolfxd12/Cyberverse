#include "GameServer.h"

#include <cstdio>

// TODO: This is only for debugging purposes, perspectively we will call into the DLL with a C ABI
int main()
{
    printf("Starting Cyberverse Server 0.0.1 (c) 2023 MeFisto94\n");

    if (!GameServer::Initialize())
    {
        printf("Error when initializing server!\n");
        return 1;
    }

    const auto gameserver = new GameServer();
    gameserver->ListenOn(7777);
    gameserver->RunBlocking();
    GameServer::Destroy();
}