/*
 *    ucon.c
 *    Copyright (C) 2011 n6210 <fotonix@pm.me>
 *
 *    Microconsole
 *    Dedicated for FTDI chips but is possible to use with others.
 *    Supports speed from 50 bps upto 4 Mbps
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _BSD_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include <pthread.h>

/* The escape code to clear the screen */
#define CLEAR "\033[H\033[J"

/* The escape code to clear to end of line */
#define CLEAR_EOL "\033[K"

#define CGREB "\033[1;32m"
#define CREDB "\033[1;31m"
#define CNORB "\033[1;97m"
#define CNORM "\033[0m"

int speed_table[] = {
    50, 75, 110, 134, 150, 200, 300, 600, 1200, 2400, 4800,
    9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600,
    1000000, 1152000, 1500000, 2000000, 2500000, 3000000, 3500000, 4000000, 0
};

int speed_value[] = {
    B50, B75, B110, B134, B150, B200, B300, B600, B1200, B2400, B4800,
    B9600, B19200, B38400, B57600, B115200, B230400, B460800, B921600,
    B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000, B0
};

#define M_STR_LEN	1024
struct strigtocom {
	char trigger[M_STR_LEN];
	char command[M_STR_LEN];
} *trigtocom;
volatile int trigtocom_cnt = 0;
bool trigtocom_enabled = false;
int go_exit = 0;
int ctrl_c_key = 0;
int date_time = 0;
unsigned char bufin[4096];
struct pollfd pollfds;
int fd;
char *mfname = NULL;

char *help = {
    "Ctrl-X - exit\n"
    "Ctrl-A or Ctrl-D is a command key combination.\n"
    "Press it and next command letter:\n"
    "H/h - print help\n"
    "Q/q - exit\n"
    "C/c - clear screen\n"
    "T/t - enable/disable time stamp at start of each line\n"
    "M/m - trigger to command enable/disable\n"
    "U/u - increase port speed\n"
    "D/d - decrease port speed\n\n"
    "Example:\n"
    "  Ctrl-D C or Ctrl-A C - clear the screen\n\n"
};

int find_speed(int speed)
{
    int idx = 0;

    while (1) {
		int spd = speed_table[idx];

		if (spd == 0) break;

		if (speed <= spd) {
	    	//fprintf(stderr, "IDX: %d\n", idx);
			return idx;
		} else
	    	idx++;
    }

    return 15; // By default return 115200
}

int set_serial_speed(int fd, int speed, struct termios *oterminfo)
{
    struct termios attr;

    memset(oterminfo, 0, sizeof(attr));
    memset(&attr, 0, sizeof(attr));

    if (tcgetattr(fd, oterminfo) == -1) {
        perror("tcgetattr");
        return -1;
    }

	memcpy(&attr, oterminfo, sizeof(attr));

    cfmakeraw(&attr);
	attr.c_cflag |= CLOCAL;

/*
    attr.c_iflag = IGNBRK;
    attr.c_cflag = CREAD | CLOCAL | CS8 | speed;
    attr.c_oflag = 0;
    attr.c_lflag = 0; // ICANON;
    attr.c_cc[VMIN] = 1;
    attr.c_cc[VTIME] = 0;
*/
    if (cfsetspeed(&attr, speed) < 0) {
		perror("cfsetspeed");
	    return -1;
	}

    tcflush(fd, TCIFLUSH);

    if (tcsetattr(fd, TCSANOW, &attr)) {
        fprintf(stderr, "Failed to set terminal state\n");
        return -1;
    }

    return 0;
}

void set_stdin(struct termios *sflags)
{
    const int fd = fileno(stdin);
    struct termios flags;

    if (tcgetattr(fd, sflags) < 0) {
		fprintf(stderr, "Unable to save terminal settings\n");
		return;
    }

	memcpy(&flags, sflags, sizeof(flags)); // make a working copy

    flags.c_lflag &= ~(ICANON | ECHO); // set raw (unset canonical modes)
    flags.c_cc[VMIN] = 0; // i.e. min 1 char for blocking, 0 chars for non-blocking
    flags.c_cc[VTIME] = 0; // block if waiting for char

    if (tcsetattr(fd, TCSANOW, &flags) < 0) {
		fprintf(stderr, "Unable to change terminal setting\n");
		return;
    }
}

void unset_stdin(struct termios *flags)
{
    const int fd = fileno(stdin);

    if (tcsetattr(fd,TCSANOW, flags) < 0) {
		printf("Unable to restore terminal settings\n");
		return;
    }
}

static void ctrl_c(int sig)
{
    //fprintf(stderr, "\nCtrl-C captured\n");
    ctrl_c_key = 1;
}

void set_signal_handler(int sig, int flags, void (*sighandler)(int))
{
    struct sigaction sa;

    sa.sa_flags = flags ? SA_RESTART : 0;
    sa.sa_handler = sighandler;
    sigaction(sig, &sa, NULL);
}

int new_speed(int fd, int idx)
{
    struct termios otinfo2;
    int ret;

    ret = set_serial_speed(fd, speed_value[idx], &otinfo2);
    if (ret)
		fprintf(stderr, "\nUnable to set speed %d\n", speed_table[idx]);
    else
		fprintf(stderr, "\nNew speed set to "CGREB"%d"CNORM" bps\n", speed_table[idx]);

    return ret;
}

int file_is_modified(const char *path) {
	struct stat file_stat;
	static struct stat fs = {.st_mtime = 0};
	int err, ret = 0;

	err = stat(path, &file_stat);
	if (err == 0) {
		if (fs.st_mtime != 0) {
			ret = file_stat.st_mtime > fs.st_mtime;
		}

		memcpy(&fs, &file_stat, sizeof(fs));
	}

	return ret;
}

int read_trigtocom_file(char *fname, bool show)
{
	int i = 0, ret;
	FILE *fp;
	char *line = NULL;
	size_t size = 0;
	ssize_t rt, rc;

	fp = fopen(fname, "r");
	if (!fp) {
		fprintf(stderr, "%s [%s]\n", strerror(errno), fname);
		return errno;
	}

	do {
		if (!trigtocom)
			trigtocom = malloc(sizeof(struct strigtocom));
		else
			trigtocom = realloc(trigtocom, sizeof(struct strigtocom) * (i + 1));

		rt = getline(&line, &size, fp);
		if ((rt != -1) && (rt > 3)) {
			line[rt - 1] = 0;
			strncpy(trigtocom[i].trigger, line, M_STR_LEN);

			rc = getline(&line, &size, fp);
			if ((rc != -1) && (rc > 1)) {
				line[rc - 1] = 0;
				strncpy(trigtocom[i].command, line, M_STR_LEN);
				i++;
			} else {
				memset(trigtocom[i].trigger, 0, M_STR_LEN);
				break;
			}
		} else
			continue;

	} while (!feof(fp));

	if (line) {
		free(line);
		trigtocom_cnt = i;
		trigtocom_enabled = true;
		printf("\n"CNORB"Found %d valid trigger(s) in file: %s"CNORM"\n", trigtocom_cnt, fname);
		if (show) {
			for (i = 0; i < trigtocom_cnt; i++)
				printf("TTC#%02d: [%s] -> [%s]\n", i, trigtocom[i].trigger, trigtocom[i].command);
		}
	}

	ret = fclose(fp);

	return ret;
}


void reload_trigcom_file(char *fname)
{
	trigtocom_cnt = 0;
	free(trigtocom);
	trigtocom = NULL;

	if (fname)
		read_trigtocom_file(fname, false);
}

void *print_from_serial(void *ptr)
{
	int n;
	char buff[4096 * 2];

	while (1) {
		errno = 0;
		n = poll(&pollfds, 1, -1);
		if (n == 1) {
			if (pollfds.revents == POLLIN) {
				errno = 0;
				n = read(fd, buff, sizeof(buff));
				if (n <= 0)	{
					if ((n < 0) && (errno != EINTR)) {
						fprintf(stderr,"Serial port read error (%d) %s\n", errno, strerror(errno));
						break;
					} else
						continue;
				}

				if (trigtocom_enabled && file_is_modified(mfname)) {
					reload_trigcom_file(mfname);
				}

				if (date_time) {
					time_t t;
					struct tm *lt;

					time(&t);
					lt = localtime(&t);

					fprintf(stderr, "%4d-%02d-%02d %2d:%02d:%02d | ",
							lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday,
							lt->tm_hour, lt->tm_min, lt->tm_sec);
				}

				fwrite(buff, n, 1, stderr);
				//{ int i; for(i = 0; i < n; i++) fprintf(stderr, "%02X", buff[i]);}
				if (trigtocom_enabled && trigtocom_cnt) {
					int i, x;
					for (i = 0; i < trigtocom_cnt; i++) {
						if (strstr(buff, trigtocom[i].trigger) != NULL) {
							dprintf(fd, "\r");
							for (x = 0; x < strlen(trigtocom[i].command); x++) {
								write(fd, &trigtocom[i].command[x], 1);
								usleep(1000);
							}
							dprintf(fd, "\r");
							fsync(fd);
							fprintf(stderr, "\n"CNORB"Trigger: "CGREB"%s"CNORB" command: "CREDB"%s"CNORM"\n", trigtocom[i].trigger, trigtocom[i].command);
						}
					}
				}

				memset(buff, 0 , sizeof(buff));
			}
			else
				if (pollfds.revents) {
					fprintf(stderr,"Serial port access error\n");
					break;
				}
		} else {
			if ((n < 0) && (errno != EINTR)) {
				fprintf(stderr,"Serial port read access error (%d) %s\n", errno, strerror(errno));
				break;
			}
		}
	}

	go_exit = 1;

	pthread_exit(NULL);
}


int main(int argc, char **argv)
{
	int n,
#ifdef AUTO_RESET
	status,
#endif
	command;
	pthread_t thread1;
	struct pollfd stdin_fd;
	struct termios flags; // for save terminal state
	struct termios oldterminfo; // for serial port
	int spd, speed = 115200;

	char *sl = "-----------------------------------------------------------";

	if (argc < 2) {
	    fprintf(stderr, "\nuc - micro console for high speed ports (FTDI)\n"
			"Copyright 2011-2018 Taddy G. <fotonix0@pm.me>\n");

	    fprintf(stderr, "Usage: uc <device> <speed> [optional_ttc_file]\n"
			"Supported speeds 50 bps - 4 Mbps\n"
			"Default speed is 115200 bps\n"
			"Examples:\n"
			"  uc /dev/ttyUSB0 115200\n"
			"  uc /dev/ttyUSB0 921600 trigger_to_command.txt\n"
	    );

	    return 1;
	}

	if (argc > 2)
	    speed = atoi(argv[2]);

	if (argc >3) {
		if (read_trigtocom_file(argv[3], true) == 0)
			mfname = argv[3];
	}

	spd = find_speed(speed);

	fprintf(stderr, "%s\n", sl);
	fprintf(stderr, "Serial port: %s (%d bps 8N1)\n\n", argv[1], speed_table[spd]);
	fprintf(stderr, "Exit: Ctrl-X or Ctrl-A Q  TTC: Ctrl-A M\n"
					"Speed: Ctrl-A U (up) or Ctrl-A D (down)\n"
					"Help: Ctrl-A H\n");
	fprintf(stderr, "%s\n\n", sl);

	fd = open(argv[1], O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
	    perror("Serial port open");
	    return 1;
	}

	if (fcntl(fd, F_SETFL, O_RDWR) < 0) {
		perror("Serial port fcntl");
		return 1;
	}

	if (lockf(fd, F_TEST, 1) < 0) {
		fprintf(stderr, CREDB"Serial port locked in use by other program - exit"CNORM"\n");
		return 2;
	}

	if (lockf(fd, F_TLOCK, 1) < 0) {
		perror("Serial port lockf");
		return 1;
	}

	if (tcflush(fd, TCIOFLUSH))	{
		perror("Serial port tcflush");
		return 1;
	}

	// Wow - we have to set some speed and then change it - hmm
	if (set_serial_speed(fd, speed_value[11], &oldterminfo) ||
	    set_serial_speed(fd, speed_value[spd], &oldterminfo))
		return 1;

#ifdef AUTO_RESET
	/* special handshakes */
	if (ioctl(fd, TIOCMGET, &status) == -1)	{
	    perror("handshake(): TIOCMGET");
	}
	// RESET_N asserted
	status |= TIOCM_DTR;
	if (ioctl(fd, TIOCMSET, &status) == -1)	{
	    perror("handshake(): TIOCMSET");
	    return 1;
	}
	fprintf(stderr, "Reset_N (DTR) asserted\n");
	// pause
	usleep(500000);
#endif

	pollfds.fd = fd;
	pollfds.events = POLLIN;

	// flush
	while (poll(&pollfds, 1, 1) == 1) {
		read(fd, bufin, sizeof(bufin));
	}

#ifdef AUTO_RESET
	// RESET_N deasserted
	status &= ~TIOCM_DTR;
	if (ioctl(fd, TIOCMSET, &status) == -1) {
	    perror("handshake(): TIOCMSET");
	    return 1;
	}
	fprintf(stderr, "Reset_N (DTR) deasserted\n");
#endif

	// Setting for the terminal
	set_stdin(&flags);
	stdin_fd.fd = 0;
	stdin_fd.events = POLLIN;

	set_signal_handler(SIGINT, 0, ctrl_c);

	command = 0;

	if (pthread_create(&thread1, NULL, print_from_serial, NULL) != 0) {
		perror("pthread_create() failed");
		return 1;
	} else {
		pthread_detach(thread1);
	}

	while (!go_exit) {
	    //Very simple terminal ;)
		if (poll(&stdin_fd, 1, 1) == 1) {
			memset(bufin, 0, sizeof(bufin));
			n = read(0, bufin, sizeof(bufin));

			if (n > 0) {
				int i;

				for (i = 0; i < n; i++)	{
					if (bufin[i] == '\n')
							bufin[i] = '\r';
				}

				// if prev key was Ctrl+A/D try execute some command
				if (command) {
					switch (bufin[0])
					{
					case 'q':
					case 'Q':
							go_exit = 1;
							break;
					case 'u':
					case 'U':
							spd++;
							if (speed_table[spd] == 0) spd = 0;

							new_speed(fd, spd);
							break;
					case 'd':
					case 'D':
							spd--;
							if (spd < 0) spd = (sizeof(speed_table)/sizeof(int)) - 2;

							new_speed(fd, spd);
							break;
					case 'p':
					case 'P':
							fprintf(stderr, CNORB"Current speed: "CGREB"%d"CNORM"\n", speed_table[spd]);
							fprintf(stderr, CNORB"TTC is: %s"CNORM"\n", trigtocom_enabled?CGREB"enabled":CREDB"disabled");
							fprintf(stderr, CNORB"Timestamp is: %s"CNORM"\n", date_time?CGREB"enabled":CREDB"disabled");
							break;
					case 'c':
					case 'C':
							fprintf(stderr, CLEAR);
							break;
					case 'm':
					case 'M':
							if (trigtocom_enabled) {
								trigtocom_enabled = false;
								if (trigtocom) {
									trigtocom_cnt = 0;
									free(trigtocom);
									trigtocom = NULL;
								}
							} else {
								trigtocom_enabled = true;
								if (mfname)
									read_trigtocom_file(mfname, false);
							}
							fprintf(stderr, "\n"CNORB"Trigger to command is now: %s"CNORM"\n", trigtocom_enabled?CGREB"enabled":CREDB"disabled");
							break;
					case 'h':
					case 'H':
							fprintf(stderr, "%s\n", help);
							break;
					case 't':
					case 'T':
							date_time = !date_time;
							break;
					default:
							if (write(fd, bufin, n) < 0)
							{
								fprintf(stderr,"Serial port write access error\n");
								break;
								goto out;
							}
					}
					command = 0;
		    	}
		    	else
		    	{
					// Try to catch some CTRL keys
		        	switch (bufin[0])
		        	{
		        	case 1: //CTRL-A
		    		    command = 1;
		    		    break;
		        	case 4: //CTRL-D
		    		    command = 2;
		    		    break;
					case 24:
						go_exit = 1;
						break;
		        	default:
						if (write(fd, bufin, n) < 0)
			    		{
							fprintf(stderr,"Serial port write access error\n");
							break;
							goto out;
			    		}
					}
		    	}
			}
	    }

	    if (ctrl_c_key == 1)
	    {
			bufin[0] = 0x3;
			write(fd, bufin, 1);
			ctrl_c_key = 0;
			//fprintf(stderr, "Ctrl-C sent\n");
	    }
	}

out:
	// Back term params
	unset_stdin(&flags);

	lockf(fd, F_ULOCK, 1);
	/* close serial port */
	tcsetattr(fd, TCSANOW, &oldterminfo);
	if (close(fd) < 0)
		perror("closeserial()");

	printf("\nConsole restored\n");

	return 0;
}
