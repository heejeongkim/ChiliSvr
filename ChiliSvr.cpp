/*
 * ChiliSvr.cpp
 *
 *  Created on: Aug 19, 2014
 *      Author: chili
 */

#include <czmq.h>
#include <zmq.h>
#include <stdio.h>
using namespace std;

char *  s_recv (void *socket) {
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int size = zmq_msg_recv(&msg, socket, 0);
    if(size == -1){
        return NULL;
    }
    char* str = (char *) malloc(size+1);
    memcpy(str, zmq_msg_data(&msg), size);
    zmq_msg_close(&msg);
    str[size]=0;
    return (str);
}

char* getIp(){
    struct ifaddrs *ifAddrStruct=NULL;
    struct ifaddrs *ifa=NULL;
    void *tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa ->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            //char addressBuffer[INET_ADDRSTRLEN];
            char* addressBuffer = (char*) malloc(INET_ADDRSTRLEN);
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if(strcmp(ifa->ifa_name, "eth0") == 0){
                //printf("%s IP Address===== %s\n", ifa->ifa_name, addressBuffer);
                return addressBuffer;
            }
        } else if (ifa->ifa_addr->sa_family==AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
           // printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
        }
    }
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    return "[ERROR] getIp(): SOMETHING WRONG";
}

char* svrIP = (char *)malloc(INET_ADDRSTRLEN);

char* initSock = "5556";     //REP port
char* updateSock = "5557";	//PULL port
char* pubSock = "5558";		//PUB port
char* storeSock = "5559";	//PULL port


void* publisher;
void* initializer;
void* propagater;
void* stateSaver;

void initialization(char* fileName, char* defaultState) {
	FILE* f= fopen(fileName, "r");

	//When there's no file (no existing state), generates new file to store state
	if(f == NULL){
		FILE* pFile = fopen(fileName, "w");
		if (pFile != NULL) {
			fputs(defaultState, pFile);
			fclose(pFile);
		}
		zmq_send(initializer, defaultState, strlen(defaultState), 0);
	}
	//If there is an existing state (state of other clients), return it
	else{
		fseek(f, 0, SEEK_END);
		int len = ftell(f);
		rewind(f);

		char* contents = (char*) malloc(sizeof(char) * len);

		if (contents == NULL){
			fprintf(stderr, "Failed to allocate memory");
			return;
		}
		fread(contents, sizeof(char), len, f);
		fclose(f);

		zmq_send(initializer, contents, len, 0);
		free(contents);
	}
}

void writeFile(char* fileName, char* contents, bool isLogFile) {
	if(isLogFile){
		FILE* pFile = fopen(fileName, "a"); //append contents
		if (pFile != NULL) {
			fputs(contents, pFile);
			fputs("\n", pFile);
			fclose(pFile);
		}
	}else{
		FILE* pFile = fopen(fileName, "w"); //Overwrite
		if (pFile != NULL) {
			fputs(contents, pFile);
			fclose(pFile);
		}
	}

}



int main() {
	//Get IP address of this computer(server) and print the address
	char* svrIp = getIp();
	printf("ServerIP: %s\n", svrIp);

	//  Prepare our context and sockets
	int size = strlen("tcp://*:") + 4;
	char addr[size];

	zctx_t *ctx = zctx_new();
	//PUB socket
	publisher = zsocket_new(ctx, ZMQ_PUB);
	sprintf(addr, "tcp://*:%s", pubSock);
	zmq_bind(publisher, addr);
	//REP socket
	initializer = zsocket_new(ctx, ZMQ_REP);
	sprintf(addr, "tcp://*:%s", initSock);
	zmq_bind(initializer, addr);
	//Pull socket (Update Responder)
	propagater = zsocket_new(ctx, ZMQ_PULL);
	sprintf(addr, "tcp://*:%s", updateSock);
	zmq_bind(propagater, addr);
	//Pull socket (State saver)
	stateSaver = zsocket_new(ctx, ZMQ_PULL);
	sprintf(addr, "tcp://*:%s", storeSock);
	zmq_bind(stateSaver, addr);


	// Initialize poll set
	zmq_pollitem_t msgs[] = {
			{ propagater, 0, ZMQ_POLLIN, 0 },
			{ stateSaver, 0, ZMQ_POLLIN, 0 },
			{ initializer, 0, ZMQ_POLLIN, 0 }
	};

	//  Process messages from sockets
	while (1) {
		//Listen msg from clients
		zmq_poll(msgs, 3, -1);

		//When a update-msg is arrived
		if (msgs[0].revents & ZMQ_POLLIN) {
			//1. Receive new state
			char* newStateMsg = s_recv(propagater);
			char* topic = (char*) malloc(strlen(newStateMsg));
			char* defaultState = (char*) malloc(strlen(newStateMsg));
			//char defaultState[strlen(newStateMsg)];
			sscanf(newStateMsg, "%s %s", topic, defaultState);
			writeFile(topic, defaultState, false);
			//printf("[Received newState]: %s\n", newStateMsg);

			//2. Publish new state to clients
			zmq_send(publisher, newStateMsg, strlen(newStateMsg), 0);
			free(topic);
			free(defaultState);
		}
		//When storeState request is arrived
		else if (msgs[1].revents & ZMQ_POLLIN) {
			//1. Receive new state
			char* newStateMsg = s_recv(stateSaver);
			printf("[Received newState]: %s\n", newStateMsg);
			char* topic = (char*) malloc(strlen(newStateMsg));
			char* contents = (char*) malloc(strlen(newStateMsg));
			sscanf(newStateMsg, "%s %s", topic, contents);
			writeFile(topic, contents, true);

			free(topic);
			free(contents);
		}
		//When a init-state request is arrived
		else if (msgs[2].revents & ZMQ_POLLIN) {
			//1. Receive name of state
			char* msg = s_recv(initializer);
			//2. Send init state
			char* topic = (char*) malloc(strlen(msg));
			char* defaultState = (char*) malloc(strlen(msg));
			sscanf(msg, "%s %s", topic, defaultState);
			printf("Send Init : %s\n", msg);
			initialization(topic, defaultState);

			free(topic);
			free(defaultState);
			free(msg);
		}else{
			printf("Cannot handle msg arrived.\n");
			break;
		}

	}

	//Close sockets & destroy context
	zmq_close (publisher);
	zmq_close (initializer);
	zmq_close (propagater);
	zctx_destroy(&ctx);

	return 0;
}
