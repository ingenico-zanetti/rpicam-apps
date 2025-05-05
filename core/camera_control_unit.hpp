/**
 * Camera Control Unit
 * Allows to remotely get and set camera settings
 * Use a listening TCP socket that can serve multiple clients.
 */
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rpicam_app.hpp"

#define CAMERA_CONTROL_UNIT_MAX_CLIENT (16)

#define CAMERA_CONTROL_UNIT_PARSER_BUFFER_SIZE (1024)

struct InputParser {
	unsigned int index;
	char buffer[CAMERA_CONTROL_UNIT_PARSER_BUFFER_SIZE];
};

class CameraControlUnit
{
	public:
		CameraControlUnit(RPiCamApp *app, unsigned short tcpListenPort);
		~CameraControlUnit();
		int run(void);
		void updateFromMetadata(libcamera::ControlList &metadata);
	private:

	RPiCamApp *cameraApp;
	int listeningSocket;
	struct pollfd clients[CAMERA_CONTROL_UNIT_MAX_CLIENT];
	InputParser parsers[CAMERA_CONTROL_UNIT_MAX_CLIENT];
	void sendString(int fd, const char *szString);
	void printControl(int fd, const libcamera::ControlId *second);
	int parseInput(int clientIndex, char *input, int inputLen);
	int analyseInput(int clientIndex);

	void updateFirstFreeSlot(void);
	int firstFreeSlot;

};

