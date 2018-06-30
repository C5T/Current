#!/usr/bin/python

# To run: time ./step1_cook.py | pv -l > ../cooked.csv
#
# Cooks the dataset by changing the value of each of the two date columns -- the pickup time and the dropoff time --
# into their expanded format, which includes the Unix epoch seconds and the date of week in the New York time zone.
#
# TL;DR: The input timestamps without the EST/EDT annotation are ambiguous, and Python, as it turns out, is the
# easiest way to create order of this chaos. For results reproducibility purposes, this cooking process is just fine.

import sys, datetime, pytz

column_pickup = 1
column_dropoff = 2
column_total_count = 23

unix_epoch_begin = datetime.datetime(1970, 1, 1, tzinfo=pytz.utc)
new_york_timezone = pytz.timezone('US/Eastern')  # Leave the daylight saving time guessing game to Python.

def parse_datetime(datetime_as_string):
  date_time = datetime_as_string.split(' ')
  assert len(date_time) == 3
  mm_dd_yyyy = date_time[0].split('/')
  hh_mm_ss = date_time[1].split(':')
  ampm_adjusted_hour = (int(hh_mm_ss[0]) % 12) + (12 if date_time[2] == 'PM' else 0)
  return new_york_timezone.localize(datetime.datetime(int(mm_dd_yyyy[2]),
                                                      int(mm_dd_yyyy[0]),
                                                      int(mm_dd_yyyy[1]),
                                                      ampm_adjusted_hour,
                                                      int(hh_mm_ss[1]),
                                                      int(hh_mm_ss[2])))

line_index = 0
with open(sys.argv[1] if 1 < len(sys.argv) else '../data.csv') as f:
  for line in f:
    columns = line.split(',')
    assert len(columns) == column_total_count
    line_index = line_index + 1
    if line_index > 1:
      for column in [column_pickup, column_dropoff]:
        d = parse_datetime(columns[column])
        columns[column] = (d.strftime('%Y-%m-%d-%H-%M-%S-%Z-%a-') + str(int((d - unix_epoch_begin).total_seconds())))
      print ','.join(columns).strip()
