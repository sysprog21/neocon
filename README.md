# neocon

`neocon` is a simple serial console utility that attempts to open a tty device
on a system until it successfully establishes a connection. Once connected, it
facilitates the transmission of terminal input and output until a read or write
failure occurs on the tty. In such cases, the utility automatically disconnects
and restarts the process.

`neocon` offers several notable advantages over other terminal programs:
* Accessing `/dev/ttyACM0` with `neocon` does not require root privileges.
  Being a member of the `dialout` group should provide sufficient privileges.
* By specifying a delay on keyboard input (as demonstrated below),
  `neocon` enables you to paste commands directly from the clipboard.
* `neocon` can be launched even when `/dev/ttyACM0` is unavailable. It will
  automatically establish a connection as soon as the device becomes ready.

The main feature of neocon is its ability to accept a list of devices and
automatically select a functional one. e.g.,
```shell
$ neocon /dev/ttyUSB{0,1,2}
```

## Options

* `-b` baud\_rate
  - Set the TTY to the specified bit rate
* `-t` delay\_ms
  - This option throttles keyboard input to a rate of one character every
    delay\_ms milliseconds. It can be used to prevent buffer overruns on the
    remote end.
* `-l` logfile
  - Specifies a file for logging purposes. Non-ASCII and non-printable
    characters are converted to hash signs (`#`). To append to an existing
    logfile, include the `-a` option. To add a timestamp before each line,
    use the `-T` option.
* `-a`
  - Append to the log file if it already exists.
* `-e` escape
  - Set the escape character (default: `~`).
* `-T`
  - Add timestamps to the log file.

## Usage

### Exiting `neocon`

To exit the `neocon`, simply type `~.` (tilde followed by a dot).
The escape character (`~`) can be changed using the `-e` option.

### Switching to the next device

To manually switch to the next available device, enter `~n` (tilde followed by
the letter `n`).

## Known issues
- The escape character is sent to the target.
