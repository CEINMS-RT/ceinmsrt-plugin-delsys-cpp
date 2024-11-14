#ifndef EMGDELSYS_H_
#define EMGDELSYS_H_

#include <stdio.h>
#include <string>

#include "execution.hxx"
#include "ExecutionEmgXml.h"
#include "ProducersPluginVirtual.h"
#include "NMSmodel.hxx"
#include <Filter.h>
#include "stdafx.h" //Delsys SDK
#include <chrono>
#include <ctime>
#include "OpenSimFileLogger.h"
#include "EMGPreProcessing.h"
#include <thread>
#include <mutex>
#include <iomanip>
#include <condition_variable>
namespace timeEMG { // there already a getime function here
#include <GetTime.h>
};

#ifdef WIN32

class __declspec(dllexport) EMGDelsys : public ProducersPluginVirtual
#endif
#if UNIX
class EMGDelsys : public ProducersPluginVirtual
#endif
{
public:
	/**
	* Constructor
	*/
	EMGDelsys();

	/**
	* Destructor
	*/
	virtual ~EMGDelsys();

	/**
	* Initialization method
	* @param xmlName Subject specific XML
	* @param executionName execution XML for CEINMS-RT software configuration
	*/
	void init(std::string xmlName, std::string executionName);

	void reset()
	{
	}

	/**
	* Get the data with the name of the EMG channel mapping the EMG.
	* The correspondence between the channel and the muscle are in the subject specific XML.
	*/
	const std::map<std::string, double>& GetDataMap();

	/**
	* Get a set of the channel name
	*/
	const std::set<std::string>& GetNameSet()
	{
		return nameSet_;
	}

	/**
	* Get the time stamp of the EMG capture.
	*/
	const double& getTime()
	{
		testConnect();
		DataMutex_.lock();
		if(cptEMGFilt_ > 0)
			timeSafe_ = timeStampEMGFilt_[cptEMGFilt_];
		DataMutex_.unlock();
		return timeSafe_;
	}

	void stop();

	void setDirectories(std::string outDirectory, std::string inDirectory = std::string())
	{
		_outDirectory = outDirectory;
		_inDirectory = inDirectory;
	}

	void setVerbose(int verbose)
	{
	}

	void setRecord(bool record)
	{
		_record = false; // we already log EMG in the main loop so the plugin shouldn't log anymore
	}

	const std::map<std::string, double>& GetDataMapTorque()
	{
		return _torque;
	}

protected:

	void filterEMG();
	void EMGFeed();
	void testConnect()
	{
		if (_connect == false)
		{
			std::cout << "Not connected to Delsys server !" << std::endl << std::flush;
			exit(EXIT_FAILURE);
		}
	}

	//Send a command to the server and wait for the response
	void SendCommandWithResponse(char * command, char * response);

	std::map<std::string, double> mapData_; //!< Map between the name of the channel and the EMG
	std::set<std::string> nameSet_; //!< Set of the name of the channel
	std::vector<std::string> nameVect_; //!< Vector of channel names
	unsigned short emgCpt_; //!< index in the data packet being read in the GetDataMap() method
	double time_; //!< Time stamp of the EMG
	double timeSafe_; //!< Thread safe time stamp for getTime() method
	double timeInit_;
	std::vector<EMGPreProcessing*> emgPreProcessingVect_; //!< Vector of class for the EMG pre-processing

	std::thread* filterThread; //!< Boost thread for the filtering of the data
	std::thread* feederThread; //!< Boost thread for the filtering of the data

	std::mutex DataMutex_; //!< Mutex for the data
	std::mutex EMGMutex_; //!< Mutex for the Raw EMG data
	std::mutex timeInitMutex_; //!< Mutex for the Raw EMG data
	std::mutex loggerMutex_; //!< Mutex for the Raw EMG data
	std::condition_variable EMGReady; //!< condition for filter EMG

	std::map<std::string, double> _torque;

	char sensortype_[512]; // sensor type
	std::vector< std::vector<double> > emgVector_; //!<vector to exchange data between emg filter and emg feeder
	bool threadEnd_;
	bool newData_;

	std::string _outDirectory;
	std::string _inDirectory;

	bool _record;
	bool _connect;

	std::vector<double> dataEMG_;
	std::vector< std::vector<double> > dataFilt_;
	std::vector<double> timeStampEMGFilt_;
	int cptEMGFilt_;
	double timenow_;

	std::vector<std::vector<double> > dataIMU_;
	std::vector<std::vector<double> > dataIMUSafe_;
	std::string ip;
	std::string port;

	OpenSimFileLogger<int>* _logger;

	ExecutionEmgXml* _executionEmgXml;

	SOCKET commSock;	//command socket
	SOCKET emgSock;		//EMG socket
	SOCKET accSock;		//ACC socket
	SOCKET imemgSock;   //IM EMG socket
	SOCKET auxSock;  //IM AUX socket
};

#endif