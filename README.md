a simple personal status provider for bars similar to the one in dwm that just take a string of text either through stdin or xroot window title, started as a fork of an old [slstatus ](https://github.com/drkhsh/slstatus) most of the codebase has been rewritten since.

## info functions and features
- battery percentage
- battery state
- battery time left
- battery smapi info
- cpu frequency
- cpu percentage
- datetime
- disk space left/used/available/percentage
- disk io
- entropy
- fan speed (through ibm fan)
- gid
- hostname
- ip
- load average
- network download
- network upload
- memory used/left/available/percentage
- custom shell command
- swap used/left/available/percentage
- temperature
- uid
- uptime
- username
- volume percentage alsa
- volume percentage pulse
- micvolume percentage pulse
- current pulse profile
- wifi essid
- wifi signal percentage

sstat has a fixed refresh execution time adjusted interval of 1 second, this is done to make it easy to have multiple time sensitive functions such as cpu usage and network upload/download speed. it has been written with minimal memory footprint in mind and can easily be launched to background with sstat -d.

## installing and setting up
1. clone repo
2. `make clean install`
3. edit config.h to your liking
4. go back to step 2

## configuration
config.h will contain a bunch of hopefully helpful explanations of the functions available,  more advanced setup examples(personally configs) are also given in the `config.cate.*` files. the main idea is you define your status similar to how you would construct a typical printf, you provide a format and content in the form of functions to make up you final status string. something to keep in mind `PULSE` has to be defined for any pulse functionality.

## usage
it's suggested you start sstat with `sstat -d` from your startup script or other means

    usage: sstat [option]
    
    options:
        -d start daemonized
        -o print status instead of setting it as rootwindow title
        -v print version info and exit
        -h print this info and exit

## todo
- redo/cleanup pulse implementation
- more system info functions

## bugs and contribution
you can report bugs or make feature requests on the issues page

for contribution, just help fix bugs and send a pr, or do whatever really, if it's useful I'll probably merge it.
