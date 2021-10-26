#ifndef SERVER_H
#define SERVER_H

#define _POSIX_SOURCE

#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/prctl.h>

#include "ipc.h"
#include "kbjoy.h"
#include "input.h"
#include "config.h"

void ServerInit(struct kbjoy *kbjoyStruct);

void DisconnectMessageOutSocket();

void ServerMain(struct kbjoy *kbjoyStruct);

int ConnectCommand(struct kbjoy *kbjoyStruct, struct sockaddr_un *from, socklen_t fromlen);

int RemoveDeadChilds(struct kbjoy *kbjoyStruct);

void signal_handler(int sig);

int AddCommand(struct kbjoy *kbjoyStruct, char buff[]);

int DelCommand(struct kbjoy *kbjoyStruct, char buff[]);

int GetPlayerNumberFromASCII(char ASCIInumber);

void GetControllerName(char ctrlName[15], char ASCIIplayerNumber);

int StartCommand(struct kbjoy *kbjoyStruct, char buff[]);

int StopCommand(struct kbjoy *kbjoyStruct, char buff[]);

int StopMapping(struct kbjoy *kbjoyStruct, int playerNumber);

int GetPath(char *dest, char *src);

void InfoCommand(struct kbjoy *kbjoyStruct);

int ListCommand(struct kbjoy *kbjoyStruct);

int TriggerCommand(struct kbjoy *kbjoyStruct, char buff[]);

#endif