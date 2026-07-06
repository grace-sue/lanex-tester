build:
	g++ main.cpp iperfExecutor.cpp pingExecutor.cpp interfaceUtils.cpp \
	configureTest.cpp test.cpp serverConfigurationLoader.cpp reportGenerator.cpp -lncurses -pthread -o LAN-EX-Tester
run:
	./LAN-EX-Tester