CPP=$(wildcard *.cpp)
RUN=$(CPP:%.cpp=%.run)

all: ${RUN}

current:
	ln -sf ../../../../ $@

%.run: %.cpp current
	@../run_basic.sh $<
