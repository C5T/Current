#!/usr/bin/python

# To run: time ./step1_rides_by_months_python.py | tee >(md5sum)
#
# Count rides by month, print the counters in the lexicographically sorted order of keys.
# The baseline Python implementation that assumes the YYYY and MM are stored in certain format in certain column.

histogram = {}

with open('../cooked.csv') as f:
  for line in f:
    date = line.split(',')[1]
    yyyy_mm_dd = date.split('-')
    yyyy = yyyy_mm_dd[0]
    mm = yyyy_mm_dd[1]
    histogram_key = mm + '/' + yyyy
    if not histogram_key in histogram:
      histogram[histogram_key] = 1
    else:
      histogram[histogram_key] += 1

for mm_yyyy, count in sorted(histogram.iteritems()):
  print "%s %s" % (count, mm_yyyy)
