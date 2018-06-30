CPP=$(wildcard *.cpp)
RUN=$(CPP:%.cpp=%.run)

all: ${RUN}

%.run: %.cpp
	@../run_basic.sh $<
