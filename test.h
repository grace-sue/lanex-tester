#ifndef TEST_H
#define TEST_H
#include "configureTest.h"
#include "interfaceUtils.h"
#include "testData.h"
#include "iperfExecutor.h"
#include "serverConfigurationLoader.h"
#include "pingExecutor.h"
namespace LANEXTest {
    
    bool startTest(ConfigureTest::testConfiguration *tc, 
                ServerConfigurationLoader::ServerConfiguration *serverConf,
                testData &testData);
};
#endif