## Driver Utility:
This is a hardwareless driver for the linux operating system compiled on the latest stable release of gcc-12.
The hardware is a message slot service where each device created from the driver is a message slot.
Each message slot contains multiple channels (p.o boxes) in which processes can write or read from them.
The module is a character device driver.

## Driver Usage:
Compile the source code using the linux kernel module compiler, alternatively you can use make to produce all the neccessary output files.
Insert the module into the linux kernel and create a device character driver with whatever minor number and the major number 235.
Call message_sender as follows: message_sender "[chrdev location | message slot]" "[channel number]" "[ message ]".
Alternatively you can write any sequence of bytes to the channel in any script.
Call message_reader as foolows: message_reader "[chrdev location | message slot]" "[channel number]"

The maximum supported buffer size that can be stored inside a message channel is 128 bytes.

## Development
The module was written and compiled on UBUNTU LSR with gcc-12.
This was an exercise on driver development for the course "operating systems"
