## About
**fwinfo** is a simple Xiaomi Mi Walkie-Talkie radios firmware files analysing tool. It displays firmware file sections information; also, with **-f** command line switch, it can **f**ix CRC32, header checksum and data length errors in firmware sections headers, which is very useful during the walkie-talkie firmware mods development.  

## Usage
**fwinfo** is a console program; all the output data are written to stdout.  
Command line is:  
_**fwinfo \<filespec\> \[-f\]**_  
    _**\<filespec\>**_ - firmware file name, for example shark_1.0.4.bin  
    _**-f**_ - optional switch to fix firmware file sections headers errors  
    
## Download
Downloads are available in project [Releases](https://github.com/Mi-Walkie-Talkie-by-Darkhorse/fwinfo/releases) section

## Building
On Linux, use _gcc_ to compile **fwinfo** from .cpp source, for example:  _gcc -O3 fwinfo.cpp -o fwinfo_  
On Windows, use any _Microsoft Visual C++_ version you want.
