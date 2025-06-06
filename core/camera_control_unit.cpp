/**
 * Camera Control Unit
 * Allows to remotely get and set camera settings
 * Use a listening TCP socket that can serve multiple clients.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <linux/limits.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <string.h>
#include <math.h>

#include "camera_control_unit.hpp"

int listenSocket(struct in_addr *address, unsigned short port){
        struct sockaddr_in local_sock_addr;
        local_sock_addr.sin_family = AF_INET;
        local_sock_addr.sin_addr = *address;
        local_sock_addr.sin_port = port;
        int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        // fprintf(stderr, "listen_socket=%d" "\n", listen_socket);
        const int enable = 1;
        if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
                // fprintf(stderr,"setsockopt(SO_REUSEADDR) failed" "\n");
        }else{
                int error = bind(listen_socket, (struct sockaddr *)&local_sock_addr, sizeof(local_sock_addr));
                if(error){
                        close(listen_socket);
                        listen_socket = -1;
                }
        }
        if(listen_socket >= 0){
                int error = listen(listen_socket, 1);
                if(error){
                        close(listen_socket);
                        listen_socket = -2;
                }
        }
        return listen_socket;
}


CameraControlUnit::CameraControlUnit(RPiCamApp *app, unsigned short tcpListenPort){
	cameraApp = app;
	struct in_addr listenAddress = {0}; // bind to this address for score connections
	listeningSocket = listenSocket(&listenAddress, htons(tcpListenPort));
	for(int i = 0 ; i < CAMERA_CONTROL_UNIT_MAX_CLIENT ; i++){
		clients[i].fd = -1;
		clients[i].events = POLLIN|POLLHUP|POLLERR;
		parsers[i].index = 0;
	}
	updateFirstFreeSlot();
}

CameraControlUnit::~CameraControlUnit(void){
	close(listeningSocket);
	for(int i = 0 ; i < CAMERA_CONTROL_UNIT_MAX_CLIENT ; i++){
		close(clients[i].fd);
	}
}

void CameraControlUnit::updateFirstFreeSlot(void){
	for(int i = 0 ; i < CAMERA_CONTROL_UNIT_MAX_CLIENT ; i++){
		if(-1 == clients[i].fd){
			firstFreeSlot = i;
			return;
		}
	}
	firstFreeSlot = -1;
}

static const char *const BANNER = "Welcome to CCU v0.01" "\n";

void CameraControlUnit::sendString(int fd, const char *szString){
	write(fd, szString, strlen(szString));
}

int CameraControlUnit::analyseInput(int clientIndex){
	InputParser *parser = parsers + clientIndex;
	if(0 == parser->index){
		// Empty request
		char emptyRequest[128];
		const libcamera::ControlList &properties = cameraApp->GetProperties();
		snprintf(emptyRequest, sizeof(emptyRequest) - 1, "Empty request from client at index %2d: properties.size()=%lu" "\n", clientIndex, properties.size());
		sendString(clients[clientIndex].fd, emptyRequest);
		for(auto iter = properties.begin() ; iter != properties.end() ; iter++){
			snprintf(emptyRequest, sizeof(emptyRequest) - 1, "%d:%s" "\n", iter->first, iter->second.toString().c_str());
			sendString(clients[clientIndex].fd, emptyRequest);
		}

	}else{
		char clientRequest[128 + CAMERA_CONTROL_UNIT_PARSER_BUFFER_SIZE];
		snprintf(clientRequest, sizeof(clientRequest) - 1, "Request from client at index %2d (%u/%u): %s" "\n", clientIndex, parser->index, (unsigned int)sizeof(parser->buffer) - 1, parser->buffer);
		sendString(clients[clientIndex].fd, clientRequest);
		char *equal = strchr(parser->buffer, '=');
		char *questionMark = strchr(parser->buffer, '?');
		if(questionMark != 0){
			*questionMark = '\0';
		}
		if(equal != NULL){
			*equal = '\0';
			if(questionMark == (equal + 1)){
				// Though question !
			}else{
				char *start = equal + 1;
				char *end = NULL;
				float value = strtof(start, &end);
				if(end != start){
					if(!strcmp(parser->buffer, "angle")){
						float angle = value;
						snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested exposure angle: %f" "\n", angle);
						sendString(clients[clientIndex].fd, clientRequest);
						if(5.0 <= angle && angle <= 355.0){
							float exposureTime = 33300.0 * angle / 360.0;
							libcamera::ControlList controls;
							controls.set(controls::ExposureTime, exposureTime);
							cameraApp->SetControls(controls);
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested exposure angle %f leads to exposure time of %f microseconds" "\n", angle, exposureTime);
						}else{
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested exposure angle MUST be in [5 .. 355]" "\n");
						}
						sendString(clients[clientIndex].fd, clientRequest);
					}
					if(!strcmp(parser->buffer, "saturation")){
						float saturation = value;
						if(0.0 <= saturation && saturation <= 9.0){
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested saturation: %f" "\n", saturation);
							libcamera::ControlList controls;
							controls.set(controls::Saturation, saturation);
							cameraApp->SetControls(controls);
						}else{
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested saturation MUST be in [0 .. 9]" "\n");
						}
						sendString(clients[clientIndex].fd, clientRequest);
					}
					if(!strcmp(parser->buffer, "gamma")){
						float gamma = value;
						if(0.0 <= gamma && gamma <= 9.0){
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested gamma: %f" "\n", gamma);
							libcamera::ControlList controls;
							controls.set(controls::Contrast, gamma);
							cameraApp->SetControls(controls);
						}else{
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested gamma MUST be in [1 .. 9]" "\n");
						}
						sendString(clients[clientIndex].fd, clientRequest);
					}
					if(!strcmp(parser->buffer, "contrast")){
						float contrast = value;
						if(0.0 <= contrast && contrast <= 9.0){
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested contrast: %f" "\n", contrast);
							libcamera::ControlList controls;
							controls.set(controls::Contrast, contrast);
							cameraApp->SetControls(controls);
						}else{
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested contrast MUST be in [0 .. 9]" "\n");
						}
						sendString(clients[clientIndex].fd, clientRequest);
					}
					if(!strcmp(parser->buffer, "temperature")){
						float temperature = value;
						if(2700.0 <= temperature && temperature <= 5500.0){
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested temperature: %f" "\n", temperature);
							libcamera::ControlList controls;
							controls.set(controls::ColourTemperature, (uint32_t)temperature);
							cameraApp->SetControls(controls);
						}else{
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested temperature MUST be in [2700 .. 5500]" "\n");
						}
						sendString(clients[clientIndex].fd, clientRequest);
					}
					if(!strcmp(parser->buffer, "gain")){
						float dB = value;
						snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested analogue gain: %fdB" "\n", dB);
						sendString(clients[clientIndex].fd, clientRequest);
						if(0.0 <= dB && dB <= 27.0){
							float digitalGain = 1.0f;
							float analogueGain = powf(10, dB / 20.0f);
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested gain %fdB leads to linearGain %f" "\n", dB, analogueGain);
							libcamera::ControlList controls;
							controls.set(controls::AnalogueGain, analogueGain);
							controls.set(controls::DigitalGain, digitalGain);
							cameraApp->SetControls(controls);
						}else{
							snprintf(clientRequest, sizeof(clientRequest) - 1, "Requested dB gain MUST be in [0 .. 27]" "\n");
						}
						sendString(clients[clientIndex].fd, clientRequest);
					}
				}
			}
		}
	}
	return(0);
}

int CameraControlUnit::parseInput(int clientIndex, char *buffer, int bufferLen){
	InputParser *parser = parsers + clientIndex;
	char *p = buffer;
	while(bufferLen > 0){
		if(parser->index < (sizeof(parser->buffer) - 1)){
			char c = *p++;
			if('\n' == c){
				parser->buffer[parser->index] = '\0'; // terminate string
				analyseInput(clientIndex);
				parser->index = 0;
			}else if('\r' != c){
				parser->buffer[parser->index++] = tolower(c);
			}
		}else{
			// overflow: reset input buffer
			parser->index = 0;
			break;
		}
		bufferLen--;
	}
	return(parser->index);
}

void CameraControlUnit::printControl(int fd, const libcamera::ControlId *second){
	char debugString[256];
	int controlType = second->type();
	size_t controlSize = second->size();
	const char *name = second->name().c_str();
	const char *vendor = second->vendor().c_str();
	snprintf(debugString, sizeof(debugString) - 1, "id():%5d,type:%d,size:%lu,name():%s,vendor():%s" "\n", second->id(), controlType, controlSize, name, vendor);
	sendString(fd, debugString);
}

int CameraControlUnit::run(void){
	int returnValue = 0;
	// Check incoming connections
	if(firstFreeSlot != -1){
		pollfd checkIncomingConnection;
	       	checkIncomingConnection.fd = listeningSocket;
		checkIncomingConnection.events = POLLIN;
		if(poll(&checkIncomingConnection, 1, 0) > 0){
			int fd = accept(listeningSocket, NULL, NULL);
			clients[firstFreeSlot].fd = fd;
			int nonBlocking = 1;
			ioctl(fd, FIONBIO, &nonBlocking);
			updateFirstFreeSlot();
			sendString(fd, BANNER);
			char debugString[256];
			snprintf(debugString, sizeof(debugString) - 1, "libcamera::controls::controls.size()=%lu" "\n", libcamera::controls::controls.size());
			sendString(fd, debugString);
			for(auto iter = libcamera::controls::controls.begin() ; iter != libcamera::controls::controls.end() ; iter++){
				printControl(fd, iter->second);
			}
			returnValue++;
		}
	}
	// Check connected sockets for incoming data
	for(int i = 0 ; i < CAMERA_CONTROL_UNIT_MAX_CLIENT ; i++){
		clients[i].events = POLLIN;
	}
	int ready = poll(clients, CAMERA_CONTROL_UNIT_MAX_CLIENT, 0);
	if(ready > 0){
		for(int i = 0 ; i < CAMERA_CONTROL_UNIT_MAX_CLIENT ; i++){
			if(clients[i].revents & POLLIN){
				char rxBuffer[CAMERA_CONTROL_UNIT_PARSER_BUFFER_SIZE];
				int received = read(clients[i].fd, rxBuffer, sizeof(rxBuffer));
				parseInput(i, rxBuffer, received);
				returnValue++;
			}
			if(clients[i].revents & (POLLERR | POLLHUP)){
				close(clients[i].fd);
				clients[i].fd = -1;
				updateFirstFreeSlot();
				parsers[i].index = 0;
				returnValue++;
			}
		}
	}
	return returnValue;
}

void CameraControlUnit::updateFromMetadata(libcamera::ControlList &metadata){
#if 0
	int fd = clients[0].fd;
	if(fd > 0){
		char emptyRequest[256];
		snprintf(emptyRequest, sizeof(emptyRequest) - 1, "metadata.size()=%lu" "\n", metadata.size());
		sendString(fd, emptyRequest);
		for(auto iter = metadata.begin() ; iter != metadata.end() ; iter++){
			switch(iter->first){
				case libcamera::controls::AE_ENABLE:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "AE_ENABLE(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
				case libcamera::controls::AE_LOCKED:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "AE_LOCKED(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
				case libcamera::controls::AE_METERING_MODE:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "AE_METERING_MODE(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
				case libcamera::controls::AE_CONSTRAINT_MODE:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "AE_CONSTRAINT_MODE(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
				case libcamera::controls::AE_EXPOSURE_MODE:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "AE_EXPOSURE_MODE(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::EXPOSURE_VALUE:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "EXPOSURE_VALUE(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
				case libcamera::controls::EXPOSURE_TIME:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "EXPOSURE_TIME(%d):%sus" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::ANALOGUE_GAIN:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "ANALOGUE_GAIN(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
				case libcamera::controls::DIGITAL_GAIN:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "DIGITAL_GAIN(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
				case libcamera::controls::SENSOR_BLACK_LEVELS:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "SENSOR8BLACK_LEVELS(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
				case libcamera::controls::FRAME_DURATION:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "FRAME_DURATION(%d):%sus" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::FOCUS_FO_M:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "FOCUS_FO_M(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::LUX:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "LUX(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::COLOUR_CORRECTION_MATRIX:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "COLOUR_CORRECTION_MATRIX(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::COLOUR_TEMPERATURE:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "COLOUR_TEMPERATURE(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::COLOUR_GAINS:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "COLOUR_GAINS(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::SCALER_CROP:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "SCALER_CROP(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::SENSOR_TIMESTAMP:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "SENSOR_TIMESTAMP(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				case libcamera::controls::FRAME_WALL_CLOCK:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "FRAME_WALL_CLOCK(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;

				default:
					snprintf(emptyRequest, sizeof(emptyRequest) - 1, "UNKNOWN(%d):%s" "\n", iter->first, iter->second.toString().c_str());
					break;
			}
			sendString(fd, emptyRequest);
		}
	}
#endif
}


