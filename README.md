# idler

Displays the name of idle machine(s)

# Synopsis

```
idler [ OPTIONS ] HOSTNAME [ HOSTNAME ... ]
```

# Description

*idler* uses RPC calls to determine the current, 5 minute average and 15
minute average load of machines specified on the command line. It then
displays the name and optionally the load average of the least (or
most) loaded machines. (If there are several machines which each have
the same lowest load average, *idler* will randomly choose among them.)

# Example
```
$ idler host1 host2 host3
host2
```

# Options

* **-n***NUMBER*
  Display the number machines with the lowest load average. By
  default, idler displays only a single machine's name.
* **-t***SECONDS*
  Sets the timeout for remote procedure calls to seconds seconds.
  The default value is 10 seconds.
* **-s***N*
  Use the Nth load average value for sorting, where n may be either
  1, 2 or 3, representing the current, five minute or fifteen minute
  averages. By default, idler uses the current load average.
* **-l**
  Display the actual load averages. By default, idler displays
  only the name(s) of the selected machine(s).
* **-r**
  Reverse the sense of the sort, so that idler shows the most
  loaded machines instead of the least loaded machines.

# Author

Phil Spector <spector@edithst.com>
