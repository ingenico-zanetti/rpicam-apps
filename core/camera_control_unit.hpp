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

class CameraControlUnit;

typedef enum {
	CCU_CALLBACK_MODE_WRITE = 0,   // set a value  (speed=16000)
	CCU_CALLBACK_MODE_READ = 1,    // get a value  (speed?)
	CCU_CALLBACK_MODE_SYNTAX = 2,  // get syntax   (speed=?)
	CCU_CALLBACK_MODE_COMMAND = 3  // send command (exit, quit, shutdown, ...)
} Ccu_Callback_Mode_e;

typedef bool (CameraControlUnit::*Callback)(int index, Ccu_Callback_Mode_e mode, const char * args);

class CameraControlUnit
{
	public:
		CameraControlUnit(RPiCamApp *app, unsigned short tcpListenPort);
		~CameraControlUnit();
		bool run(void);
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
	std::unordered_map<std::string, Callback> map;
	bool shutdown;

	bool contrastCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool saturationCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool gammaCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool awbgainsCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool temperatureCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool shutdownCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool gaindbCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool gainCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool shutterSpeedCallback(int index, Ccu_Callback_Mode_e mode, const char *args);
	bool shutterAngleCallback(int index, Ccu_Callback_Mode_e mode, const char *args);

	void updateFirstFreeSlot(void);
	int firstFreeSlot;

};

