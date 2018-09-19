#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "common.h"




////////////////////////////////////////////////////////////////////////
//  치명적인 오류 처리
////////////////////////////////////////////////////////////////////////
void fatal(char *message)
{
	fprintf(stderr, "Error: %s\n", message);
	exit(1);
}

////////////////////////////////////////////////////////////////////////
//  일반 에러 처리
////////////////////////////////////////////////////////////////////////
void syserr(char *message)
{
	fprintf(stderr, "Error: %s (%d", message, errno);
	exit(1);
}

////////////////////////////////////////////////////////////////////////
//  부모가 자식의 실행이 끝나길 기다리는 함수
////////////////////////////////////////////////////////////////////////
void waitfor(int pid)
{
	int wpid, status;

	// wait를 이용하여 프로세스를 기다린다.
	while ((wpid = wait(&status)) != pid && wpid != ERROR);
}

////////////////////////////////////////////////////////////////////////
//  프롬프트를 출력한다.
////////////////////////////////////////////////////////////////////////
void print_prompt()
{
	char *ps;
	char *index;

	// PS2 환경 변수를 읽어 사용한다.
	if ((ps = (char*)getenv("PS2")) != NULL)
	{
		// 환경 변수의 값이 끝날때까지 한바이트씩 이동하며 확인
		while (*ps != '\0')
		{
			// 역슬래쉬(\)가 있다면 다음의 문자를 확인한다.
			if (*ps == '\\')
			{
				ps++;

				// 사용자 정보를 출력
				if (*ps == 'u')
				{
					printf("%s", getenv("USER"));
				}

				// 호스트명을 출력
				else if (*ps == 'h')
				{
					printf("%s", getenv("HOSTNAME"));
				}

				// 현재 디렉토리를 출력한다.
				else if (*ps == 'w')
				{
					printf("%s", get_current_dir_name());
				}
			}

			// 아무것도 아닐 경우 문자를 출력
			else
			{
				printf("%c", *ps);
				ps++;
			}
		}
	}

	// PS2 환경 변수가 없다면 기본 프롬프트를 출력한다.
	else
	{
		printf(">> ");
	}
}

////////////////////////////////////////////////////////////////////////
//  사용자 정의 명령을 처리하는 함수
////////////////////////////////////////////////////////////////////////
BOOLEAN shellcmd(int ac, char *av[], int sourcefd, int destfd)
{
	char *path;

	// cd
	if (!strcmp(av[0], "cd"))
	{
		cmd_cd(ac, av);
	}
	// ls
	else if (!strcmp(av[0], "ls"))
	{
		cmd_ls(ac, av);
	}
	// cp
	else if (!strcmp(av[0], "cp"))
	{
		cmd_cp(ac, av);
	}
	// rm
	else if (!strcmp(av[0], "rm"))
	{
		cmd_rm(ac, av);
	}
	// mv
	else if (!strcmp(av[0], "mv"))
	{
		cmd_mv(ac, av);
	}
	// mkdir
	else if (!strcmp(av[0], "mkdir"))
	{
		cmd_mkdir(ac, av);
	}
	// rmdir
	else if (!strcmp(av[0], "rmdir"))
	{
		cmd_rmdir(ac, av);
	}
	// cat
	else if (!strcmp(av[0], "cat"))
	{
		cmd_cat(ac, av);
	}
	// exit
	else if (!strcmp(av[0], "exit"))
	{
		cmd_exit();
	}
	else
	{
		return FALSE;
	}

	if (sourcefd != 0 || destfd != 1)
	{
		fprintf(stderr, "Ilegal redirection or pipeline.\n");
	}

	return TRUE;
}



////////////////////////////////////////////////////////////////////////
//  표준 입/출력을 이용한 리다이렉트 함수
////////////////////////////////////////////////////////////////////////
void redirect(int sourcefd, char *sourcefile, int destfd, char *destfile, BOOLEAN append, BOOLEAN backgrnd)
{
	int flags, fd;

	// 열려있는 소스 파일디스크립터가 없고 백그라운드 실행일 경우
	if (sourcefd == 0 && backgrnd)
	{
		strcpy(sourcefile, "/dev/null");
		sourcefd = BADFD;
	}

	// 소스 파일 디스크립터가 있을 경우
	if (sourcefd != 0)
	{
		// 표준입력을 닫는다.
		if (close(0) == ERROR)
		{
			syserr("close");
		}

		if (sourcefd > 0)
		{
			// 표준입력을 재정의한다.
			if (dup(sourcefd) != 0)
			{
				fatal("dup");
			}
		}

		// 소스파일을 연다.
		else if (open(sourcefile, O_RDONLY, 0) == ERROR)
		{
			fprintf(stderr, "Cannot open %s\n", sourcefile);
			exit(0);
		}
	}

	// 출력 파일 디스크립터가 있다면
	if (destfd != 1)
	{
		// 표준 출력을 닫는다.
		if (close(1) == ERROR)
		{
			syserr("close");
		}

		// 표준 출력을 재정의
		if (destfd > 1)
		{
			if (dup(destfd) != 1)
			{
				fatal("dup");
			}
		}

		else
		{
			// 파일이 없을 경우에 생성하고, 쓰기도 가능하게 파일을 설정
			flags = O_WRONLY | O_CREAT;

			// 추가 모드가 아닐 경우 파일을 비우는 옵션도 추가
			if (!append)
			{
				flags |= O_TRUNC;
			}

			// 파일을 연다.
			if (open(destfile, flags, 0666) == ERROR)
			{
				fprintf(stderr, "Cannot create %s\n", destfile);
				exit(0);
			}

			// 추가 모드라면 가장 끝을 찾는다.
			if (append)
			{
				if (lseek(1, 0L, 2) == ERROR) syserr("lseek");
			}
		}
	}

	// 파일디스크립터를 닫는다.
	for (fd = 3; fd < MAXFD; fd++)
	{
		close(fd);
	}

	return;
}




////////////////////////////////////////////////////////////////////////
//  명령어를 파싱한다.
////////////////////////////////////////////////////////////////////////
SYMBOL parse(int *waitpid, BOOLEAN makepipe, int *pipefdp)
{
	SYMBOL symbol, term;
	int argc, sourcefd, destfd;
	int pid, pipefd[2];
	char *argv[MAXARG + 1], sourcefile[MAXFNAME];
	char destfile[MAXFNAME];
	char word[MAXWORD];
	BOOLEAN append;

	argc = 0;
	sourcefd = 0;
	destfd = 1;


	while (TRUE)
	{
		// 하나의 단어씩을 분석한다.
		switch (symbol = getsymbol(word))
		{
			// 일반 문자일 경우
		case S_WORD:
			if (argc == MAXARG)
			{
				fprintf(stderr, "Too many args.\n");
				break;
			}

			// 새로운 인자 배열을 메모리 할당 한다.
			argv[argc] = (char *)malloc(strlen(word) + 1);

			if (argv[argc] == NULL)
			{
				fprintf(stderr, "Out of arg memory.\n");
				break;
			}

			// 명령어 복사
			strcpy(argv[argc], word);

			// arg 카운터 증가
			argc++;
			continue;

			// < 일 경우
		case S_LT:

			// 파이프가 열려있다면 오류
			if (makepipe)
			{
				fprintf(stderr, "Extra <.\n");
				break;
			}

			// 소스파일의 심볼값을 검사한다.
			if (getsymbol(sourcefile) != S_WORD)
			{
				fprintf(stderr, "Illegal <.\n");
				break;
			}

			sourcefd = BADFD;
			continue;

			// > 혹은 >> 일 경우
		case S_GT:
		case S_GTGT:

			// 목적 파일이 정의되어있지 않을 경우 에러
			if (destfd != 1)
			{
				fprintf(stderr, "Extra > or >>.\n");
				break;
			}

			// 목적어 파일의 심볼타임이 문자열이 아니면 에러
			if (getsymbol(destfile) != S_WORD)
			{
				fprintf(stderr, "Illegal > or >>.\n");
				break;
			}

			// GTGT일 경우 추가 모드로
			destfd = BADFD;
			append = (symbol == S_GTGT);
			continue;

			// |, &, ;, 줄바꿈 문자일 경우 - 하나의 명령어 단위
		case S_BAR:
		case S_AMP:
		case S_SEMI:
		case S_NL:

			argv[argc] = NULL;
			// 심볼이 파이프(|) 일 경우
			if (symbol == S_BAR)
			{
				if (destfd != 1)
				{
					fprintf(stderr, "> or >> conflicts with |.\n");
					break;
				}

				// 현재의 표준출력 디스크립터를 함께 넘겨 명령어를 분석한다.
				term = parse(waitpid, TRUE, &destfd);
			}

			// 종료 문자 세팅
			else
			{
				term = symbol;
			}

			// 파이프가 열려있을 경우 파이프를 연결한다.
			if (makepipe)
			{
				if (pipe(pipefd) == ERROR)
				{
					syserr("pipe");
				}
				*pipefdp = pipefd[1];
				sourcefd = pipefd[0];
			}

			// 명령을 수행한다.
			pid = execute(argc, argv, sourcefd, sourcefile,
				destfd, destfile, append, term == S_AMP);

			// 파이프가 아닐 경우 기다릴 PID를 설정
			if (symbol != S_BAR)
			{
				*waitpid = pid;
			}

			// 인자값을 없을 경우
			if (argc == 0 && (symbol != S_NL || sourcefd > 1))
			{
				fprintf(stderr, "Missing command.\n");
			}

			// 인자로 입력된 값들의 메모리 해제
			while (--argc >= 0)
			{
				free(argv[argc]);
			}

			return term;

			// 명령이 잘못되었을 경우 종료
		case S_EOF:
			exit(0);
		}
	}
}



////////////////////////////////////////////////////////////////////////
//  명령어를 실행하는 함수
////////////////////////////////////////////////////////////////////////
int execute(int ac, char *av[], int sourcefd, char *sourcefile, int destfd, char *destfile,
	BOOLEAN append, BOOLEAN backgrnd)
{
	int pid;

	// 인자가 없거나 사용자 정의 명령을 수행하여 실패하였을 경우
	if (ac == 0 || shellcmd(ac, av, sourcefd, destfd))
	{
		return 0;
	}

	// 프로세스를 fork한다.
	pid = fork();

	switch (pid)
	{
		// 에러가 났을 경우
	case ERROR:
		fprintf(stderr, "Cannot create new process.\n");
		return 0;

		// 자식일때, 프로그램을 실행하는 실체
	case 0:
		redirect(sourcefd, sourcefile, destfd, destfile, append, backgrnd);
		execvp(av[0], av);
		fprintf(stderr, "Cannot execute %s\n", av[0]);
		exit(0);

		// 부모일 때
	default:
		// 읽기 파일디스크립션을 닫는다.
		if (sourcefd > 0 && close(sourcefd) == ERROR)
		{
			syserr("close sourcefd");
		}

		// 쓰기 파일디스크립션을 닫는다.
		if (destfd > 1 && close(destfd) == ERROR)
		{
			syserr("close destfd");
		}

		// 백그라운드 명령일 경우 pic 출력
		if (backgrnd)
		{
			printf("%d\n", pid);
		}

		return pid;
	}



////////////////////////////////////////////////////////////////////////
//  해당인자가 존재하는 확인하는 함수
////////////////////////////////////////////////////////////////////////
int check_arg(char *av[], const char *opt)
{
	int count = 0;

	// 모든 인자값을 확인한다.
	while (*av != '\0')
	{
		// opt 인자가 존재하는가?
		if (!strcmp(av[count], opt))
		{
			return TRUE;
		}

		av++;
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////////////
//  디렉토리를 변경하는 함수
////////////////////////////////////////////////////////////////////////
void cmd_cd(int ac, char *av[])
{
	char *path;

	// 인자가 있을 경우 path를 설정
	if (ac > 1)
	{
		path = av[1];
	}

	// 인자가 없을 경우 HOME디렉토리를 설정
	else if ((path = (char*)getenv("HOME")) == NULL)
	{
		// 환경 변수가 없을 경우 현재 디렉토리로 설정
		path = ".";
	}

	// 디렉토리를 변경한다.
	if (chdir(path) == ERROR)
	{
		fprintf(stderr, "%s: bad directory.\n", path);
	}
}

////////////////////////////////////////////////////////////////////////
//  프로그램을 종료
////////////////////////////////////////////////////////////////////////
void cmd_exit()
{
	exit(1);
}

////////////////////////////////////////////////////////////////////////
//  디렉토리 리스트를 출력하는 함수
////////////////////////////////////////////////////////////////////////
void cmd_ls(int ac, char *av[])
{
	DIR *dp;
	struct dirent *entry;
	char *path;
	int count;
	int opt_a;
	int opt_l;

	// 인자가 없을 경우 자기 자신의 디렉토리로 설정한다.
	if (ac < 2)
	{
		path = ".";
	}

	// 인자가 있을 경우 설정
	else
	{
		path = av[1];
	}

	// 디렉토리를 연다.
	if ((dp = opendir(path)) == NULL)
	{
		fprintf(stderr, "Can't open directory: %s", av[1]);
		return;
	}

	// 다음의 인자들이 존재하는지 확인
	opt_a = check_arg(av, "-a");
	opt_l = check_arg(av, "-l");

	count = 0;

	// 파일이나 디렉토리를 읽어들인다.
	while ((entry = readdir(dp)) != NULL)
	{
		// -a 옵션이 없을 경우 숨김 파일은 표시하지 않는다.
		if (!opt_a)
		{
			if (entry->d_name[0] == '.')
			{
				continue;
			}
		}

		// 출력
		printf("%s\t", entry->d_name);

		// -l 옵션이 설정되어있을 경우 줄마다 한원소씩을 출력한다.
		if (opt_l)
		{
			printf("\n");
		}

		// 한줄에 3개씩 출력한다.
		else
		{
			if (count > 3)
			{
				printf("\n");
				count = 0;
			}
			else
			{
				count++;
			}
		}
	}

	// 디렉토리를 닫는다.
	closedir(dp);
	printf("\n");
}

////////////////////////////////////////////////////////////////////////
//  파일을 복사하는 함수
////////////////////////////////////////////////////////////////////////
void cmd_cp(int ac, char *av[])
{
	FILE *src;
	FILE *dst;
	char ch;

	// 인자가 2개 이하일 경우 에러
	if (ac < 3)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// 복사할 소스 파일을 연다.
	if ((src = fopen(av[1], "r")) == NULL)
	{
		fprintf(stderr, "%s: Can't open file.\n", av[1]);
		return;
	}

	// 쓰기를 할 파일을 연다.
	if ((dst = fopen(av[2], "w")) == NULL)
	{
		fprintf(stderr, "%s: Can't open file.\n", av[2]);
		return;
	}

	// 복사
	while (!feof(src))
	{
		ch = (char)fgetc(src);

		if (ch != EOF)
		{
			fputc((int)ch, dst);
		}
	}

	// -v 옵션이 있을 경우 내용 출력
	if (check_arg(av, "-v"))
	{
		printf("cp %s %s\n", av[1], av[2]);
	}

	fclose(src);
	fclose(dst);
}

////////////////////////////////////////////////////////////////////////
//  파일 삭제 명령 함수
////////////////////////////////////////////////////////////////////////
void cmd_rm(int ac, char *av[])
{
	// 인자가 없을 경우 에러
	if (ac < 2)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// 파일 삭제
	unlink(av[1]);

	// -v 옵션이 있을 경우 결과 출력
	if (check_arg(av, "-v"))
	{
		printf("rm %s\n", av[1]);
	}
}

////////////////////////////////////////////////////////////////////////
//  파일  이동 명령어
////////////////////////////////////////////////////////////////////////
void cmd_mv(int ac, char *av[])
{
	FILE *src;
	FILE *dst;
	char ch;

	// 인자가 2개 이하일 경우 에러
	if (ac < 3)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// 복사할 파일을 연다.
	if ((src = fopen(av[1], "r")) == NULL)
	{
		fprintf(stderr, "%s: Can't open file.\n", av[1]);
		return;
	}

	// 쓰기할 파일을 연다.
	if ((dst = fopen(av[2], "w")) == NULL)
	{
		fprintf(stderr, "%s: Can't open file.\n", av[2]);
		return;
	}

	// 복사
	while (!feof(src))
	{
		ch = (char)fgetc(src);

		if (ch != EOF)
		{
			fputc((int)ch, dst);
		}
	}

	fclose(src);
	fclose(dst);

	// 기존 파일 삭제
	unlink(av[1]);

	// -v 옵션을 사용하면  결과를 출력한다.
	if (check_arg(av, "-v"))
	{
		printf("mv %s %s\n", av[1], av[2]);
	}
}

////////////////////////////////////////////////////////////////////////
//  디렉토리를 생성하는 함수
////////////////////////////////////////////////////////////////////////
void cmd_mkdir(int ac, char *av[])
{
	// 인자가 없을 경우 에러
	if (ac < 2)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// 디렉토리를 생성한다.
	if (mkdir(av[1], 0755))
	{
		fprintf(stderr, "Make directory failed.\n");
	}
}

////////////////////////////////////////////////////////////////////////
//  디렉토리를 삭제하는 함수
////////////////////////////////////////////////////////////////////////
void cmd_rmdir(int ac, char *av[])
{
	// 인자가 없을 경우 에러
	if (ac < 2)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// 디렉토리를 삭제한다.
	if (rmdir(av[1]))
	{
		fprintf(stderr, "Remove directory failed.\n");
	}
}

////////////////////////////////////////////////////////////////////////
//  cat 명령어 함수
////////////////////////////////////////////////////////////////////////
void cmd_cat(int ac, char *av[])
{
	int ch;
	FILE *fp;

	// 인자가 없을 경우 에러처리
	if (ac < 2)
	{
		fprintf(stderr, "Not enough arguments");
		return;
	}

	// 일기전용으로 파일을 연다.
	if ((fp = fopen(av[1], "r")) == NULL)
	{
		fprintf(stderr, "No such file on directory.\n");
		return;
	}

	// 내용 출력
	while ((ch = getc(fp)) != EOF)
	{
		putchar(ch);
	}

	fclose(fp);
}




typedef enum { NEUTRAL, GTGT, INQUOTE, INWORD } STATUS;

////////////////////////////////////////////////////////////////////////
//  문자열을 분석하며 심볼을 찾는 함수
////////////////////////////////////////////////////////////////////////
SYMBOL getsymbol(char *word)
{
	STATUS state;
	int c;
	char *w;

	state = NEUTRAL;
	w = word;

	while ((c = getchar()) != EOF)
	{
		switch (state)
		{
		case NEUTRAL:
			switch (c)
			{
			case ';':
				return S_SEMI;

			case '&':
				return S_AMP;

			case '|':
				return S_BAR;

			case '<':
				return S_LT;

			case '\n':
				return S_NL;

			case ' ':
			case '\t':
				continue;

			case '>':
				state = GTGT;
				continue;

			case '"':
				state = INQUOTE;
				continue;

			default:
				state = INWORD;
				*w++ = c;
				continue;
			}

		case GTGT:
			if (c == '>')
			{
				return S_GTGT;
			}
			ungetc(c, stdin);
			return S_GT;

		case INQUOTE:
			switch (c)
			{
			case '\\':
				*w++ = getchar();
				continue;

			case '"':
				*w = '\0';
				return S_WORD;

			default:
				*w++ = c;
				continue;
			}

		case INWORD:
			switch (c)
			{
			case ';':
			case '&':
			case '|':
			case '<':
			case '>':
			case '\n':
			case ' ':
			case '\t':
				ungetc(c, stdin);
				*w = '\0';
				return S_WORD;

			default:
				*w++ = c;
				continue;
			}
		}
	}

	return S_EOF;
}



////////////////////////////////////////////////////////////////////////
//  메인 함수
////////////////////////////////////////////////////////////////////////
int main()
{
	int pid, fd;
	SYMBOL term;

	// 프롬프트를 출력
	print_prompt();

	// 무한 반복하며 사용자 입력을 받는다.
	while (TRUE)
	{
		// 사용자 명령을 입력받아 분석한다.
		term = parse(&pid, FALSE, NULL);

		// 명령의 마지막 심볼이 &일 경우 백그라운드로 명령을 수행한다. 
		// 즉 wait를 하지 않는다.
		if (term != S_AMP && pid != 0)
		{
			// 백그라운드 명령이 아닌 경우 자식이 프로세스를 종료할때까지 기다린다.
			waitfor(pid);
		}

		// 마지막 문자가 줄바꿈 문자일 경우 새로운 프롬프트를 출력한다.
		if (term == S_NL)
		{
			print_prompt();
		}

		// 파일 디스크립터를 닫는다.
		for (fd=3; fd<MAXFD; fd++)
		{
			close(fd);
		}
	}
}
