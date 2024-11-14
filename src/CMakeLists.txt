ADD_LIBRARY( EMG_Delsys SHARED EMG_Delsys.cpp
)

TARGET_LINK_LIBRARIES( EMG_Delsys
	XMLExecutionInterpreter
	XMLEMGInterpreter
	PreprocessingEMG
	FileLogger
	DynLib
	${Boost_LIBRARIES}
	XercesC::XercesC
	ws2_32
)

target_include_directories(EMG_Delsys PUBLIC
	${PROJECT_SOURCE_DIR}/src)

ADD_EXECUTABLE(testPlugin test.cpp
)

TARGET_LINK_LIBRARIES(testPlugin
	EMG_Delsys
)