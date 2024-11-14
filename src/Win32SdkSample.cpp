
/* Trigno SDK Example.  Copyright (C) 2011-2012 Delsys, Inc.
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to 
* deal in the Software without restriction, including without limitation the 
* rights to use, copy, modify, merge, publish, and distribute the Software, 
* and to permit persons to whom the Software is furnished to do so, subject to 
* the following conditions:
* 
* The above copyright notice and this permission notice shall be included in 
* all copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#include "stdafx.h"

SOCKET commSock;	//command socket
SOCKET emgSock;		//EMG socket
SOCKET accSock;		//ACC socket
SOCKET imemgSock;   //IM EMG socket
SOCKET auxSock;  //IM AUX socket


//Prompt for an input line using a default value
void QueryResponse(char * prompt, char * default, char * response)
{
	printf("%s [%s]: ", prompt, default);
	gets(response);
	if(strlen(response)==0)	//use default value if empty line
		strcpy(response, default);
}

//Send a command to the server and wait for the response
void SendCommandWithResponse(char * command, char * response)
{
	int n;
	char tmp[128];

	strcpy(tmp, command);
	tmp[strlen(command)-2]=0;	//trip last CR/LF
	printf("Command: %s", tmp);	//display to user

	if (send(commSock, command, strlen(command), 0) == SOCKET_ERROR) 
	{
		printf("Can't send command.\n");
		_exit(1);	//bail out on error
	}

	//wait for response
	//note - this implementation assumes that the entire response is in one TCP packet
	n=recv(commSock, tmp, sizeof(tmp), 0);
	tmp[n-2]=0;	//strip last CR/LF
	printf("Response: %s", tmp);	//display to user
	strcpy(response, tmp);	//return the response line

	return;
}

//The main function. Program starts here.
int _tmain(int argc, _TCHAR* argv[])
{
	char response[32];	//buffer for response
	char tmp[128];	//string buffer for various uses
	char * onoff;	//used to hold the trigger state	
	int collectionTime;	//time to collect data in samples (seocnds * 2000)
	int sensor;	//sensor to save data
	char sensortype; // sensor type
	FILE *emgdata, *accdata;	//the files to be written with EMG and ACC data from standard snesor & EMG, ACC, GYRO and MAG data from IM sensor

	printf("Delsys Digital SDK Win32 Demo Applicatioln\n");

	//Open output files
	emgdata = fopen("EmgData.csv", "w"); 
	accdata = fopen("AccData.csv",  "w"); 
	//imemgdata=fopen("IM_EmgData.csv","w");
	//auxdata=fopen("AuxData.csv","w");

	if(emgdata == NULL || accdata == NULL)
	{
		printf("Can't open data files.\n");
		return 1;	//bail out id we can't open the data files
	}

	//Create headers for CSV files
	fprintf(emgdata, "Sample Number,Value\n");
	fprintf(accdata, "Sample Number,ACC X,ACC Y,ACC Z,GYRO X,GYRO Y,GYRO Z,MAG X,MAG Y,MAG Z\n");

	WSADATA wsaData;	//Windows socket data
	int iResult;	//result of call

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;	//could not open Windows sockets library
	}

	//Get the server URL. This can be a FQDN or IP address
	QueryResponse("Enter the server URL", "localhost", response);

	// Declare and initialize variables used for DNS resolution.
	char* ip = response;
	char* port = NULL;
	struct addrinfo aiHints;
	struct addrinfo *aiList = NULL;
	int retVal;

	// Setup the hints address info structure
	// which is passed to the getaddrinfo() function
	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_INET;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = IPPROTO_TCP;

	// Call getaddrinfo(). If the call succeeds,
	// the aiList variable will hold a linked list
	// of addrinfo structures containing response
	// information about the host
	if ((retVal = getaddrinfo(ip, port, &aiHints, &aiList)) != 0) 
	{
		printf("Invalid URL.\n");
		return 1;	//could not resolve URL
	}

	// Set up the connection to server for communication
	commSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in sinRemote;
	sinRemote.sin_family = AF_INET;
	sinRemote.sin_addr.s_addr = ((sockaddr_in*)(aiList->ai_addr))->sin_addr.s_addr;
	sinRemote.sin_port = htons(50040);

	//Try to connect
	if (connect(commSock, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		printf("Can't connect to Trigno Server!\n");
		return 1;	//server is not responding
	}
	else
	{
		printf("Connected to Trigno Server.\n");
	}

	int n;	//byte count
	//Get response from the server after connecting
	n=recv(commSock, tmp,sizeof(tmp), 0);
	tmp[n-2]=0;	//back up over second CR/LF
	printf(tmp);	//display to user


	// Set up the connection to server for EMG data
	emgSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sinRemote.sin_port = htons(50041);
	if (connect(emgSock, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		printf("Error initializing Trigno EMG data connection (can't connect).\n");
		return 1;
	}

	// Set up the connection to server for ACC data
	accSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sinRemote.sin_port = htons(50042);
	if (connect(accSock, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		printf("Error initializing Trigno ACC data connection (can't connect).\n");
		return 1;
	}
	//Set up the connection to server for IM EMG data
	imemgSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sinRemote.sin_port = htons(50043);
	if (connect(imemgSock, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		printf("Error initializing Trigno IM sensor EMG data connection (can't connect).\n");
		return 1;
	}

	//Set up the connection to server for IM ACC, GYRO , MAG data
	auxSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sinRemote.sin_port = htons(50044);
	if (connect(auxSock, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		printf("Error initializing Trigno IM AUX data connection (can't connect).\n");
		return 1;
	}


	//Query for start/stop triggers and send to the server
	QueryResponse("Start trigger (y/n)", "n", response);

	if (strcmp(response, "y")==0 || strcmp(response, "Y")==0)
		onoff="ON";
	else
		onoff="OFF";	//everything else means no
	sprintf(tmp, "TRIGGER START %s\r\n\r\n", onoff);
	SendCommandWithResponse(tmp, tmp);

	QueryResponse("Stop trigger (y/n)", "n", response);

	if (strcmp(response, "y")==0 || strcmp(response, "Y")==0)
		onoff="ON";
	else
		onoff="OFF";	//everything else means no
	sprintf(tmp, "TRIGGER STOP %s\r\n\r\n", onoff);
	SendCommandWithResponse(tmp, tmp);

	do
	{
		QueryResponse("Sensor to save data from (1-16)", "1", response);
		sensor = atoi(response);	//convert to int
	}
	while (sensor <1 || sensor >16);	//check that sensor number is valid

	//Get the type of the sensor
	sprintf(tmp,"SENSOR %d TYPE?\r\n\r\n",sensor);
	SendCommandWithResponse(tmp,&sensortype);

	//Get the time interval to acquire data
	do
	{
		QueryResponse("Data collection duration (sec.)", "20", response);
		collectionTime = atoi(response);
	}
	while (collectionTime <=0);	//make sure we have a valid time

	collectionTime *= 2000;	//adjuect to number of samples

	//Send command to start data acquisition
	SendCommandWithResponse("UPSAMPLE OFF\r\n\r\n", tmp);
	SendCommandWithResponse("START\r\n\r\n", tmp);

	printf("Please wait ");

	int k;	//byte count
	char emgbuf[4*16];		//buffer for one sample from each sensor
	char accbuf[4*3*16];	//buffer for one sample on each axis for each sensor
	char imemgbuf[4*16];   //buffer for one sample from each IM sensor
	char auxbuf[4*9*16];   //buffer for one sample on 9 axis for each IM sensor
	unsigned long bytesRead;	//bytes available for reading
	float emgsample;	//EMG sample
	float accsample[3];	//ACC sample (x, y, z axis)
	float imemgsample;  //IM EMG sample
	float auxsample[9]; // ACC sample (x, y, z axis), GYRO sample (x, y, z axis), MAG sample (x, y, z axis)
	int emgSampleNumber = 1;	//current sample number
	int accSampleNumber = 1;	//current sample number
	int imemgSampleNumber=1;	//current sample number
	int auxSampleNumber=1;		//current sample number
	//Read data from standard sensor
	if(sensortype=='D')
	{
		while(collectionTime>0)	//loop until all samples acquired
		{
			
				//See if we have enough data
					ioctlsocket(emgSock, FIONREAD, &bytesRead);	//peek at data available
					while(bytesRead >= sizeof(emgbuf))
					{
						k=recv(emgSock, emgbuf, sizeof(emgbuf), 0);
						//process data
						memcpy(&emgsample, &emgbuf[4*(sensor-1)], sizeof(emgsample));
						//Write to file
						fprintf(emgdata, "%d,%e\n", emgSampleNumber, emgsample);
						--collectionTime; ++emgSampleNumber;
						if(collectionTime%2000==0)
							printf(".");	//indicate progress to user
						ioctlsocket(emgSock, FIONREAD, &bytesRead);	//peek at data available
					}

					//See if we have enough data
					ioctlsocket(accSock, FIONREAD, &bytesRead);	//peek at data available
					while(bytesRead >= sizeof(accbuf))
					{
						k=recv(accSock, accbuf, sizeof(accbuf), 0);
						//process data
						memcpy(accsample, &accbuf[4*3*(sensor-1)], sizeof(accsample));
						//Write to file
						fprintf(accdata, "%d,%e,%e,%e\n", accSampleNumber, accsample[0], accsample[1], accsample[2]);
						++accSampleNumber;
						ioctlsocket(accSock, FIONREAD, &bytesRead);	//peek at data available
					}
		
			//Look for stop from server
			ioctlsocket(commSock, FIONREAD, &bytesRead);
			if (bytesRead > 0)
			{
				recv(commSock, tmp, bytesRead, 0);
				tmp[bytesRead] = '\0';
				if (strstr(tmp, "STOPPED\r\n") != NULL)
					collectionTime=0;//we are done
			}

			Sleep(10);	//let other threads run
		}


	}
	//Read data from IM sensor
	if(sensortype=='L')
	{
		while(collectionTime>0)	//loop until all samples acquired
		{
			//See if we have enough data
			ioctlsocket(imemgSock, FIONREAD, &bytesRead);	//peek at data available
			while(bytesRead >= sizeof(imemgbuf))
			{
				k=recv(imemgSock, imemgbuf, sizeof(imemgbuf), 0);
				//process data
				memcpy(&imemgsample, &imemgbuf[4*(sensor-1)], sizeof(imemgsample));
				//Write to file
				fprintf(emgdata, "%d,%e\n", imemgSampleNumber, imemgsample);
				--collectionTime; ++imemgSampleNumber;
				if(collectionTime%2000==0)
					printf(".");	//indicate progress to user
				ioctlsocket(imemgSock, FIONREAD, &bytesRead);	//peek at data available
			}

			//See if we have enough data
			ioctlsocket(auxSock, FIONREAD, &bytesRead);	//peek at data available
			while(bytesRead >= sizeof(auxbuf))
			{
				k=recv(auxSock, auxbuf, sizeof(auxbuf), 0);
				//process data
				memcpy(auxsample, &auxbuf[4*9*(sensor-1)], sizeof(auxsample));
				//Write to file
				fprintf(accdata, "%d,%e,%e,%e,%e,%e,%e,%e,%e,%e\n", auxSampleNumber, auxsample[0], auxsample[1], auxsample[2],auxsample[3],auxsample[4], auxsample[5],auxsample[6], auxsample[7], auxsample[8]);
				++auxSampleNumber;
				ioctlsocket(auxSock, FIONREAD, &bytesRead);	//peek at data available
			}

			//Look for stop from server
			ioctlsocket(commSock, FIONREAD, &bytesRead);
			if (bytesRead > 0)
			{
				recv(commSock, tmp, bytesRead, 0);
				tmp[bytesRead] = '\0';
				if (strstr(tmp, "STOPPED\r\n") != NULL)
					collectionTime=0;//we are done
			}

			Sleep(10);	//let other threads run
		}
	
	}
	

	printf("\nData collection complete, disconnecting.\n");

	//Stop data collection and disconnect
	SendCommandWithResponse("QUIT\r\n\r\n", tmp);

	//Close sockets and files
	fclose(emgdata);
	fclose(accdata);
	closesocket(commSock);
	closesocket(emgSock);
	closesocket(accSock);
	closesocket(imemgSock);
	closesocket(auxSock);

	printf("Press enter to exit:");
	getchar();	//allow other threads to run

	return 0;
}


