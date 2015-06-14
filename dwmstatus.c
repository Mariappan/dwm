#define _BSD_SOURCE
#define _GNU_SOURCE
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/statvfs.h>

#include <X11/Xlib.h>

#include <alsa/asoundlib.h>
#include <alsa/control.h>


#define MB 1048576
#define GB 1073741824

char *tz = "Asia/Kolkata";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *buf = NULL;

	va_start(fmtargs, fmt);
	if (vasprintf(&buf, fmt, fmtargs) == -1){
		fprintf(stderr, "malloc vasprintf\n");
		exit(1);
	}
	va_end(fmtargs);

	return buf;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf(buf);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		perror("getloadavg");
		exit(1);
	}

	return smprintf("\x03\uf3a4 %3.2f", avgs[0]);
}


char *
up() {
	struct sysinfo info;
	int h,m = 0;
	sysinfo(&info);
	h = info.uptime/3600;
	m = (info.uptime - h*3600 )/60;
	return smprintf("\x04\uf108 %dh%02dm",h,m);
}

char *
get_volume(void)
{
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *s_elem;

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, "default");
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);
	snd_mixer_selem_id_malloc(&s_elem);
	snd_mixer_selem_id_set_name(s_elem, "Master");

	elem = snd_mixer_find_selem(handle, s_elem);

	if (elem == NULL)
	{
		snd_mixer_selem_id_free(s_elem);
		snd_mixer_close(handle);

		exit(EXIT_FAILURE);
	}

	long int vol, max, min, percent;

	snd_mixer_handle_events(handle);
	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, 0, &vol);

	percent = (vol * 100) / max;

	snd_mixer_selem_id_free(s_elem);
	snd_mixer_close(handle);

	return smprintf("\x05\uf028 %3d",percent);
}

int
parse_netdev(unsigned long long int *receivedabs, unsigned long long int *sentabs)
{
	char buf[255];
	char *datastart;
	static int bufsize;
	int rval;
	FILE *devfd;
	unsigned long long int receivedacc, sentacc;

	bufsize = 255;
	devfd = fopen("/proc/net/dev", "r");
	rval = 1;

	// Ignore the first two lines of the file
	fgets(buf, bufsize, devfd);
	fgets(buf, bufsize, devfd);

	while (fgets(buf, bufsize, devfd)) {
		if ((datastart = strstr(buf, "lo:")) == NULL) {
			datastart = strstr(buf, ":");

			// With thanks to the conky project at http://conky.sourceforge.net/
			sscanf(datastart + 1, "%llu  %*d     %*d  %*d  %*d  %*d   %*d        %*d       %llu",\
					&receivedacc, &sentacc);
			*receivedabs += receivedacc;
			*sentabs += sentacc;
			rval = 0;
		}
	}

	fclose(devfd);
	return rval;
}


void
calculate_speed(char *speedstr, unsigned long long int newval, unsigned long long int oldval)
{
	double speed;
	speed = (newval - oldval) / 1024.0;
	if (speed > 1024.0) {
		speed /= 1024.0;
		sprintf(speedstr, "%5.2f M", speed);
	} else {
		sprintf(speedstr, "%5.2f K", speed);
	}
}

char *
get_netusage(unsigned long long int *rec, unsigned long long int *sent)
{
	unsigned long long int newrec, newsent;
	newrec = newsent = 0;
	char downspeedstr[15], upspeedstr[15];
	static char retstr[42];
	int retval;

	retval = parse_netdev(&newrec, &newsent);
	if (retval) {
		fprintf(stdout, "Error when parsing /proc/net/dev file.\n");
		exit(1);
	}

	calculate_speed(downspeedstr, newrec, *rec);
	calculate_speed(upspeedstr, newsent, *sent);

	sprintf(retstr, "\x01\uf063 %s  \x02\uf062 %s", downspeedstr, upspeedstr);
	/*sprintf(retstr, "\uf175 %s \uf176 %s", downspeedstr, upspeedstr);*/

	*rec = newrec;
	*sent = newsent;
	return retstr;
}

char *
get_ram()
{
	uintmax_t used = 0;

	struct sysinfo mem;
	sysinfo(&mem);

	/*total   = (uintmax_t) mem.totalram / MB;*/
	used    = (uintmax_t) (mem.totalram - mem.freeram -
			mem.bufferram - mem.sharedram) / MB;

	return smprintf("\uf3a5 %4dM", used);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

int runevery(time_t *ltime, int sec){
	/* return 1 if sec elapsed since last run
	 * else return 0
	 */
	time_t now = time(NULL);

	if ( difftime(now, *ltime ) >= sec)
	{
		*ltime = now;
		return(1);
	}
	else
		return(0);
}

int main(void)
{
	char *status = NULL;
	char *tmprs = NULL;
	char *avgs = NULL;
	time_t count60 = 0;
	char *uptm = NULL;
	char *vol = NULL;
	char *netstats = NULL;
	char *ram = NULL;
	static unsigned long long int rec, sent;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	parse_netdev(&rec, &sent);
	for (;;sleep(1)) {
		/* checks every minutes */
		if ( runevery(&count60, 60) )
		{
			free(tmprs);
			free(uptm);
			tmprs = mktimes("\x06\uf073 %b-%d  %I:%M %p", tz);
			uptm = up();

			free(ram);
			ram = get_ram();
		}
		/* checks every second */
		avgs = loadavg();
		vol = get_volume();
		netstats = get_netusage(&rec, &sent);

		status = smprintf("%s %s %s %s %s %s",
				netstats, avgs, ram, uptm, vol, tmprs);
		setstatus(status);
		free(vol);
		free(avgs);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}

