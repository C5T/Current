.PHONY: default indent clean check coverage

default:
	(cd url; make default) && (cd tcp; make default) && (cd http; make default) && (cd api; make default) && echo "ALL TESTS PASS"

indent:
	(find . -name "*.cc" ; find . -name "*.h") | xargs clang-format-3.5 -i

clean:
	rm -rf build .noshit
	(cd url; make clean) && (cd tcp; make clean) && (cd http; make clean) && (cd api; make clean)

check:
	(cd url; make check) && (cd tcp; make check) && (cd http; make check) && (cd api; make check)

coverage:
	(cd url; make coverage) && (cd tcp; make coverage) && (cd http; make coverage) && (cd api; make coverage)
