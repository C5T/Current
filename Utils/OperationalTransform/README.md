# Operational Transform

This utility parses the edit history of a pad encoded as an Operational Transform JSON.

To test it end-to-end, I've created a new CoderPad.io snippet.

* The pad can be accessed via [this link](https://coderpad.io/NPNNAZ2G).
* The `golden/data.ot` file has been crawled form [this URL](https://coderpad.firebaseio.com/NPNNAZ2G/history.json).

Thanks Vincent @vincentwoo for the [reference](https://github.com/firebase/firepad/blob/master/examples/firepad.rb). The code of this utility has been golden-tested against the reference script on a corpus of a thousand++ pads, and the results match 100% except one created-and-fully-erased document, which is an empty file as per this tool's output, and a single `'\n'` according to the reference script.

This tool also comes with a binary to dump the contents of the pad throughout the coding process.

# Examples

```
# Prerequisites:
git clone https://github.com/C5T/Current
cd Current/Utils/OperationalTransform
NDEBUG=1 make
```

## At certain cutoff

Synopsis:
```
./.current/history --input golden/data.ot --at_m 0.25  # At 0.25 minutes == 15 seconds.
```

### Pad
Started 2016/11/10 17:58:27 local time, 2016/11/10 17:58:27 UTC.
### 15s into the code
```
def say_hello():
    print 'Привет, мир!'

say_hello()
```

# At certain intervals

Synopsis:
```
./.current/history --input golden/data.ot --interval_s 15  # Every 15 seconds
```

### Pad
Started 2016/11/10 17:58:27 local time, 2016/11/10 17:58:27 UTC.
### 0s into the code
```
def say_hello():
    print 'Hello, World'

for i in xrange(5):
    say_hello()
```
### 15s into the code
```
def say_hello():
    print 'Привет, мир!'

say_hello()
```
### Result after the total of 23s of coding
```
def say_hello():
    print 'Test passed.'

say_hello()
```
