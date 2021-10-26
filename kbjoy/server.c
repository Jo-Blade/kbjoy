/* This file contains all function about the main server process
## this process will receive commands and data of all other
## process.
## It will create the child process to remap keyboard as joystick
*/

#include "server.h"

volatile int signalNumber = 0;

/* If the out socket is opened by a process send the disconnect message */
void DisconnectMessageOutSocket()
{
	int fd;
	struct sockaddr_un addr;

	if ((fd = SocketFDInit()) < 0)
		exit(EXIT_FAILURE);

	SockaddrSet(&addr, OUT_SOCK_FILE);

	if (SendToUnconnected(fd, "disconnect", &addr, sizeof(addr)) < 0)
		exit(EXIT_FAILURE);

	syslog(LOG_INFO, "Retry in 5s");
	sleep(5);
}

/* Init and lock sockets */
void ServerInit(struct kbjoy *kbjoyStruct)
{
	int ret;
	FILE *pidfile = NULL;
	int i;

	syslog(LOG_INFO, "test lock...\n");

	/* test if file is locked */
	if ((ret = TestFileLock(PROCESS_LOCK, NULL)) != 0)
	{
		if (ret == LOCKED)
			syslog(LOG_CRIT, "Kbjoy is already running or delete %s file in %s", PROCESS_LOCK, WORKDIR);

		exit(EXIT_FAILURE);
	}

	syslog(LOG_INFO, "test lock good\n");

	/* lock the file */
	/* ret get the fd of the locked file,
	## we dont save it because the lock will be release
	## automatically when the process will be stopped */
	if ((ret = LockFile(PROCESS_LOCK)) < 0)
		exit(EXIT_FAILURE);

	/* Write pid of child process in file */
	pidfile = fopen("pid", "w+");
	if (pidfile == NULL)
	{
		syslog(LOG_CRIT, "Can't create Pid File: %d (%s)\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	fprintf(pidfile, "%d", getpid());
	fclose(pidfile);

	/* create and bind command socket */
	if ((kbjoyStruct->fdInRead = SockCreate(SERVER_SOCK_FILE)) < 0)
		exit(EXIT_FAILURE);

	/* test if another process is reading the out socket */
	for (i = 1; i < 10; i++)
	{
		ret = TestFileLock(OUT_SOCK_LOCK, NULL);

		switch (ret)
		{
		case UNLOCKED:
			syslog(LOG_INFO, "test lock good: %s", OUT_SOCK_LOCK);

			/* Quit the loop */
			i = 100;
			break;

		case LOCKED:
			syslog(LOG_CRIT, "Can't acquire the lock: %s ; test %d of 5", OUT_SOCK_LOCK, i);

			if (i == 5)
			{
				syslog(LOG_CRIT, "Another process is reading the out socket, you should stop it and restart or delete the file %s at the location %s", OUT_SOCK_LOCK, WORKDIR);
				exit(EXIT_FAILURE);
			}

			DisconnectMessageOutSocket();
			break;

		default:
			exit(EXIT_FAILURE);
			break;
		}
	}

	/* create and bind the out socket */
	if ((kbjoyStruct->fdOutRead = SockCreate(OUT_SOCK_FILE)) < 0)
		exit(EXIT_FAILURE);

	/* connect to the out socket */
	if ((kbjoyStruct->fdOutSend = SockConnect(OUT_SOCK_FILE)) < 0)
		exit(EXIT_FAILURE);
}

/* Infinite loop getting data from all process
## Create the child process when requested */
void ServerMain(struct kbjoy *kbjoyStruct)
{
	int ret;
	int i;
	char buff[BUFF_LEN];

	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);

	/* may cause bug if not initialized */
	memset(&from, 0, fromlen);
	from.sun_family = AF_UNIX;

	/* set to 0 all players/joysticks */
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		kbjoyStruct->allPlayers[i] = 0;
		kbjoyStruct->allJoysticks[i] = 0;
	}
	kbjoyStruct->triggerProcess = 0;

	for (;;)
	{
		/* remove dead process from players list */
		if (RemoveDeadChilds(kbjoyStruct) < 0)
			exit(EXIT_FAILURE);

		/* make interruption if a child process die */
		signal(SIGCHLD, signal_handler);

		/* wait until a command is received, may be a separate function */
		if ((ret = recvfrom(kbjoyStruct->fdInRead, buff, BUFF_LEN, 0, (struct sockaddr *)&from, &fromlen)) < 0)
		{
			/* if there is a signal interruption */
			if (signalNumber == SIGCHLD)
			{
				syslog(LOG_INFO, "recvfrom failed: SIGCHLD interruption\n");
				signalNumber = 0;
				continue;
			}

			syslog(LOG_CRIT, "recvfrom failed: %d (%s)\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		/* remove sigchld_handler if no signal was received */
		signal(SIGCHLD, SIG_DFL);

		syslog(LOG_INFO, "recvfrom %d characters: '%s'\n", ret, buff);

		if (!strcmp(buff, "ping"))
		{
			/* ping command -> send response to the sender */
			if (SendToUnconnected(kbjoyStruct->fdInRead, "ping success", &from, fromlen) < 0)
				syslog(LOG_CRIT, "failure to send response\n");
		}
		else if (!strcmp(buff, "connect"))
		{
			/* the client ask for getting the fdOutRead in order to become the output */
			syslog(LOG_INFO, "received: connect command\n");

			if (ConnectCommand(kbjoyStruct, &from, fromlen) < 0)
				syslog(LOG_CRIT, "failure to connect client as output\n");
		}
		else if (!strncmp(buff, "add ", 4))
		{
			/* the client ask for adding a new virtual joystick */
			if (AddCommand(kbjoyStruct, buff) < 0)
			{
				syslog(LOG_CRIT, "failure to add a new virtual joystick\n");

				if (send(kbjoyStruct->fdOutSend, "no gamepad added", 17, 0) < 0)
					syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "no gamepad added", errno, strerror(errno));
			}
		}
		else if (!strncmp(buff, "del ", 4))
		{
			/* the client ask for adding a new virtual joystick */
			if (DelCommand(kbjoyStruct, buff) < 0)
			{
				syslog(LOG_CRIT, "failure to remove the virtual joystick\n");

				if (send(kbjoyStruct->fdOutSend, "no gamepad removed", 19, 0) < 0)
					syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "no gamepad removed", errno, strerror(errno));
			}
		}
		else if (!strncmp(buff, "start ", 6))
		{
			/* the client ask for start remapping a keyboard */
			if (StartCommand(kbjoyStruct, buff) < 0)
			{
				syslog(LOG_CRIT, "failure to start mapping\n");

				if (send(kbjoyStruct->fdOutSend, "mapping failed", 15, 0) < 0)
					syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "mapping failed", errno, strerror(errno));
			}
		}
		else if (!strncmp(buff, "stop ", 5))
		{
			/* the client ask for stop remapping a keyboard */
			if (StopCommand(kbjoyStruct, buff) < 0)
			{
				syslog(LOG_CRIT, "failure to stop mapping\n");

				if (send(kbjoyStruct->fdOutSend, "cannot stop mapping", 20, 0) < 0)
					syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "cannot stop mapping", errno, strerror(errno));
			}
		}
		else if (!strcmp(buff, "info"))
		{
			/* the client ask for getting info about connected devices */
			InfoCommand(kbjoyStruct);
		}
		else if (!strcmp(buff, "list"))
		{
			/* the client ask for getting info about connected devices */
			if (ListCommand(kbjoyStruct) < 0)
			{
				syslog(LOG_CRIT, "failure to list keyboards\n");

				if (send(kbjoyStruct->fdOutSend, "cannot list keyboards", 22, 0) < 0)
					syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "cannot list keyboards", errno, strerror(errno));
			}
		}
		else if (!strncmp(buff, "trigger ", 8))
		{
			if (TriggerCommand(kbjoyStruct, buff) < 0)
			{
				syslog(LOG_CRIT, "failure to execute trigger command\n");

				if (send(kbjoyStruct->fdOutSend, "trigger command failed", 23, 0) < 0)
					syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "trigger command failed", errno, strerror(errno));
			}
		}
		else if (!strcmp(buff, "exit"))
		{
			/* the client ask for exit the daemon */
			syslog(LOG_INFO, "exit the daemon\n");

			if (send(kbjoyStruct->fdOutSend, "kbjoy exited", 13, 0) < 0)
				syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "kbjoy exited", errno, strerror(errno));

			exit(EXIT_SUCCESS);
		}
	}
}

/* Fonction Called when "connect" command is received:
## the client ask for getting the fdOutRead in order to become the output */
int ConnectCommand(struct kbjoy *kbjoyStruct, struct sockaddr_un *from, socklen_t fromlen)
{
	/* counter of client connection, 
	## used to ignore old messages when connected */
	static char connectID = 0;

	int ret;
	char buff[BUFF_LEN];

	if ((ret = TestFileLock(OUT_SOCK_LOCK, NULL)) != UNLOCKED)
	{
		if (ret == LOCKED)
			syslog(LOG_CRIT, "connect command: Another process is reading the out socket, you should stop it before retry or delete the file %s at the location %s", OUT_SOCK_LOCK, WORKDIR);

		/* send "rejection" to the sender */
		strcpy(buff, "rejection");
		SendToUnconnected(kbjoyStruct->fdInRead, buff, from, fromlen);

		return -1;
	}
	syslog(LOG_INFO, "connect state: testlock success");

	/* next connection number */
	connectID++;

	/* send "accept:" + connectID to the sender */
	strcpy(buff, "accept:X");
	buff[7] = connectID;

	if (SendToUnconnected(kbjoyStruct->fdInRead, buff, from, fromlen) < 0)
		return -1;

	syslog(LOG_INFO, "connect state: response sent -> '%s' ; ID:%d", buff, (int)buff[7]);

	/* send fd used to read the out socket
	## I don't understand why I have to pass "fdInRead" in first Argument */
	if (PassFD(kbjoyStruct->fdInRead, kbjoyStruct->fdOutRead, from, fromlen) < 0)
		return -1;

	/* send message to begin the communication
	## older messages should be ignored by the client 
	## the client may close the connection if this message is not received */
	strcpy(buff, "begin:X");
	buff[6] = connectID;

	if (send(kbjoyStruct->fdOutSend, buff, strlen(buff) + 1, 0) < 0)
	{
		syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));
		return -1;
	}

	syslog(LOG_INFO, "connect state: begin message sent -> '%s'", buff);
	return 0;
}

/* Remove Dead process from players list */
int RemoveDeadChilds(struct kbjoy *kbjoyStruct)
{
	int pid;
	int status;
	int i;

	/* get all stopped child pid */
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		syslog(LOG_INFO, "process with pid: %d is dead\n", pid);

		if (kbjoyStruct->triggerProcess == pid)
		{
			if (send(kbjoyStruct->fdOutSend, "trigger process exited", 23, 0) < 0)
				syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "trigger process exited", errno, strerror(errno));

			kbjoyStruct->triggerProcess = 0;
			continue;
		}

		for (i = 0; i < MAX_PLAYERS; i++)
		{
			if (kbjoyStruct->allPlayers[i] == pid)
			{
				char message[32];

				kbjoyStruct->allPlayers[i] = 0;
				syslog(LOG_INFO, "player %d was removed\n", i + 1);

				/* send message */
				GetControllerName(message, i + 49);
				strcat(message, " parsing stopped");

				if (send(kbjoyStruct->fdOutSend, message, 32, 0) < 0)
					syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", message, errno, strerror(errno));

				if (ControllerReset(kbjoyStruct->allJoysticks[i]) < 0)
					syslog(LOG_CRIT, "controller reset failed");

				/* if the process ask to delete its virtual controller */
				if (WEXITSTATUS(status) == 10)
				{
					syslog(LOG_INFO, "delete virtual controller for player %d\n", i + 1);
					/* remove the virtual gamepad */
					if (DeleteVirtualJoy(kbjoyStruct->allJoysticks[i]) < 0)
						return -1;
					kbjoyStruct->allJoysticks[i] = 0;

					/* send message */
					GetControllerName(message, i + 49);
					strcat(message, " removed");

					if (send(kbjoyStruct->fdOutSend, message, 32, 0) < 0)
						syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", message, errno, strerror(errno));
				}

				break;
			}
			else if (i == MAX_PLAYERS - 1)
			{
				syslog(LOG_CRIT, "no player was found with pid: %d \n", pid);
				return -1;
			}
		}
	}
	return 0;
}

/* run when sigchld signal is received */
void signal_handler(int sig)
{
	switch (sig)
	{
	case SIGCHLD:
		signalNumber = SIGCHLD;
		break;

	default:
		break;
	}
}

/* this function is executed when the command "add [gamepad number]" is received.
## It create a new virtual contoller who appears connected in games */
int AddCommand(struct kbjoy *kbjoyStruct, char buff[])
{
	int playerNumber;
	char ctrlName[15];
	char message[22];

	/* convert ASCII number to player number */
	if ((playerNumber = GetPlayerNumberFromASCII(buff[4])) < 0)
		return -1;

	syslog(LOG_INFO, "received: add command for the player %d \n", playerNumber);

	/* verify that the device not exist
	## don't forget to remove 1 because an array begins at the index 0 */
	if (kbjoyStruct->allJoysticks[playerNumber - 1] != 0)
	{
		syslog(LOG_CRIT, "error: the virtual controller number %d already exist\n", playerNumber);
		return -1;
	}

	/* Name of Gamepad */
	GetControllerName(ctrlName, buff[4]);

	/* create and assign a new virtual gamepad */
	if ((kbjoyStruct->allJoysticks[playerNumber - 1] = NewVirtualJoy(ctrlName)) < 0)
	{
		syslog(LOG_CRIT, "failure to create new virtual device: %d (%s)\n", errno, strerror(errno));
		kbjoyStruct->allJoysticks[playerNumber - 1] = 0;

		return -1;
	}

	/* send success message */
	strcpy(message, ctrlName);
	strcat(message, " added");

	if (send(kbjoyStruct->fdOutSend, message, 22, 0) < 0)
		syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));

	return 0;
}

/* this function is executed when the command "del [gamepad number]" is received.
## It deletes the virtual contoller who becomes disconnected in games */
int DelCommand(struct kbjoy *kbjoyStruct, char buff[])
{
	int playerNumber;
	char ctrlName[15];
	char message[24];

	/* convert ASCII number to player number */
	if ((playerNumber = GetPlayerNumberFromASCII(buff[4])) < 0)
		return -1;

	syslog(LOG_INFO, "received: del command for the player %d \n", playerNumber);

	/* verify that the device exist
	## don't forget to remove 1 because an array begins at the index 0 */
	if (kbjoyStruct->allJoysticks[playerNumber - 1] == 0)
	{
		syslog(LOG_CRIT, "error: the virtual controller number %d not exist\n", playerNumber);
		return -1;
	}

	/* remove the mapping process if exist */
	if (StopMapping(kbjoyStruct, playerNumber) < -1)
		return -1;

	/* remove the virtual gamepad */
	if (DeleteVirtualJoy(kbjoyStruct->allJoysticks[playerNumber - 1]) < 0)
		return -1;
	kbjoyStruct->allJoysticks[playerNumber - 1] = 0;

	/* Name of Gamepad */
	GetControllerName(ctrlName, buff[4]);

	/* send success message */
	strcpy(message, ctrlName);
	strcat(message, " removed");

	if (send(kbjoyStruct->fdOutSend, message, 24, 0) < 0)
		syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));

	return 0;
}

/* convert ASCII number to int and verify it is a correct player number */
int GetPlayerNumberFromASCII(char ASCIInumber)
{
	int playerNumber;

	playerNumber = ASCIInumber - 48;
	if ((playerNumber < 1) || (playerNumber > MAX_PLAYERS))
	{
		syslog(LOG_CRIT, "error: invalid player number\n");
		return -1;
	}

	return playerNumber;
}

/* Be carefull, ctrlName must be at least 15 characters length
## this function return the name for the current virtual joystick */
void GetControllerName(char ctrlName[15], char ASCIIplayerNumber)
{
	strcpy(ctrlName, "kbjoy Player X");
	/* don't forget to use the ASCII value */
	ctrlName[13] = ASCIIplayerNumber;
}

/* this function is executed when the command "start [gamepad number] [keyboard path] [config path]"
## is received. It starts the remapping of a keyboard by creating a new child */
int StartCommand(struct kbjoy *kbjoyStruct, char buff[])
{
	int playerNumber;
	pid_t pid;
	char message[24];
	int fdKeyboard;
	int fdConfig;
	char *buffPath;
	int ret;

	/* convert ASCII number to player number */
	if ((playerNumber = GetPlayerNumberFromASCII(buff[6])) < 0)
		return -1;

	syslog(LOG_INFO, "received: start command for the player %d \n", playerNumber);

	/* verify that the device exist
	## don't forget to remove 1 because an array begins at the index 0 */
	if (kbjoyStruct->allJoysticks[playerNumber - 1] == 0)
	{
		syslog(LOG_CRIT, "error: the virtual controller number %d not exist\n", playerNumber);
		return -1;
	}

	/* verify that the remapper process not exist
	## don't forget to remove 1 because an array begins at the index 0 */
	if (kbjoyStruct->allPlayers[playerNumber - 1] != 0)
	{
		syslog(LOG_CRIT, "error: the parsing process for player %d already exist\n", playerNumber);
		return -1;
	}

	/* open the specified keyboard */
	if (buff[7] == '\0')
	{
		syslog(LOG_CRIT, "error: no keyboard specified\n");
		return -1;
	}

	buffPath = malloc(sizeof(char) * (strlen(buff) + 1));
	ret = GetPath(buffPath, &buff[8]);

	syslog(LOG_INFO, "keyboard path: %s\n", buffPath);
	if ((fdKeyboard = open(buffPath, O_RDWR)) < 0)
	{
		syslog(LOG_CRIT, "cannot open keyboard : %d (%s)\n", errno, strerror(errno));
		free(buffPath);
		return -1;
	}

	/* get config file path */
	if (ret)
	{
		/* next char is at buff index: 8 + strlen(buffPath) + 1 */
		GetPath(buffPath, buff + strlen(buffPath) + 9);
		syslog(LOG_INFO, "config path: %s\n", buffPath);
	}
	else
	{
		char *defaultConfig = DEFAULTCONFIG;

		free(buffPath);
		buffPath = malloc(sizeof(char) * (strlen(defaultConfig) + 1));

		strcpy(buffPath, defaultConfig);

		syslog(LOG_INFO, "no config specified, use the default config file: %s\n", buffPath);
	}

	/* open config file */
	if ((fdConfig = open(buffPath, O_RDONLY)) < 0)
	{
		syslog(LOG_CRIT, "cannot open config file : %d (%s)\n", errno, strerror(errno));
		free(buffPath);
		return -1;
	}

	/* create new process */
	if ((pid = fork()) < 0)
	{
		syslog(LOG_CRIT, "fork failed : %d (%s)\n", errno, strerror(errno));
		return -1;
	}

	/* if we are in the child */
	if (pid == 0)
	{
		char *args[7];
		int i;

		/* force the child to stop when its parent die */
		prctl(PR_SET_PDEATHSIG, SIGINT);

		syslog(LOG_INFO, "kbjoy remapper for player %d -> pid: %d\n", playerNumber, getpid());

		args[0] = KBJOYREMAPPER;
		args[6] = NULL;

		for (i = 1; i < 5; i++)
			args[i] = malloc(6 * sizeof(char));

		sprintf(args[1], "%d", kbjoyStruct->fdOutSend);
		sprintf(args[2], "%d", kbjoyStruct->allJoysticks[playerNumber - 1]);
		sprintf(args[3], "%d", fdKeyboard);
		sprintf(args[4], "%d", fdConfig);

		args[5] = malloc(sizeof(char));
		args[5] = (char *)&playerNumber;

		if (execv(KBJOYREMAPPER, args) == -1)
		{
			syslog(LOG_CRIT, "execv failed : %d (%s)\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		for (i = 1; i < 6; i++)
			free(args[i]);

		exit(EXIT_SUCCESS);
	}

	/* in the parent */
	close(fdKeyboard);
	close(fdConfig);

	kbjoyStruct->allPlayers[playerNumber - 1] = pid;

	/* send success message */
	GetControllerName(message, buff[6]);
	strcat(message, " started");

	if (send(kbjoyStruct->fdOutSend, message, 24, 0) < 0)
		syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", message, errno, strerror(errno));

	return 0;
}

/* this function is executed when the command "stop [gamepad number]" is received.
## It stop the remapping of a keyboard by killing the correct child */
int StopCommand(struct kbjoy *kbjoyStruct, char buff[])
{
	int playerNumber;

	/* convert ASCII number to player number */
	if ((playerNumber = GetPlayerNumberFromASCII(buff[5])) < 0)
		return -1;

	syslog(LOG_INFO, "received: stop command for the player %d \n", playerNumber);

	if (StopMapping(kbjoyStruct, playerNumber) < 0)
		return -1;

	return 0;
}

/* kill the remapping process if exist for the player specified
## return -1 if no process found
## return -2 on error */
int StopMapping(struct kbjoy *kbjoyStruct, int playerNumber)
{
	/* verify that the remapper process exist
	## don't forget to remove 1 because an array begins at the index 0 */
	if (kbjoyStruct->allPlayers[playerNumber - 1] == 0)
	{
		syslog(LOG_INFO, "error: the parsing process for player %d not exist\n", playerNumber);
		return -1;
	}

	/* stop the remapper process */
	if (kill(kbjoyStruct->allPlayers[playerNumber - 1], SIGINT) < 0)
	{
		syslog(LOG_CRIT, "kill failed: %d (%s)\n", errno, strerror(errno));
		return -2;
	}

	return 0;
}

/* get arguments separated by spaces
## '\ ' becomes a space
## take care dest is big enough
## return 0 if src is totally read */
int GetPath(char *dest, char *src)
{
	/* result = malloc(sizeof(char) * (strlen(buff) + 1)); */
	char *buff;
	buff = dest;

	/* while there is char to read */
	while (*src != '\0')
	{
		/* if arg terminated by space */
		if (*src == ' ')
		{
			*buff = '\0';
			buff++;
			return 1;
		}

		/* replace '\ ' by a space */
		if ((*src == '\\') && (*(src + 1) == ' '))
			src++;

		*buff = *src;
		buff++;
		src++;
	}

	*buff = '\0';
	return 0;
}

/* send state of all possible devices:
## - "running" -> the device is parsed
## - "exist" -> the device is visible in game but does nothing
## - "null" -> default state, the device is dead */
void InfoCommand(struct kbjoy *kbjoyStruct)
{
	char buff[40];
	int i;
	pid_t pid;

	memset(buff, 0, sizeof(buff));
	strcpy(buff, "max-player:");
	buff[11] = MAX_PLAYERS + 48;

	if (send(kbjoyStruct->fdOutSend, buff, 40, 0) < 0)
		syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		strcpy(buff, "player-X:");
		buff[7] = i + 49;

		if ((pid = kbjoyStruct->allPlayers[i]) != 0)
		{
			/* make sure pid will have no more characters */
			if (pid < __INT_MAX__)
				sprintf(buff, "player-%d:running,pid=%d", i + 1,
						kbjoyStruct->allPlayers[i]);
		}
		else if (kbjoyStruct->allJoysticks[i] != 0)
			strcat(buff, "exist");
		else
			strcat(buff, "null");

		if (send(kbjoyStruct->fdOutSend, buff, 40, 0) < 0)
			syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));
	}

	
	if ((pid = kbjoyStruct->triggerProcess) != 0)
	{
		/* make sure pid will have no more characters (=< 10chars) */
		if (pid < __INT_MAX__)
			sprintf(buff, "trigger-proc:running,pid=%d", kbjoyStruct->triggerProcess);
	}
	else
		strcpy(buff, "trigger-proc:null");

	if (send(kbjoyStruct->fdOutSend, buff, 40, 0) < 0)
		syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));
}

/* send all keyboards paths found on this computer */
int ListCommand(struct kbjoy *kbjoyStruct)
{
	int fdDevsFile;
	int eventNumber;

	if ((fdDevsFile = OpenDevsInfoFile()) < 0)
		return -1;

	if (send(kbjoyStruct->fdOutSend, "list-start", 11, 0) < 0)
	{
		syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "list-start", errno, strerror(errno));
		close(fdDevsFile);
		return -1;
	}

	while ((eventNumber = FindNextKeyboard(fdDevsFile)) > -1)
	{
		char buff[20];
		/* make sure there is no more characters */
		if (eventNumber < 100)
			sprintf(buff, "/dev/input/event%d", eventNumber);

		if (send(kbjoyStruct->fdOutSend, buff, 20, 0) < 0)
			syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));
	}

	close(fdDevsFile);

	if (eventNumber < -1)
		return -1;

	if (send(kbjoyStruct->fdOutSend, "list-end", 9, 0) < 0)
		syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "list-end", errno, strerror(errno));

	return 0;
}

/* received the command: trigger ["stop" | key_name]
## it will start a process (or stop it if the arg is "stop")
## who send a signal for each connected keyboards when the
## key in key_name is pressed or realeased
## (the keyboards are listed once) */
int TriggerCommand(struct kbjoy *kbjoyStruct, char buff[])
{
	char *commandArg;
	int triggerKey;
	int fdDevsFile;
	pid_t pid;

	commandArg = &buff[8];
	/* if the second arg of the trigger command is "stop",
	## kill the trigger process (if exist) */
	if (!strcmp(commandArg, "stop"))
	{
		if (kbjoyStruct->triggerProcess == 0)
			syslog(LOG_INFO, "trigger command: no process to stop\n");

		/* stop the remapper process */
		else if (kill(kbjoyStruct->triggerProcess, SIGINT) < 0)
		{
			syslog(LOG_CRIT, "kill failed: %d (%s)\n", errno, strerror(errno));
			return -1;
		}

		return 0;
	}
	/* else it contains the trigger key name */
	SetKeyboardKey(commandArg, &triggerKey);

	if (triggerKey == 0)
		return -1;

	/* verify the process not already running */
	if (kbjoyStruct->triggerProcess != 0)
	{
		syslog(LOG_CRIT, "trigger command error: trigger process already running\n");
		return -1;
	}

	/* create new process */
	if ((pid = fork()) < 0)
	{
		syslog(LOG_CRIT, "fork failed : %d (%s)\n", errno, strerror(errno));
		return -1;
	}

	/* if we are in the child */
	if (pid == 0)
	{
		char *args[14];
		int ret;
		int i;

		/* force the child to stop when its parent die */
		prctl(PR_SET_PDEATHSIG, SIGINT);

		memset(args, 0, sizeof(args));
		args[0] = KBJOYTRIGGER;

		/* save the number of all events in /dev/input
		## who are keyboards (maxi 10 keyboards) */
		if ((fdDevsFile = OpenDevsInfoFile()) < 0)
			return -1;

		/* a file descriptor =<65535, we need 5 characters + end character */
		args[1] = malloc(6 * sizeof(char));
		sprintf(args[1], "%d", kbjoyStruct->fdOutSend);

		/* a keyboard code <1000, we need 3 characters + end character */
		args[2] = malloc(4 * sizeof(char));
		sprintf(args[2], "%d", triggerKey);

		i = 3;
		while ((ret = FindNextKeyboard(fdDevsFile)) > -1)
		{
			/* verify there is no more characters */
			if (ret < 100)
			{
				args[i] = malloc(3 * sizeof(char));
				sprintf(args[i], "%d", ret);
			}

			i++;
			if (i > 12)
				break;
		}

		close(fdDevsFile);

		/* if there is an error, ignore "execv" */
		if (ret < -2)
		{
			int k;
			syslog(LOG_CRIT, "trigger command: error during keyboard search\n");
			for (k = 1; k < i; k++)
				free(args[i]);
			exit(EXIT_FAILURE);
		}

		if (execv(KBJOYTRIGGER, args) == -1)
		{
			syslog(LOG_CRIT, "execv failed: %d (%s)\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		/* may be useless */
		exit(EXIT_SUCCESS);
	}

	kbjoyStruct->triggerProcess = pid;
	return 0;
}
