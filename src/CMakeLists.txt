ADD_LIBRARY( EMG_Delsys SHARED EMG_Delsys.cpp
)


TARGET_LINK_LIBRARIES( EMG_Delsys
	xmlInput
	emg_preprocessing
	EmgXml
	FileIO
	ComsumerPlugin
	${Boost_LIBRARIES}
	XercesC::XercesC
	${WINLIB}
	${WINMM}
)


ADD_EXECUTABLE(testPlugin test.cpp
)

TARGET_LINK_LIBRARIES(testPlugin
	EMG_Delsys
)