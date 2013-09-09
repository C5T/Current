# Build&run is performed by run.sh, with various options.

all: report.html

report.html: run.sh test.cc autogen/functions.h ../fncas/*.h
	./run.sh >$@

autogen/functions.h: functions/*.h
	mkdir -p autogen
	cat functions/*.h >$@
	ls functions/ | sed "s/\(.*\)\.h/REGISTER_FUNCTION(\1);/g" >>$@

clean:
	rm -rf autogen/
	rm -f test_binary report.html
