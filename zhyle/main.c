#include <stdio.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>

#define ITER_CNT 	10	
#define FILENAME	".upafile"

typedef struct sh_t
{
   int cnt;
   char str[100];
} /* __attribute__((packed))*/ sh;

int main()
{
   	sem_t guard;
	sh shared;
	shared.str[0] = 0;
	const size_t size = sizeof(shared);

	int fd;	
	if ((fd = open(FILENAME, O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) == -1) {
		printf("open error: %d \n", errno);
	};

	printf("fd = %d\n", fd);

	if (sem_init(&guard, 1, 1) == -1) {
		printf("sem_init error\n");		
	}
	
	lseek(fd, size + 1, SEEK_SET);

	if ((write(fd, &shared , size)) == -1) {
		printf("write error: %d\n", errno);
	}
	lseek(fd, 0, SEEK_SET);


	void *fm  = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd , 0);
	if (fm == MAP_FAILED) {
		printf("mmap error: %d\n", errno);
	}	

	if (munmap(fm, size) == -1) {
		printf("munmap error\n");
	}

	pid_t pid;
	pid = fork();
	if (pid == -1) {
		printf("fork error: %d\n", pid);
		return 1;
	} else if (pid == 0) {
		printf("this is a child\n");
		int i = 0;
		while (i < ITER_CNT) {
			int cfd = fd;	

			sem_wait(&guard);

			void * cfm  = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, cfd , 0);
			if (cfm == MAP_FAILED) {
				printf("child: mmap error: %d\n", errno);
				sem_post(&guard);
				return 1;
			}

			sh * psh = (sh *)cfm;
			printf("child: cnt = %d str: %s\n", psh->cnt, psh->str);
			psh->cnt = i;			
			sprintf((char *)psh->str, "%s - %d", "child", i);			
			printf("child: cnt = %d str: %s\n", psh->cnt, psh->str);

			if (munmap(cfm, size) == -1) {
				printf("munmap error\n");
			}

			sem_post(&guard);

			sleep(1);
			i++;
		}
	} else {
		printf("this is a parent\n");
		sleep(1);
		int i = 0;
		int status;
		while (i < ITER_CNT) {
			int pfd = fd;

			sem_wait(&guard);

			void * pfm  = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, pfd , 0);
			if (pfm == MAP_FAILED) {
				printf("parent: mmap error: %d\n", errno);
				break;
			}

			sh * psh = (sh *)pfm;
			printf("parent: cnt = %d str: %s\n", psh->cnt, psh->str);
			psh->cnt = i;			
			sprintf((char *)psh->str, "%s - %d", "parent", i);			
			printf("parent: cnt = %d str: %s\n", psh->cnt, psh->str);

			if (munmap(pfm, size) == -1) {
				printf("parent: munmap error\n");
			
			}
			sem_post(&guard);

			sleep(2);
			i++;
		}
		wait(&status);
	}

	if (sem_destroy(&guard) == -1) {
		printf("sem_destroy error: %d\n", errno);
	}

	close(fd);
	return 0;
}
