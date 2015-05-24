all: test

test: report.html

report.html: ../eval.cc ../function.h ../f/*.h
	./run_perf_test.sh >$@

clean:
	rm -rf report.html autogen/
