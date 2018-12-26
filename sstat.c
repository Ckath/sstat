/* see LICENSE file for copyright and license information. */

#include <alsa/asoundlib.h>
#include <err.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/wireless.h>
#include <netdb.h>
#include <pulse/pulseaudio.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <X11/Xlib.h>

typedef enum { STDOUT, XROOT } output;

static char *battery_perc(const char *bat);
static char *battery_perc_smapi(const char *bat);
static char *battery_state(const char *bat);
static char *battery_state_smapi(const char *bat);
static char *battery_time_smapi(const char *bat);
static char *cpu_freq(void);
static char *cpu_perc(long double ps_old[4]);
static char *datetime(const char *fmt);
static char *disk_free(const char *mnt);
static char *disk_io(void);
static char *disk_perc(const char *mnt);
static char *disk_total(const char *mnt);
static char *disk_used(const char *mnt);
static char *entropy(void);
static char *fan_ibm(void);
static char *gid(void);
static char *hostname(void);
static char *ip(const char *iface);
static char *load_avg(void);
static char *net_down(double *rx_old, const char *iface);
static char *net_up(double *tx_old, const char *iface);
static char *ram_free(void);
static char *ram_perc(void);
static char *ram_total(void);
static char *ram_used(void);
static char *run_command(const char *cmd);
static char *swap_free(void);
static char *swap_perc(void);
static char *swap_total(void);
static char *swap_used(void);
static char *temp(const char *file);
static char *uid(void);
static char *uptime(void);
static char *username(void);
static char *vol_perc_alsa(const char *card);
static char *wifi_essid(const char *iface);
static char *wifi_perc(void);
static void sighandler(const int signo);
static void update_status(output dest, const char *str);

#include "config.h"

static unsigned short int done;
static Display *display;

/* pulse garbage */
#ifdef PULSE
static char *pulse_profile(void);
static char *pulse_profile_icon(void);
static char *vol_perc_pulse(void);
static char *micvol_perc_pulse(void);
static void pulse_context_state_cb(pa_context *c, void *userdata);
static void pulse_sink_info_cb(pa_context *c, const pa_sink_info *sink_info, int eol, void *userdata);
static void pulse_source_info_cb(pa_context *c, const pa_source_info *source_info, int eol, void *userdata);
static void pulse_volume_change_cb(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata);

static char pulse_vol_str[80] = UNKNOWN_STR;
static char pulse_micvol_str[80] = UNKNOWN_STR;
static  char pulse_profile_str[80] = UNKNOWN_STR;
#endif

#define RETURN_FORMAT(len, format, ...)\
    static char ret_str[len];\
    sprintf(ret_str, format, ##__VA_ARGS__);\
    return ret_str;

static char *
battery_perc(const char *bat)
{
    char path[50];
    int perc;
    FILE *fp;

    sprintf(path, "/sys/class/power_supply/%s/capacity", bat);
    fp = fopen(path, "r");
    if (fp == NULL) {
        warn("Failed to open file %s", path);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%i", &perc);
    fclose(fp);

    RETURN_FORMAT(5, "%d%%", perc)
}

static char *
battery_state(const char *bat)
{
    char path[40];
    char state[12];
    FILE *fp;

    sprintf(path, "/sys/class/power_supply/%s/status", bat);
    fp = fopen(path, "r");
    if (fp == NULL) {
        warn("Failed to open file %s", path);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%11s", state);
    fclose(fp);

    if (!strcmp(state, "Charging")) {
        RETURN_FORMAT(20, BATT_CHARGING_STR);
    } else if (!strcmp(state, "Discharging")) {
        RETURN_FORMAT(20, BATT_DISCHARGING_STR);
    } else if (!strcmp(state, "Full")) {
        RETURN_FORMAT(20, BATT_FULL_STR);
    } else {
        RETURN_FORMAT(20, BATT_UNKNOWN_STR);
    }
}

static char *
battery_perc_smapi(const char *bat)
{
    char path[60];
    int perc;
    FILE *fp;

    sprintf(path, "/sys/devices/platform/smapi/%s/remaining_percent", bat);
    fp = fopen(path, "r");
    if (fp == NULL) {
        warn("Failed to open file %s", path);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%i", &perc);
    fclose(fp);

    RETURN_FORMAT(5, "%d%%", perc);
}

static char *
battery_state_smapi(const char *bat)
{
    char path[50];
    char state[12];
    FILE *fp;

    sprintf(path, "/sys/devices/platform/smapi/%s/state", bat);
    fp = fopen(path, "r");
    if (fp == NULL) {
        warn("Failed to open file %s", path);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%11s", state);
    fclose(fp);

    if (!strcmp(state, "charging")) {
        RETURN_FORMAT(20, BATT_CHARGING_STR);
    } else if (!strcmp(state, "discharging")) {
        RETURN_FORMAT(20, BATT_DISCHARGING_STR);
    } else if (!strcmp(state, "idle")) {
        RETURN_FORMAT(20, BATT_FULL_STR);
    } else {
        RETURN_FORMAT(20, BATT_UNKNOWN_STR);
    }
}

static char *
battery_time_smapi(const char *bat)
{
    char path[70];
    int time = -1;
    FILE *fp;

    sprintf(path, "/sys/devices/platform/smapi/%s/remaining_running_time_now", bat);
    fp = fopen(path, "r");
    if (fp == NULL) {
        warn("Failed to open file %s", path);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%d", &time);
    fclose(fp);

    if(time == -1) {
        sprintf(path, "/sys/devices/platform/smapi/%s/remaining_charging_time", bat);
        fp = fopen(path, "r");
        if (fp == NULL) {
            warn("Failed to open file %s", path);
            RETURN_FORMAT(10, UNKNOWN_STR);
        }
        fscanf(fp, "%d", &time);
        fclose(fp);
    }

    if (time == -1) {
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    RETURN_FORMAT(20, "%02d:%02d", time/60, time%60);
}

static char *
cpu_freq(void)
{
    int freq;
    FILE *fp;

    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (fp == NULL) {
        warn("Failed to open file /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%d", &freq);
    fclose(fp);

    RETURN_FORMAT(20, "%4dMHz", freq/1000);
}

static char *
cpu_perc(long double ps_old[4])
{
    int perc;
    long double ps[4];
    FILE *fp;

    fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/stat");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%*s %Lf %Lf %Lf %Lf",
            &ps[0], &ps[1], &ps[2], &ps[3]);
    fclose(fp);

    perc = 100 * ((ps_old[0]+ps_old[1]+ps_old[2]) - (ps[0]+ps[1]+ps[2])) / ((ps_old[0]+ps_old[1]+ps_old[2]+ps_old[3]) - (ps[0]+ps[1]+ps[2]+ps[3]));

    ps_old[0] = ps[0];
    ps_old[1] = ps[1];
    ps_old[2] = ps[2];
    ps_old[3] = ps[3];

    RETURN_FORMAT(5, "%02d%%", perc);
}

static char *
fan_ibm(void)
{
    int fan;
    FILE *fp;

    fp = fopen("/proc/acpi/ibm/fan", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/acpi/ibm/fan");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%*s %*s\nspeed: %d", &fan);
    fclose(fp);

    RETURN_FORMAT(5, "%04d", fan);
}

static char *
datetime(const char *fmt)
{
    time_t t;
    char str[80];

    t = time(NULL);
    if (strftime(str, sizeof(str), fmt, localtime(&t)) == 0) {
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    RETURN_FORMAT(50, "%s", str);
}

static char *
disk_free(const char *mnt)
{
    struct statvfs fs;

    if (statvfs(mnt, &fs) < 0) {
        warn("Failed to get filesystem info");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    RETURN_FORMAT(10, "%f", (float)fs.f_bsize * (float)fs.f_bfree / 1024 / 1024 / 1024);
}

static char *
disk_perc(const char *mnt)
{
    int perc;
    struct statvfs fs;

    if (statvfs(mnt, &fs) < 0) {
        warn("Failed to get filesystem info");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    perc = 100 * (1.0f - ((float)fs.f_bfree / (float)fs.f_blocks));

    RETURN_FORMAT(5, "%d%%", perc);
}

static char *
disk_total(const char *mnt)
{
    struct statvfs fs;

    if (statvfs(mnt, &fs) < 0) {
        warn("Failed to get filesystem info");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    RETURN_FORMAT(10, "%f", (float)fs.f_bsize * (float)fs.f_blocks / 1024 / 1024 / 1024);
}

static char *
disk_used(const char *mnt)
{
    struct statvfs fs;

    if (statvfs(mnt, &fs) < 0) {
        warn("Failed to get filesystem info");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    RETURN_FORMAT(10, "%f", (float)fs.f_bsize * ((float)fs.f_blocks - (float)fs.f_bfree) / 1024 / 1024 / 1024);
}

static char *
disk_io(void)
{
    int diskIO;
    FILE *fp;

    fp = fopen("/proc/diskstats", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/diskstats");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%*d %*d %*s %*d %*d %*d %*d %*d %*d %*d %*d %d", &diskIO);
    fclose(fp);

    RETURN_FORMAT(5, "%02d", diskIO);
}

static char *
entropy(void)
{
    int num;
    FILE *fp;

    fp= fopen("/proc/sys/kernel/random/entropy_avail", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/sys/kernel/random/entropy_avail");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%d", &num);
    fclose(fp);

    RETURN_FORMAT(10, "%d", num);
}

static char *
gid(void)
{
    RETURN_FORMAT(10, "%d", getgid());
}

static char *
hostname(void)
{
    char buf[HOST_NAME_MAX];

    if (gethostname(buf, sizeof(buf)) == -1) {
        warn("hostname");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    RETURN_FORMAT(66, "%s", buf);
}

static char *
ip(const char *iface)
{
    struct ifaddrs *ifaddr, *ifa;
    int s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        warn("Failed to get IP address for interface %s", iface);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (!strcmp(ifa->ifa_name, iface) && (ifa->ifa_addr->sa_family == AF_INET)) {
            if (s != 0) {
                warnx("Failed to get IP address for interface %s", iface);
                RETURN_FORMAT(10, UNKNOWN_STR);
            }
            freeifaddrs(ifaddr);
            RETURN_FORMAT(66, "%s", host);
        }
    }

    freeifaddrs(ifaddr);

    RETURN_FORMAT(10, UNKNOWN_STR);
}

static char *
load_avg(void)
{
    double avgs[3];

    if (getloadavg(avgs, 3) < 0) {
        warnx("Failed to get the load avg");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    RETURN_FORMAT(50, "%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

static char *
ram_free(void)
{
    long free;
    FILE *fp;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/meminfo");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "MemFree: %ld kB\n", &free);
    fclose(fp);

    RETURN_FORMAT(10, "%f", (float)free / 1024 / 1024);
}

static char *
ram_perc(void)
{
    long total, free, buffers, cached;
    FILE *fp;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/meminfo");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "MemTotal: %ld kB\n", &total);
    fscanf(fp, "MemFree: %ld kB\n", &free);
    fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
    fscanf(fp, "Cached: %ld kB\n", &cached);
    fclose(fp);

    RETURN_FORMAT(5, "%ld%%", 100 * ((total - free) - (buffers + cached)) / total);
}

static char *
ram_total(void)
{
    long total;
    FILE *fp;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/meminfo");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "MemTotal: %ld kB\n", &total);
    fclose(fp);

    RETURN_FORMAT(10, "%f", (float)total / 1024 / 1024);
}

static char *
ram_used(void)
{
    long free, total, buffers, cached;
    FILE *fp;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/meminfo");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "MemTotal: %ld kB\n", &total);
    fscanf(fp, "MemFree: %ld kB\n", &free);
    fscanf(fp, "MemAvailable: %ld kB\nBuffers: %ld kB\n", &buffers, &buffers);
    fscanf(fp, "Cached: %ld kB\n", &cached);
    fclose(fp);

    RETURN_FORMAT(10, "%f", (float)(total - free - buffers - cached) / 1024 / 1024);
}

static char *
run_command(const char *cmd)
{
    FILE *fp;
    char buf[1024] = "n/a";

    fp = popen(cmd, "r");
    if (fp == NULL) {
        warn("Failed to get command output for %s", cmd);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fgets(buf, sizeof(buf)-1, fp);
    pclose(fp);

    buf[strlen(buf)] = '\0';
    strtok(buf, "\n");

    RETURN_FORMAT(256, "%s", buf);
}

static char *
swap_free(void)
{
    long total, free;
    FILE *fp;
    char buf[2048];
    size_t bytes_read;
    char *match;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/meminfo");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    bytes_read = fread(buf, sizeof(char), sizeof(buf), fp);
    buf[bytes_read] = '\0';
    fclose(fp);
    if (bytes_read == 0 || bytes_read == sizeof(buf)) {
        warn("Failed to read /proc/meminfo\n");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    match = strstr(buf, "SwapTotal");
    sscanf(match, "SwapTotal: %ld kB\n", &total);
    if (total == 0) {
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    match = strstr(buf, "SwapFree");
    sscanf(match, "SwapFree: %ld kB\n", &free);

    RETURN_FORMAT(10, "%f", (float)free / 1024 / 1024);
}

static char *
swap_perc(void)
{
    long total, free, cached;
    FILE *fp;
    char buf[2048];
    size_t bytes_read;
    char *match;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/meminfo");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    bytes_read = fread(buf, sizeof(char), sizeof(buf), fp);
    buf[bytes_read] = '\0';
    fclose(fp);
    if (bytes_read == 0 || bytes_read == sizeof(buf)) {
        warn("Failed to read /proc/meminfo\n");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    match = strstr(buf, "SwapTotal");
    sscanf(match, "SwapTotal: %ld kB\n", &total);
    if (total == 0) {
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    match = strstr(buf, "SwapCached");
    sscanf(match, "SwapCached: %ld kB\n", &cached);

    match = strstr(buf, "SwapFree");
    sscanf(match, "SwapFree: %ld kB\n", &free);

    RETURN_FORMAT(5, "%ld%%", 100 * (total - free - cached) / total);
}

static char *
swap_total(void)
{
    long total;
    FILE *fp;
    char buf[2048];
    size_t bytes_read;
    char *match;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/meminfo");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    bytes_read = fread(buf, sizeof(char), sizeof(buf), fp);
    buf[bytes_read] = '\0';
    fclose(fp);
    if (bytes_read == 0 || bytes_read == sizeof(buf)) {
        warn("Failed to read /proc/meminfo\n");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    match = strstr(buf, "SwapTotal");
    sscanf(match, "SwapTotal: %ld kB\n", &total);
    if (total == 0) {
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    RETURN_FORMAT(10, "%f", (float)total / 1024 / 1024);
}

static char *
swap_used(void)
{
    long total, free, cached;
    FILE *fp;
    char buf[2048];
    size_t bytes_read;
    char *match;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/meminfo");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    bytes_read = fread(buf, sizeof(char), sizeof(buf), fp);
    buf[bytes_read] = '\0';
    fclose(fp);
    if (bytes_read == 0 || bytes_read == sizeof(buf)) {
        warn("Failed to read /proc/meminfo\n");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    match = strstr(buf, "SwapTotal");
    sscanf(match, "SwapTotal: %ld kB\n", &total);
    if (total == 0) {
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    match = strstr(buf, "SwapCached");
    sscanf(match, "SwapCached: %ld kB\n", &cached);

    match = strstr(buf, "SwapFree");
    sscanf(match, "SwapFree: %ld kB\n", &free);

    RETURN_FORMAT(10, "%f", (float)(total - free - cached) / 1024 / 1024);
}

static char *
temp(const char *file)
{
    int temp;
    FILE *fp;

    fp = fopen(file, "r");
    if (fp == NULL) {
        warn("Failed to open file %s", file);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%d", &temp);
    fclose(fp);

    RETURN_FORMAT(20, "%d°C", temp / 1000);
}

static char *
uptime(void)
{
    struct sysinfo info;
    int h = 0;
    int m = 0;

    sysinfo(&info);
    h = info.uptime / 3600;
    m = (info.uptime - h * 3600 ) / 60;

    RETURN_FORMAT(20, "%dh %dm", h, m);
}

static char *
username(void)
{
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);

    if (pw == NULL) {
        warn("Failed to get username");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    RETURN_FORMAT(256, "%s", pw->pw_name);
}

static char *
uid(void)
{
    RETURN_FORMAT(10, "%d", geteuid());
}

#ifndef PULSE
static char *
vol_perc_alsa(const char *card)
{
    int mute;
    long int vol, max, min;
    uint16_t vol_perc;
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *s_elem;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    snd_mixer_selem_id_malloc(&s_elem);
    snd_mixer_selem_id_set_name(s_elem, "Master");
    elem = snd_mixer_find_selem(handle, s_elem);

    if (elem == NULL) {
        snd_mixer_selem_id_free(s_elem);
        snd_mixer_close(handle);
        warn("Failed to get volume percentage for %s", card);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    snd_mixer_handle_events(handle);
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_get_playback_volume(elem, 0, &vol);
    snd_mixer_selem_get_playback_switch(elem, 0, &mute);

    snd_mixer_selem_id_free(s_elem);
    snd_mixer_close(handle);

    vol_perc = ((uint16_t)(vol * 100) / max);
    if (!mute) {
        RETURN_FORMAT(20, VOL_MUTE_STR);
    } else if (max == 0 || vol_perc == 0) {
        RETURN_FORMAT(20, VOL_ZERO_STR);
    } else {
        RETURN_FORMAT(20, VOL_STR, vol_perc);
    }
}
#endif

#ifdef PULSE
static char *
vol_perc_pulse(void)
{
    RETURN_FORMAT(80, pulse_vol_str);
}

static char *
micvol_perc_pulse(void)
{
    RETURN_FORMAT(80, pulse_micvol_str);
}

static char *
pulse_profile(void)
{
    RETURN_FORMAT(80, pulse_profile_str);
}

static char *
pulse_profile_icon(void)
{
    if (!strcmp(PULSE_HEADPHONE_STR, pulse_profile_str)) {
        RETURN_FORMAT(80, PULSE_HEADPHONE_ICON);
    } else if (!strcmp(PULSE_SPEAKER_STR, pulse_profile_str)) {
        RETURN_FORMAT(80, PULSE_SPEAKER_ICON);
    } else if (!strcmp(PULSE_HDMI_STR, pulse_profile_str)) {
        RETURN_FORMAT(80, PULSE_HDMI_ICON);
    }
    RETURN_FORMAT(10, UNKNOWN_STR);
}

static void
pulse_context_state_cb(pa_context *c, void *userdata)
{
    switch(pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;
        case PA_CONTEXT_READY:; /* <- note the semi-colon, very important */
            pa_context_set_subscribe_callback(c, pulse_volume_change_cb, NULL);
            pa_operation_unref(pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE, NULL, NULL));
            pa_operation_unref(pa_context_get_sink_info_list(c, pulse_sink_info_cb, NULL));
            pa_operation_unref(pa_context_get_source_info_list(c, pulse_source_info_cb, NULL));
            break;
        default:
            fprintf(stderr, "pulse connection failure: %s\n",
                    pa_strerror(pa_context_errno(c)));

            sprintf(pulse_vol_str, UNKNOWN_STR);
            sprintf(pulse_profile_str, UNKNOWN_STR);
            pa_context_unref(c);
            break;
    }
}

static void
pulse_sink_info_cb(pa_context *c, const pa_sink_info *sink_info, int eol, void *userdata)
{
    if (sink_info && sink_info->index == SINK_INDEX) {
        sprintf(pulse_profile_str, sink_info->name);

        pa_volume_t vol = (int)(pa_cvolume_avg(&sink_info->volume) * 100.0 
                / (sink_info->n_volume_steps-1) + .5);

        if (sink_info->mute) {
            sprintf(pulse_vol_str, VOL_MUTE_STR);
        } else if (vol == 0) {
            sprintf(pulse_vol_str, VOL_ZERO_STR "%%");
        } else {
            sprintf(pulse_vol_str, VOL_STR "%%", vol);
        }
    }
}

static void
pulse_source_info_cb(pa_context *c, const pa_source_info *source_info, int eol, void *userdata)
{
    if (source_info && source_info->index == SOURCE_INDEX) {
        pa_volume_t vol = (int)(pa_cvolume_avg(&source_info->volume) * 100.0 
                / (source_info->n_volume_steps-1) + .5);

        if (source_info->mute) {
            sprintf(pulse_micvol_str, VOL_MUTE_STR);
        } else if (vol == 0) {
            sprintf(pulse_micvol_str, VOL_ZERO_STR "%%");
        } else {
            sprintf(pulse_micvol_str, VOL_STR "%%", vol);
        }
    }
}

static void
pulse_volume_change_cb(pa_context *c, pa_subscription_event_type_t t, 
        uint32_t idx, void *userdata)
{
    pa_operation_unref(pa_context_get_sink_info_list(c, pulse_sink_info_cb, NULL));
    pa_operation_unref(pa_context_get_source_info_list(c, pulse_source_info_cb, NULL));
}
#endif

static char *
net_up(double *tx_old, const char *iface)
{
    char path[50];
    double tx_val, tx_buf = 0;
    FILE *fp;

    sprintf(path, "/sys/class/net/%s/statistics/tx_bytes", iface);
    fp = fopen(path, "r");
    if (fp == NULL) {
        warn("Failed to open file %s", path);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%lf", &tx_buf);
    fclose(fp);

    tx_val = tx_buf-*tx_old;
    *tx_old = tx_buf;

    if (tx_val < 1000) {
        RETURN_FORMAT(20, "%-4.3g B/s", tx_val);
    } else if (tx_val < 1024000) {
        RETURN_FORMAT(20, "%-4.3gKB/s", tx_val/1024.0);
    } else {
        RETURN_FORMAT(20, "%-4.3gMB/s", tx_val/1048576.0);
    }
}

static char *
net_down(double *rx_old, const char *iface)
{
    char path[50];
    double rx_val, rx_buf = 0;
    FILE *fp;

    sprintf(path, "/sys/class/net/%s/statistics/rx_bytes", iface);
    fp = fopen(path, "r");
    if (fp == NULL) {
        warn("Failed to open file %s", path);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    fscanf(fp, "%lf", &rx_buf);
    fclose(fp);

    rx_val = rx_buf-*rx_old;
    *rx_old = rx_buf;

    if (rx_val < 1000) {
        RETURN_FORMAT(20, "%-4.3g B/s", rx_val);
    } else if (rx_val < 1024000) {
        RETURN_FORMAT(20, "%-4.3gKB/s", rx_val/1024.0);
    } else {
        RETURN_FORMAT(20, "%-4.3gMB/s", rx_val/1048576.0);
    }
}

static char *
wifi_perc(void)
{
    int perc = -1;
    FILE *fp;

    fp = fopen("/proc/net/wireless", "r");
    if (fp == NULL) {
        warn("Failed to open file /proc/net/wireless");
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    /* there has to be a better way to accomplish this
     * but it works */
    fscanf(fp, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s\n\
            %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*d\n\
            %*s %*d %d", &perc);
    fclose(fp);

    if (perc == -1) {
        RETURN_FORMAT(10, UNKNOWN_STR);
    } else  {
        RETURN_FORMAT(5, "%d%%", perc);
    }
}

static char *
wifi_essid(const char *iface)
{
    char id[IW_ESSID_MAX_SIZE+1];
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct iwreq wreq;

    memset(&wreq, 0, sizeof(struct iwreq));
    wreq.u.essid.length = IW_ESSID_MAX_SIZE+1;
    sprintf(wreq.ifr_name, iface);
    if (sockfd == -1) {
        warn("Failed to get ESSID for interface %s", iface);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }
    wreq.u.essid.pointer = id;
    if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
        warn("Failed to get ESSID for interface %s", iface);
        RETURN_FORMAT(10, UNKNOWN_STR);
    }

    close(sockfd);

    if (!strcmp((char *)wreq.u.essid.pointer, "")) {
        RETURN_FORMAT(10, UNKNOWN_STR);
    } else {
        RETURN_FORMAT(256, "%s", (char *)wreq.u.essid.pointer);
    }
}

static void
update_status(output dest, const char *str)
{
    if (dest == XROOT) {
        XStoreName(display, DefaultRootWindow(display), str);
        XSync(display, False);
    } else {
        printf("%s\n", str);
    }
}

static void
sighandler(const int signo)
{
    if (signo == SIGTERM || signo == SIGINT) {
        done = 1;
    }
}

int
main(int argc, char *argv[])
{
    output dest = XROOT;
    if (argc == 2 && !strcmp("-v", argv[1])) {
        printf("sstat-%s\n", VERSION);
        exit(0);
    } else if (argc == 2 && !strcmp("-d", argv[1])) {
        if (daemon(1, 1) < 0) {
            err(1, "daemon");
        }
    } else if (argc == 2 && !strcmp("-o", argv[1])) {
        dest = STDOUT;
    } else if (argc != 1) { 
        fprintf(stderr, "usage: sstat [option]\n"
                "options:\n"
                "  -d start daemonized\n"
                "  -o print status instead of setting it as rootwindow title\n"
                "  -v print version info and exit\n"
                "  -h print this info and exit\n");
        exit(1);
    }
    if (dest == XROOT) {
        if (!(display = XOpenDisplay(NULL))) {
            fprintf(stderr, "sstat: cannot open display\n");
            exit(1);
        }
    }

    /* init signal handling */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sighandler;
    sigaction(SIGINT,  &act, 0);
    sigaction(SIGTERM, &act, 0);

    /* take care of variables that need to be
     * kept between cycles */
#define cpu_perc()          cpu_perc(ps_old)
#define net_up(interface)   net_up(&tx_old, interface)
#define net_down(interface) net_down(&rx_old, interface)
    long double ps_old[4];
    double rx_old, tx_old;

#ifdef PULSE

    /* init pulseaudio */
    pa_threaded_mainloop *m = pa_threaded_mainloop_new();
    pa_threaded_mainloop_start(m);
    assert(m);

    pa_threaded_mainloop_lock(m);

    pa_context *c = pa_context_new(pa_threaded_mainloop_get_api(m), "sstat_volmon");
    pa_context_set_state_callback(c, pulse_context_state_cb, NULL);
    pa_context_connect(c, NULL, PA_CONTEXT_NOFLAGS, NULL);
    assert(c);

    pa_threaded_mainloop_unlock(m);
#endif

    /* main loop, 
     * make sure to keep delay exactly one second */
    struct timeval tv;
    unsigned long utime = 0;
    int status_len = snprintf(NULL, 0, STATUS_FORMAT, STATUS_CONTENT);
    char status_str[++status_len];
    while (!done) {
        gettimeofday(&tv, NULL);
        utime = 1000000 * tv.tv_sec + tv.tv_usec;

        sprintf(status_str, STATUS_FORMAT, STATUS_CONTENT);
        update_status(dest, status_str); 

        gettimeofday(&tv, NULL);
        utime = (1000000 * tv.tv_sec + tv.tv_usec) - utime;

        if (utime < 1000000) {
            usleep(1000000 - utime);
        }
    }

    if (dest == XROOT) {
        update_status(dest, NULL);
        XCloseDisplay(display);
    }

#ifdef PULSE
    /* cleanup pulse */
    pa_context_unref(c);
    pa_threaded_mainloop_stop(m);
    pa_threaded_mainloop_free(m);
#endif

    return 0;
}
