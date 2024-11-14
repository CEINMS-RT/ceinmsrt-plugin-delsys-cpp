#include <EMG_Delsys.h>

EMGDelsys::EMGDelsys()
{
	_record = false;
	_connect = false;
	newData_ = false;
	cptEMGFilt_ = 0;
}

EMGDelsys::~EMGDelsys()
{
	delete _executionEmgXml;
	delete filterThread;
	delete feederThread;

	//delete pointer to filter class
	for (std::vector<EMGPreProcessing*>::iterator it = emgPreProcessingVect_.begin(); it != emgPreProcessingVect_.end(); it++)
		delete *it;
}

void EMGDelsys::init(std::string xmlName, std::string executionName)
{
	// Default port and IP
	ip = "192.168.56.1";
	port = "31000";

	//If we save the data create logger class
	if (_record)
		_logger = new OpenSimFileLogger<int>(_outDirectory);
	
	//pointer to subject XML
	std::auto_ptr<NMSmodelType> subjectPointer;

	try
	{
		subjectPointer = std::auto_ptr<NMSmodelType>(subject(xmlName, xml_schema::flags::dont_initialize));
	}
	catch (const xml_schema::exception& e)
	{
		cout << e << endl;
		exit(EXIT_FAILURE);
	}

	NMSmodelType::Channels_type& channels(subjectPointer->Channels());
	ChannelsType::Channel_sequence& channelSequence(channels.Channel());

	std::cout << "Reading Execution XML from: " << executionName << std::endl;

	// Configuration XML
	std::auto_ptr<ExecutionType> executionPointer;

	try
	{
		std::auto_ptr<ExecutionType> temp(execution(executionName, xml_schema::flags::dont_initialize));
		executionPointer = temp;
	}
	catch (const xml_schema::exception& e)
	{
		cout << e << endl;
		exit(EXIT_FAILURE);
	}

	// Get the EMG XML configuration
	const std::string& EMGFile = executionPointer->ConsumerPlugin().EMGDeviceFile().get();

	// EMG XML class reader
	_executionEmgXml = new ExecutionEmgXml(EMGFile);

	//Ip and Port from the EMG xml
	ip = _executionEmgXml->getIP();
	port = _executionEmgXml->getPort();

	// Get filter parameters
	const std::vector<double>& aCoeffHP = _executionEmgXml->getACoeffHP();
	const std::vector<double>& bCoeffHP = _executionEmgXml->getBCoeffHP();
	const std::vector<double>& aCoeffLP = _executionEmgXml->getACoeffLP();
	const std::vector<double>& bCoeffLP = _executionEmgXml->getBCoeffLP();

	if (aCoeffHP.size() == 0 || bCoeffHP.size() == 0 || aCoeffLP.size() == 0 || bCoeffLP.size() == 0)
	{
		std::cout << "Error: Filter coefficients size is zero." << std::endl;
		exit(EXIT_FAILURE);
	}

	// Get Max EMG for normalization
	std::vector<double> maxAmp = _executionEmgXml->getMaxEmg();
	//std::cout << "Number of elements in maxAmp: " <<  maxAmp.size() << std::endl;
	//if (maxAmp.size() < 16)
	//{
	//	std::cout << "Number of Max amplitude EMG < 16 rewrite 16 to 0." << std::endl;
	//	static const double arr[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	//	maxAmp = vector<double>(arr, arr + sizeof(arr) / sizeof(arr[0]));
	//}
	
	//Get the EMG channel from the subject xml
	for (ChannelsType::Channel_iterator it = channelSequence.begin(); it != channelSequence.end(); it++)
	{
		//get name channel
		nameSet_.insert(it->name());
		nameVect_.push_back(it->name());

		//Create the filter class
		emgPreProcessingVect_.push_back(
			new EMGPreProcessing(aCoeffLP, bCoeffLP, aCoeffHP, bCoeffHP,
			maxAmp[std::distance<ChannelsType::Channel_iterator>(channelSequence.begin(), it)]));
	// std::cout << "std::distance<ChannelsType::Channel_iterator>(channelSequence.begin(), it)]): " << std::distance<ChannelsType::Channel_iterator>(channelSequence.begin(), it) << std::endl;
	// std::distance calculates the number of elements between first and last. Then, this is basically taking the maxAmp for a specific channel (distance between channelSequence.begin() and it).
	}
	//std::cout << "emgPreProcessingVect size: " << emgPreProcessingVect_.size() << std::endl;

	//Create file for saving data
	if (_record)
	{
		_logger->addLog(Logger::Emgs, nameVect_);
		_logger->addLog(Logger::EmgsFilter, nameVect_);
	}

	// Bool to stop thread
	threadEnd_ = true;

	//  thread for getting the data from the delsys trigno control software
	feederThread = new std::thread(&EMGDelsys::EMGFeed, this);

	// thread for the filtering of the data 
	filterThread = new std::thread(&EMGDelsys::filterEMG, this);

	std::cout << "Wait 2 sec for connecting to the delsys system ..." << std::endl; 
	std::this_thread::sleep_for(std::chrono::seconds(10)); // This line is needed because CEINMS-RT will call plugin.getTime(), and the vectors used there are not initialized if we don't wait. 
}

void EMGDelsys::stop()
{
	//std::cout << "Stop " << nameVect_.size() << std::endl;
	std::vector<double> emgMax;
	for (std::vector<std::string>::const_iterator it = nameVect_.begin(); it != nameVect_.end(); it++)
	{
		emgMax.push_back(emgPreProcessingVect_[std::distance<std::vector<std::string>::const_iterator>(
			nameVect_.begin(), it)]->getMax());
		//std::cout << emgMax.back() << std::endl;
		//delete emgPreProcessingVect_[std::distance<std::vector<std::string>::const_iterator>(nameVect_.begin(), it)];
	}
	//std::cout << emgMax.size() << std::endl;
	_executionEmgXml->setMaxEmg(emgMax);
	_executionEmgXml->UpdateEmgXmlFile();
	//delete _executionEmgXml;
	std::cout << "EMGmax set to file" << std::endl;

	threadEnd_ = false;
	filterThread->join();
	//feederThread->join();
	std::this_thread::sleep_for(std::chrono::seconds(1)); // somehow joining the feederThread doesn't work so we just wait for 1 second so the thread can end 

	if (_record)
	{
		_logger->stop();
		delete _logger;
	}
}

void EMGDelsys::EMGFeed()
{
	//char response[32];	//buffer for response
	char tmp[128];	//string buffer for various uses
	int collectionTime;	//time to collect data in samples (seconds * 2000)
	int sensor = emgPreProcessingVect_.size();	//sensor to save data
	//int sensor = 16;

	if (sensor < 1 || sensor > 16)
	{
		std::cout << " The number of EMG has to be between 1 and 16." << std::endl;
		exit(EXIT_FAILURE);
	}

#ifdef WIN32
	WSADATA wsaData;	//Windows socket data
	int iResult;	//result of call
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		std::cout << "WSAStartup failed: " << iResult << std::endl;
		exit(EXIT_FAILURE);	//could not open Windows sockets library
	}
#endif
	struct addrinfo aiHints;
	struct addrinfo *aiList = NULL;
	int retVal;

	// Setup the hints address info structure
	// which is passed to the getaddrinfo() function
	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_INET;
	aiHints.ai_socktype = SOCK_STREAM;
	aiHints.ai_protocol = IPPROTO_TCP;

	std::cout << "Delsys system IP: " << ip << std::endl;
	// Call getaddrinfo(). If the call succeeds,
	// the aiList variable will hold a linked list
	// of addrinfo structures containing response
	// information about the host

	if ((retVal = getaddrinfo(ip.c_str(), port.c_str(), &aiHints, &aiList)) != 0)
	{
		std::cout << "Invalid URL." << std::endl;
		exit(EXIT_FAILURE);	//could not resolve URL
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
		std::cout << "Can't connect to Trigno Server!" << std::endl << std::flush;
		exit(EXIT_FAILURE);	//server is not responding
	}
	else
		std::cout << "Connected to Trigno Server." << std::endl << std::flush;
	
	int n;	//byte count
	//Get response from the server after connecting
	n = recv(commSock, tmp, sizeof(tmp), 0);
	tmp[n - 2] = 0;	//back up over second CR/LF
	printf(tmp);	//display to user

	// Set up the connection to server for EMG data
	emgSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sinRemote.sin_port = htons(50041);
	if (connect(emgSock, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		std::cout << "Error initializing Trigno EMG data connection (can't connect)." << std::endl << std::flush;
		exit(EXIT_FAILURE);
	}

	// Set up the connection to server for ACC data
	accSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sinRemote.sin_port = htons(50042);
	if (connect(accSock, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		std::cout << "Error initializing Trigno ACC data connection (can't connect)." << std::endl << std::flush;
		exit(EXIT_FAILURE);
	}

	//Set up the connection to server for IM EMG data
	imemgSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sinRemote.sin_port = htons(50043);
	if (connect(imemgSock, (sockaddr*)&sinRemote, sizeof(sockaddr_in)) == SOCKET_ERROR)
	{
		std::cout << "Error initializing Trigno IM sensor EMG data connection (can't connect)." << std::endl << std::flush;
		exit(EXIT_FAILURE);
	}

	//Query for start/stop triggers and send to the server
	SendCommandWithResponse("TRIGGER START OFF\r\n\r\n", tmp);

	SendCommandWithResponse("TRIGGER STOP OFF\r\n\r\n", tmp);

	std::cout << "Delsys sensor Type: " << std::flush;

	//Get the type of the sensor
	for (int i = 0; i != sensor; i++) // Crash at the end
	{
		sprintf(tmp, "SENSOR %d TYPE?\r\n\r\n", i + 1);
		SendCommandWithResponse(tmp, &sensortype_[i]);
		std::cout << sensortype_[i];
	}
	std::cout << std::endl;

	//Send command to start data acquisition
	SendCommandWithResponse("UPSAMPLE OFF\r\n\r\n", tmp);
	SendCommandWithResponse("START\r\n\r\n", tmp);

	//If we are connected CEINMS does not have to wait anymore
	_connect = true;

	int k;	//byte count
	const int nSensors = 16;
	char emgbuf[sizeof(float) * nSensors];		//buffer for one sample from each sensor
	int sizeEMGbuf = sizeof(float) * nSensors;

	unsigned long bytesRead;	//bytes available for reading
	//double frq = 0;
	//long compt = 0;
	double timeInitCpy;
	std::vector<float> tempEMG;
	tempEMG.resize(sensor);
	
	while (threadEnd_)
	{
		std::vector< std::vector<double> > emgVectorTemp;
		//Get the time of the new packet
		timeInitMutex_.lock();
		timenow_ = rtb::getTime();
		timeInitCpy = timenow_;
		timeInitMutex_.unlock();

		//See if we have enough data
		ioctlsocket(emgSock, FIONREAD, &bytesRead);	//peek at data available

		while (bytesRead >= sizeof(emgbuf))
		{
			k = recv(emgSock, emgbuf, sizeEMGbuf, 0);
			//process data
			memcpy(tempEMG.data(), emgbuf, sizeEMGbuf);
			bytesRead = bytesRead - sizeEMGbuf;
			tempEMG.resize(8);

			emgVectorTemp.push_back(std::vector<double>(tempEMG.begin(), tempEMG.end()));
		}
		
		//Give the data to the thread filter and notify new data
		EMGMutex_.lock();
		emgVector_ = emgVectorTemp;
		EMGMutex_.unlock();
		EMGReady.notify_one(); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		std::vector<double> timeVector;
		
		//Compute time stamp for every raw sample EMG
		for (int cpt = 0; cpt < emgVectorTemp.size(); cpt++)
		{
			timeVector.push_back(timeInitCpy - (1.0 / 2000.0) * (emgVectorTemp.size() - cpt - 1)); // To understand this formula just think that the samles are taken in a specific time sample but they were generated in the past
		}

		//Save sta to file
		if (_record)
		{
			loggerMutex_.lock();
			for (int cpt = 0; cpt < emgVectorTemp.size(); cpt++)
			{
				_logger->log(Logger::Emgs, timeVector[cpt], emgVectorTemp[cpt]);
			}
			loggerMutex_.unlock();
		}

		//Look for stop from server
		ioctlsocket(commSock, FIONREAD, &bytesRead);
		if (bytesRead > 0)
		{
			recv(commSock, tmp, bytesRead, 0);
			tmp[bytesRead] = '\0';
			if (strstr(tmp, "STOPPED\r\n") != NULL)
				threadEnd_ = false;
		}

	}

	EMGReady.notify_one();

	//Stop data collection and disconnect
	SendCommandWithResponse("QUIT\r\n\r\n", tmp);
	closesocket(commSock);
	closesocket(emgSock);
	closesocket(accSock);
	closesocket(imemgSock);
	closesocket(auxSock);
	WSACleanup();
}

void EMGDelsys::filterEMG()
{
	std::vector< std::vector<double> > emgVectorFiltCpy; // COMMENT HERE WHAT THIS IS FOR
	std::vector<double> timeCpy; // COMMENT HERE WHAT THIS IS FOR
	while (threadEnd_)
	{
		//Wait foir new data
		std::unique_lock<std::mutex> lk(EMGMutex_);
		//std::cout << "Keeps waiting? " << std::endl;
		EMGReady.wait(lk);
		// Can we add a reasonable timeout?

		//Get the data from EMGFeeder thread
		std::vector< std::vector<double> > emgVectorTemp = emgVector_;
		
		// Vector for filtered data
		std::vector< std::vector<double> > emgVectorFilt;

		//std::cout << emgVectorTemp.size() << std::endl;

		//make sure we have data
		if (emgVectorTemp.size() != 0)
		{
			// variable for dividing Fs by 2 (2000Hz to 1000Hz)
			int cpt = 0;

			//Loop on all data (every package element of emgVectorTemp has the size of emg buffer)
			for (std::vector< std::vector<double> >::const_iterator it1 = emgVectorTemp.begin(); it1 != emgVectorTemp.end(); it1++)
			{
				//if data is not pair do nothing (divide freq by 2)
				if (cpt%2 == 0) 
				{
					// temp data for vector of electrode data (16 electrodes with delsys)
					std::vector<double> tempFilt;

					int cnt = 0; // Counter to avoid out of range. Electrodes (regardless the number) has to be placed in the same order as the muscles listed in MTU calibrated. 
					// The code will always have 16 columns. I think it is better to have the MTU calibrated file for 16 muscles. 
					// The first electrode (1, 2, 3 or whatever number) will always record the tibialis anterior. 
					// Loop on the electrodes
					// However, if we want the labels to be correct, we have to put them in order. There is a 1:1 relation between muscle and number of electrode 
					// due to the header of the sto files. 
					for (std::vector<double>::const_iterator it2 = it1->begin(); it2 != it1->end(); it2++)
					{

						int cpt = std::distance<std::vector<double>::const_iterator>(it1->begin(), it2);
						//if sensor type is I EMG = 0 (sensor Off)
						if (sensortype_[cpt] != 'I'){ // check sign for empty sensor
							tempFilt.push_back(emgPreProcessingVect_[cnt]->computeData(*it2)); // THIS IS THE FILTERING + RECTIFICATION + NORMALIZATION
							cnt++;
						}
						else
							tempFilt.push_back(0);
					}
					emgVectorFilt.push_back(tempFilt);
				}
				cpt++;
			}
		}

		//if we have filtered data
		if (emgVectorFilt.size() != 0)
		{
			std::vector<double> timeVector;

			//Give time of EMG
			timeInitMutex_.lock();
			double timeInitCpy = timenow_;
			
			timeInitMutex_.unlock();

			//Compute timestamp for all EMG filtered sample
			for (int cpt = 0; cpt < emgVectorFilt.size(); cpt++)
			{
				timeVector.push_back(timeInitCpy - (1.0 / 1000.0) * (emgVectorFilt.size() - cpt - 1));
			}
			timeCpy = timeVector;

			//Give the value to other thread (Mutex)
			DataMutex_.lock();
			cptEMGFilt_ = emgVectorFilt.size() - 1;//0				

			dataFilt_ = emgVectorFilt;

			//new data availible
			newData_ = true;
			
			//update time
			timeStampEMGFilt_ = timeVector;
			DataMutex_.unlock();

			

			//Save data to file
			if (_record)
			{
				loggerMutex_.lock();
				for (int cpt = 0; cpt < emgVectorFilt.size(); cpt++)
				{
					_logger->log(Logger::EmgsFilter, timeVector[cpt], emgVectorFilt[cpt]);
				}
				loggerMutex_.unlock();
			}
		}
	}
}

const std::map<std::string, double>& EMGDelsys::GetDataMap()
{
	testConnect();
	// Get the data
	std::vector<double> dataEMGSafe;
	DataMutex_.lock();

	//if we do not have data send 0
	if (newData_)
	{
		dataEMGSafe = dataFilt_[cptEMGFilt_];
		//std::cout << "dataEMGSafe: " << dataEMGSafe.size() << " | " << "nameVect_: " << nameVect_.size() << std::endl;
		if (cptEMGFilt_ < dataFilt_.size() - 1)
			cptEMGFilt_++;
	}
	else
	{
		DataMutex_.unlock();
		for (std::vector<std::string>::const_iterator it = nameVect_.begin(); it != nameVect_.end(); it++)
		{
			mapData_[*it] = 0;
		}
		return mapData_;
	}
	DataMutex_.unlock();
	
	// Fill the map
	for (std::vector<std::string>::const_iterator it = nameVect_.begin(); it != nameVect_.end(); it++)
	{
		mapData_[*it] = dataEMGSafe[std::distance<std::vector<std::string>::const_iterator>(nameVect_.begin(), it)];
	}
	return mapData_;
}

//Send a command to the server and wait for the response
void EMGDelsys::SendCommandWithResponse(char * command, char * response)
{
	int n;
	char tmp[128];

	strcpy(tmp, command);
	tmp[strlen(command) - 2] = 0;	//trip last CR/LF
	//printf("Command: %s", tmp);	//display to user

	if (send(commSock, command, strlen(command), 0) == SOCKET_ERROR)
	{
		printf("Can't send command.\n");
		_exit(1);	//bail out on error
	}

	//wait for response
	//note - this implementation assumes that the entire response is in one TCP packet
	n = recv(commSock, tmp, sizeof(tmp), 0);
	tmp[n - 2] = 0;	//strip last CR/LF
	//printf("Response: %s", tmp);	//display to user
	strcpy(response, tmp);	//return the response line

	return;
}

#ifdef UNIX
extern "C" ProducersPluginVirtual* create()
{
	return new EMGDelsys;
}

extern "C" void destroy(ProducersPluginVirtual* p)
{
	delete p;
}
#endif
#ifdef WIN32 // __declspec (dllexport) id important for dynamic loading
extern "C" __declspec (dllexport) ProducersPluginVirtual* __cdecl create() {
	return new EMGDelsys;
}

extern "C" __declspec (dllexport) void __cdecl destroy(ProducersPluginVirtual* p) {
	delete p;
}
#endif