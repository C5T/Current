.PHONY: default indent clean check coverage

default:
	(cd url; make default) && (cd tcp; make default) && (cd http; make default) && (cd api; make default) && echo "ALL TESTS PASS"

indent:
	../../scripts/indent.sh

clean:
	rm -rf build .current
	(cd url; make clean) && (cd tcp; make clean) && (cd http; make clean) && (cd api; make clean)

check:
	(cd url; make check) && (cd tcp; make check) && (cd http; make check) && (cd api; make check)

coverage:
	(cd url; make coverage) && (cd tcp; make coverage) && (cd http; make coverage) && (cd api; make coverage)
