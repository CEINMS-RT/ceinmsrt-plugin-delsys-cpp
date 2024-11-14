#include <EMG_Delsys.h>
#include <cstdlib>
#include <csignal>
#include <windows.h>
#include <xercesc/util/PlatformUtils.hpp>

bool endMain;

void SigintHandler(int sig)
{
	endMain = true;
}

int main(void)
{
	endMain = false;
	signal(SIGINT, SigintHandler);

	xercesc::XMLPlatformUtils::Initialize();
	std::cout << "start" << std::endl;
	
	EMGDelsys plugin;
	plugin.setRecord(true);
	plugin.setDirectories("DelsysTest", "DelsysTest");
	plugin.init("C:/Users/thijs/Documents/Master thesis/CEINMS/ceinms-rt/plugin/EMG_Delsys/subjectMTUCalibrated.xml", "C:/Users/thijs/Documents/Master thesis/CEINMS/ceinms-rt/plugin/EMG_Delsys/executionRT.xml");

	Sleep(300);
	std::map<std::string, double> data = plugin.GetDataMap();
	std::cout << plugin.getTime() << " : " << data["tibant_r"] << std::endl; // gaslat_l
	while (!endMain)
	{
		//std::cout << plugin.getTime() << std::endl;
		std::map<std::string, double> data = plugin.GetDataMap();
		std::cout << plugin.getTime() << " : " << data["tibant_r"] << std::endl;
		Sleep(100);
	}
	std::cout << "stop" << std::endl;
	plugin.stop();
	//std::cout << "exit" << std::endl << std::flush;
	return 0;
}