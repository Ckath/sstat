/* see LICENSE file for copyright and license information. */

/* macro used for creating the 'icon' with dwm color patch vars */
#define icon(str) "" str ""

/* text to show if no value can be retrieved */
#define UNKNOWN_STR          "n/a"

/* volume symbols/text, 
 * %i is only needed for VOL_STR */
#define VOL_MUTE_STR         icon("") "mute"
#define VOL_ZERO_STR         icon("") "0%%"
#define VOL_STR              icon("") "%i%%"

/* symbols/text for battery status */
#define BATT_CHARGING_STR    ""
#define BATT_DISCHARGING_STR ""
#define BATT_FULL_STR        ""
#define BATT_UNKNOWN_STR     ""

/* available functions
- battery_perc [argument: battery name]         : battery percentage
- battery_perc_smapi [argument: battery name]   : battery percentage, uses smapi 
- battery_state [argument: battery name]        : battery charging state
- battery_state_smapi [argument: battery name]  : battery charging state, uses smapi 
- battery_time_smapi [argument: battery name]   : time till full/empty, uses smapi 
- cpu_freq [argument: none]                     : cpu frequency in MHz 
- cpu_perc [argument: none]                     : cpu usage in percent 
- datetime [argument: format]                   : date/time (for help 'man strftime')
- disk_free [argument: mountpoint]              : free disk space in GB 
- disk_io [argument: none]                      : active number of I/O operations 
- disk_perc [argument: mountpoint]              : disk usage in percent 
- disk_total [argument: mountpoint]             : total disk space in GB 
- disk_used [argument: mountpoint]              : used disk space in GB 
- entropy [argument: none]                      : available entropy 
- fan_ibm [argument: none]                      : fan speed in rpm 
- gid [argument: none]                          : gid of current user 
- hostname [argument: none]                     : machine hostname
- ip [argument: interface]                      : ip address 
- load_avg [argument: none]                     : load average 
- net_down [argument: network card interface]   : current download in B/s|KB/s|MB/s 
- net_up [argument: network card interface]     : current upload in B/s|KB/s|MB/s 
- ram_free [argument: none]                     : free ram in GB 
- ram_perc [argument: none]                     : ram usage in percent 
- ram_total [argument: none]                    : total ram in GB 
- ram_used [argument: none]                     : used ram in GB 
- run_command [argument: command]               : run custom shell command 
- swap_free [argument: none]                    : free swap in GB 
- swap_perc [argument: none]                    : swap usage in percent 
- swap_total [argument: none]                   : total swap in GB 
- swap_used [argument: none]                    : used swap in GB 
- temp [argument: temperature file]             : temperature in celsius 
- uid [argument: none]                          : uid of current user 
- uptime [argument: none]                       : uptime 
- username [argument: none]                     : username of current user 
- vol_perc [argument: soundcard]                : alsa volume and mute status in percent 
- wifi_essid [argument: wifi card interface]    : wifi essid 
- wifi_perc [argument: none]                    : wifi signal in percent */

/*                                          FORMAT */
#define STATUS_FORMAT \
    "情報%s"                                /* volume */\
    icon("") "%s %s%s"                    /* battery */\
    icon("") "%s %.2s/%.3sGB"            /* disk */\
    icon("") "%s %s %s %s"             /* net */\
    icon("") "%s %2s %.5sGB %s %s"    /* sys */\
    "%s"                                  /* datetime */

/*                                          CONTENT */
#define STATUS_CONTENT \
    vol_perc("hw:0"),                       /* volume */\
    battery_time_smapi("BAT0"),             /* battery */\
    battery_state_smapi("BAT0"),\
    battery_perc_smapi("BAT0"),\
    disk_io(),                              /* disk */\
    disk_used("/"),\
    disk_total("/"),\
    ip("wlp3s0"),                           /* net */\
    wifi_perc(),\
    net_up("wlp3s0"),\
    net_down("wlp3s0"),\
    cpu_freq(),                             /* sys */\
    cpu_perc(),\
    ram_used(),\
    temp("/sys/class/hwmon/hwmon0/temp1_input"),\
    fan_ibm(),\
    datetime("%F %T")                       /* datetime */