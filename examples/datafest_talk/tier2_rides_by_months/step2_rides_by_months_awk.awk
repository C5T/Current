#!/usr/bin/awk -f

# To run: time cat ../cooked.csv | ./step2_rides_by_months_awk.awk | sort -k 2 | tee >(md5sum)

# Count rides by month, print the counters in an unsorted order of keys.
BEGIN { FS="," }
{ split($2, date_time, " "); split(date_time[1], yyyy_mm_dd, "-"); ++histogram[yyyy_mm_dd[2] "/" yyyy_mm_dd[1]]; }
END { for (i in histogram) printf "%d %s\n", histogram[i], i; }
