.PHONY: default indent clean check coverage

default:
	(cd tcp; make default) && (cd http; make default) && echo "ALL TESTS PASS"

indent:
	../../scripts/indent.sh

clean:
	rm -rf build .current
	(cd tcp; make clean) && (cd http; make clean)

check:
	(cd tcp; make check) && (cd http; make check)

coverage:
	(cd tcp; make coverage) && (cd http; make coverage)
