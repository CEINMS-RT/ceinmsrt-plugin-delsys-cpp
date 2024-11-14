%module EMGDelsysSwig
%{
#include "EMG_Delsys.h"
%}

%include "std_vector.i"
%include "std_string.i"
%include "std_map.i"

using namespace std;

class EMGDelsys : public ProducersPluginVirtual
{
public:

	EMGDelsys();

	virtual ~EMGDelsys();

	void init(std::string xmlName, std::string executionName);

	void reset();

	const std::map<std::string, double>& GetDataMap();


	const std::set<std::string>& GetNameSet();

	const double& getTime();

	void stop();

	void setDirectories(std::string outDirectory, std::string inDirectory = std::string());

	void setVerbose(int verbose);

	void setRecord(bool record);

	const std::map<std::string, double>& GetDataMapTorque();
};


namespace std {
   %template(vectors) vector<string>;
   %template(vectord) vector<double>;
   %template(vectordd) vector<vector<double> >;
   %template(vectorss) vector<vector<string> >;
   %template(mapsd) std::map<std::string, double>;
};