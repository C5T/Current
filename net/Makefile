.PHONY: test indent clean check coverage

test:
	(cd tcp; make test) && (cd http; make test) && (cd api; make test) && echo "ALL TESTS PASS"

indent:
	(find . -name "*.cc" ; find . -name "*.h") | xargs clang-format-3.5 -i

clean:
	(cd tcp; make clean) && (cd http; make clean) && (cd api; make clean)

check:
	(cd tcp; make check) && (cd http; make check) && (cd api; make check)

coverage:
	(cd tcp; make coverage) && (cd http; make coverage) && (cd api; make coverage)
