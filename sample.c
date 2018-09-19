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
//  ġ������ ���� ó��
////////////////////////////////////////////////////////////////////////
void fatal(char *message)
{
	fprintf(stderr, "Error: %s\n", message);
	exit(1);
}

////////////////////////////////////////////////////////////////////////
//  �Ϲ� ���� ó��
////////////////////////////////////////////////////////////////////////
void syserr(char *message)
{
	fprintf(stderr, "Error: %s (%d", message, errno);
	exit(1);
}

////////////////////////////////////////////////////////////////////////
//  �θ� �ڽ��� ������ ������ ��ٸ��� �Լ�
////////////////////////////////////////////////////////////////////////
void waitfor(int pid)
{
	int wpid, status;

	// wait�� �̿��Ͽ� ���μ����� ��ٸ���.
	while ((wpid = wait(&status)) != pid && wpid != ERROR);
}

////////////////////////////////////////////////////////////////////////
//  ������Ʈ�� ����Ѵ�.
////////////////////////////////////////////////////////////////////////
void print_prompt()
{
	char *ps;
	char *index;

	// PS2 ȯ�� ������ �о� ����Ѵ�.
	if ((ps = (char*)getenv("PS2")) != NULL)
	{
		// ȯ�� ������ ���� ���������� �ѹ���Ʈ�� �̵��ϸ� Ȯ��
		while (*ps != '\0')
		{
			// ��������(\)�� �ִٸ� ������ ���ڸ� Ȯ���Ѵ�.
			if (*ps == '\\')
			{
				ps++;

				// ����� ������ ���
				if (*ps == 'u')
				{
					printf("%s", getenv("USER"));
				}

				// ȣ��Ʈ���� ���
				else if (*ps == 'h')
				{
					printf("%s", getenv("HOSTNAME"));
				}

				// ���� ���丮�� ����Ѵ�.
				else if (*ps == 'w')
				{
					printf("%s", get_current_dir_name());
				}
			}

			// �ƹ��͵� �ƴ� ��� ���ڸ� ���
			else
			{
				printf("%c", *ps);
				ps++;
			}
		}
	}

	// PS2 ȯ�� ������ ���ٸ� �⺻ ������Ʈ�� ����Ѵ�.
	else
	{
		printf(">> ");
	}
}

////////////////////////////////////////////////////////////////////////
//  ����� ���� ����� ó���ϴ� �Լ�
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
//  ǥ�� ��/����� �̿��� �����̷�Ʈ �Լ�
////////////////////////////////////////////////////////////////////////
void redirect(int sourcefd, char *sourcefile, int destfd, char *destfile, BOOLEAN append, BOOLEAN backgrnd)
{
	int flags, fd;

	// �����ִ� �ҽ� ���ϵ�ũ���Ͱ� ���� ��׶��� ������ ���
	if (sourcefd == 0 && backgrnd)
	{
		strcpy(sourcefile, "/dev/null");
		sourcefd = BADFD;
	}

	// �ҽ� ���� ��ũ���Ͱ� ���� ���
	if (sourcefd != 0)
	{
		// ǥ���Է��� �ݴ´�.
		if (close(0) == ERROR)
		{
			syserr("close");
		}

		if (sourcefd > 0)
		{
			// ǥ���Է��� �������Ѵ�.
			if (dup(sourcefd) != 0)
			{
				fatal("dup");
			}
		}

		// �ҽ������� ����.
		else if (open(sourcefile, O_RDONLY, 0) == ERROR)
		{
			fprintf(stderr, "Cannot open %s\n", sourcefile);
			exit(0);
		}
	}

	// ��� ���� ��ũ���Ͱ� �ִٸ�
	if (destfd != 1)
	{
		// ǥ�� ����� �ݴ´�.
		if (close(1) == ERROR)
		{
			syserr("close");
		}

		// ǥ�� ����� ������
		if (destfd > 1)
		{
			if (dup(destfd) != 1)
			{
				fatal("dup");
			}
		}

		else
		{
			// ������ ���� ��쿡 �����ϰ�, ���⵵ �����ϰ� ������ ����
			flags = O_WRONLY | O_CREAT;

			// �߰� ��尡 �ƴ� ��� ������ ���� �ɼǵ� �߰�
			if (!append)
			{
				flags |= O_TRUNC;
			}

			// ������ ����.
			if (open(destfile, flags, 0666) == ERROR)
			{
				fprintf(stderr, "Cannot create %s\n", destfile);
				exit(0);
			}

			// �߰� ����� ���� ���� ã�´�.
			if (append)
			{
				if (lseek(1, 0L, 2) == ERROR) syserr("lseek");
			}
		}
	}

	// ���ϵ�ũ���͸� �ݴ´�.
	for (fd = 3; fd < MAXFD; fd++)
	{
		close(fd);
	}

	return;
}




////////////////////////////////////////////////////////////////////////
//  ��ɾ �Ľ��Ѵ�.
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
		// �ϳ��� �ܾ�� �м��Ѵ�.
		switch (symbol = getsymbol(word))
		{
			// �Ϲ� ������ ���
		case S_WORD:
			if (argc == MAXARG)
			{
				fprintf(stderr, "Too many args.\n");
				break;
			}

			// ���ο� ���� �迭�� �޸� �Ҵ� �Ѵ�.
			argv[argc] = (char *)malloc(strlen(word) + 1);

			if (argv[argc] == NULL)
			{
				fprintf(stderr, "Out of arg memory.\n");
				break;
			}

			// ��ɾ� ����
			strcpy(argv[argc], word);

			// arg ī���� ����
			argc++;
			continue;

			// < �� ���
		case S_LT:

			// �������� �����ִٸ� ����
			if (makepipe)
			{
				fprintf(stderr, "Extra <.\n");
				break;
			}

			// �ҽ������� �ɺ����� �˻��Ѵ�.
			if (getsymbol(sourcefile) != S_WORD)
			{
				fprintf(stderr, "Illegal <.\n");
				break;
			}

			sourcefd = BADFD;
			continue;

			// > Ȥ�� >> �� ���
		case S_GT:
		case S_GTGT:

			// ���� ������ ���ǵǾ����� ���� ��� ����
			if (destfd != 1)
			{
				fprintf(stderr, "Extra > or >>.\n");
				break;
			}

			// ������ ������ �ɺ�Ÿ���� ���ڿ��� �ƴϸ� ����
			if (getsymbol(destfile) != S_WORD)
			{
				fprintf(stderr, "Illegal > or >>.\n");
				break;
			}

			// GTGT�� ��� �߰� ����
			destfd = BADFD;
			append = (symbol == S_GTGT);
			continue;

			// |, &, ;, �ٹٲ� ������ ��� - �ϳ��� ��ɾ� ����
		case S_BAR:
		case S_AMP:
		case S_SEMI:
		case S_NL:

			argv[argc] = NULL;
			// �ɺ��� ������(|) �� ���
			if (symbol == S_BAR)
			{
				if (destfd != 1)
				{
					fprintf(stderr, "> or >> conflicts with |.\n");
					break;
				}

				// ������ ǥ����� ��ũ���͸� �Բ� �Ѱ� ��ɾ �м��Ѵ�.
				term = parse(waitpid, TRUE, &destfd);
			}

			// ���� ���� ����
			else
			{
				term = symbol;
			}

			// �������� �������� ��� �������� �����Ѵ�.
			if (makepipe)
			{
				if (pipe(pipefd) == ERROR)
				{
					syserr("pipe");
				}
				*pipefdp = pipefd[1];
				sourcefd = pipefd[0];
			}

			// ����� �����Ѵ�.
			pid = execute(argc, argv, sourcefd, sourcefile,
				destfd, destfile, append, term == S_AMP);

			// �������� �ƴ� ��� ��ٸ� PID�� ����
			if (symbol != S_BAR)
			{
				*waitpid = pid;
			}

			// ���ڰ��� ���� ���
			if (argc == 0 && (symbol != S_NL || sourcefd > 1))
			{
				fprintf(stderr, "Missing command.\n");
			}

			// ���ڷ� �Էµ� ������ �޸� ����
			while (--argc >= 0)
			{
				free(argv[argc]);
			}

			return term;

			// ����� �߸��Ǿ��� ��� ����
		case S_EOF:
			exit(0);
		}
	}
}



////////////////////////////////////////////////////////////////////////
//  ��ɾ �����ϴ� �Լ�
////////////////////////////////////////////////////////////////////////
int execute(int ac, char *av[], int sourcefd, char *sourcefile, int destfd, char *destfile,
	BOOLEAN append, BOOLEAN backgrnd)
{
	int pid;

	// ���ڰ� ���ų� ����� ���� ����� �����Ͽ� �����Ͽ��� ���
	if (ac == 0 || shellcmd(ac, av, sourcefd, destfd))
	{
		return 0;
	}

	// ���μ����� fork�Ѵ�.
	pid = fork();

	switch (pid)
	{
		// ������ ���� ���
	case ERROR:
		fprintf(stderr, "Cannot create new process.\n");
		return 0;

		// �ڽ��϶�, ���α׷��� �����ϴ� ��ü
	case 0:
		redirect(sourcefd, sourcefile, destfd, destfile, append, backgrnd);
		execvp(av[0], av);
		fprintf(stderr, "Cannot execute %s\n", av[0]);
		exit(0);

		// �θ��� ��
	default:
		// �б� ���ϵ�ũ������ �ݴ´�.
		if (sourcefd > 0 && close(sourcefd) == ERROR)
		{
			syserr("close sourcefd");
		}

		// ���� ���ϵ�ũ������ �ݴ´�.
		if (destfd > 1 && close(destfd) == ERROR)
		{
			syserr("close destfd");
		}

		// ��׶��� ����� ��� pic ���
		if (backgrnd)
		{
			printf("%d\n", pid);
		}

		return pid;
	}



////////////////////////////////////////////////////////////////////////
//  �ش����ڰ� �����ϴ� Ȯ���ϴ� �Լ�
////////////////////////////////////////////////////////////////////////
int check_arg(char *av[], const char *opt)
{
	int count = 0;

	// ��� ���ڰ��� Ȯ���Ѵ�.
	while (*av != '\0')
	{
		// opt ���ڰ� �����ϴ°�?
		if (!strcmp(av[count], opt))
		{
			return TRUE;
		}

		av++;
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////////////
//  ���丮�� �����ϴ� �Լ�
////////////////////////////////////////////////////////////////////////
void cmd_cd(int ac, char *av[])
{
	char *path;

	// ���ڰ� ���� ��� path�� ����
	if (ac > 1)
	{
		path = av[1];
	}

	// ���ڰ� ���� ��� HOME���丮�� ����
	else if ((path = (char*)getenv("HOME")) == NULL)
	{
		// ȯ�� ������ ���� ��� ���� ���丮�� ����
		path = ".";
	}

	// ���丮�� �����Ѵ�.
	if (chdir(path) == ERROR)
	{
		fprintf(stderr, "%s: bad directory.\n", path);
	}
}

////////////////////////////////////////////////////////////////////////
//  ���α׷��� ����
////////////////////////////////////////////////////////////////////////
void cmd_exit()
{
	exit(1);
}

////////////////////////////////////////////////////////////////////////
//  ���丮 ����Ʈ�� ����ϴ� �Լ�
////////////////////////////////////////////////////////////////////////
void cmd_ls(int ac, char *av[])
{
	DIR *dp;
	struct dirent *entry;
	char *path;
	int count;
	int opt_a;
	int opt_l;

	// ���ڰ� ���� ��� �ڱ� �ڽ��� ���丮�� �����Ѵ�.
	if (ac < 2)
	{
		path = ".";
	}

	// ���ڰ� ���� ��� ����
	else
	{
		path = av[1];
	}

	// ���丮�� ����.
	if ((dp = opendir(path)) == NULL)
	{
		fprintf(stderr, "Can't open directory: %s", av[1]);
		return;
	}

	// ������ ���ڵ��� �����ϴ��� Ȯ��
	opt_a = check_arg(av, "-a");
	opt_l = check_arg(av, "-l");

	count = 0;

	// �����̳� ���丮�� �о���δ�.
	while ((entry = readdir(dp)) != NULL)
	{
		// -a �ɼ��� ���� ��� ���� ������ ǥ������ �ʴ´�.
		if (!opt_a)
		{
			if (entry->d_name[0] == '.')
			{
				continue;
			}
		}

		// ���
		printf("%s\t", entry->d_name);

		// -l �ɼ��� �����Ǿ����� ��� �ٸ��� �ѿ��Ҿ��� ����Ѵ�.
		if (opt_l)
		{
			printf("\n");
		}

		// ���ٿ� 3���� ����Ѵ�.
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

	// ���丮�� �ݴ´�.
	closedir(dp);
	printf("\n");
}

////////////////////////////////////////////////////////////////////////
//  ������ �����ϴ� �Լ�
////////////////////////////////////////////////////////////////////////
void cmd_cp(int ac, char *av[])
{
	FILE *src;
	FILE *dst;
	char ch;

	// ���ڰ� 2�� ������ ��� ����
	if (ac < 3)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// ������ �ҽ� ������ ����.
	if ((src = fopen(av[1], "r")) == NULL)
	{
		fprintf(stderr, "%s: Can't open file.\n", av[1]);
		return;
	}

	// ���⸦ �� ������ ����.
	if ((dst = fopen(av[2], "w")) == NULL)
	{
		fprintf(stderr, "%s: Can't open file.\n", av[2]);
		return;
	}

	// ����
	while (!feof(src))
	{
		ch = (char)fgetc(src);

		if (ch != EOF)
		{
			fputc((int)ch, dst);
		}
	}

	// -v �ɼ��� ���� ��� ���� ���
	if (check_arg(av, "-v"))
	{
		printf("cp %s %s\n", av[1], av[2]);
	}

	fclose(src);
	fclose(dst);
}

////////////////////////////////////////////////////////////////////////
//  ���� ���� ��� �Լ�
////////////////////////////////////////////////////////////////////////
void cmd_rm(int ac, char *av[])
{
	// ���ڰ� ���� ��� ����
	if (ac < 2)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// ���� ����
	unlink(av[1]);

	// -v �ɼ��� ���� ��� ��� ���
	if (check_arg(av, "-v"))
	{
		printf("rm %s\n", av[1]);
	}
}

////////////////////////////////////////////////////////////////////////
//  ����  �̵� ��ɾ�
////////////////////////////////////////////////////////////////////////
void cmd_mv(int ac, char *av[])
{
	FILE *src;
	FILE *dst;
	char ch;

	// ���ڰ� 2�� ������ ��� ����
	if (ac < 3)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// ������ ������ ����.
	if ((src = fopen(av[1], "r")) == NULL)
	{
		fprintf(stderr, "%s: Can't open file.\n", av[1]);
		return;
	}

	// ������ ������ ����.
	if ((dst = fopen(av[2], "w")) == NULL)
	{
		fprintf(stderr, "%s: Can't open file.\n", av[2]);
		return;
	}

	// ����
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

	// ���� ���� ����
	unlink(av[1]);

	// -v �ɼ��� ����ϸ�  ����� ����Ѵ�.
	if (check_arg(av, "-v"))
	{
		printf("mv %s %s\n", av[1], av[2]);
	}
}

////////////////////////////////////////////////////////////////////////
//  ���丮�� �����ϴ� �Լ�
////////////////////////////////////////////////////////////////////////
void cmd_mkdir(int ac, char *av[])
{
	// ���ڰ� ���� ��� ����
	if (ac < 2)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// ���丮�� �����Ѵ�.
	if (mkdir(av[1], 0755))
	{
		fprintf(stderr, "Make directory failed.\n");
	}
}

////////////////////////////////////////////////////////////////////////
//  ���丮�� �����ϴ� �Լ�
////////////////////////////////////////////////////////////////////////
void cmd_rmdir(int ac, char *av[])
{
	// ���ڰ� ���� ��� ����
	if (ac < 2)
	{
		fprintf(stderr, "Not enough arguments.\n");
		return;
	}

	// ���丮�� �����Ѵ�.
	if (rmdir(av[1]))
	{
		fprintf(stderr, "Remove directory failed.\n");
	}
}

////////////////////////////////////////////////////////////////////////
//  cat ��ɾ� �Լ�
////////////////////////////////////////////////////////////////////////
void cmd_cat(int ac, char *av[])
{
	int ch;
	FILE *fp;

	// ���ڰ� ���� ��� ����ó��
	if (ac < 2)
	{
		fprintf(stderr, "Not enough arguments");
		return;
	}

	// �ϱ��������� ������ ����.
	if ((fp = fopen(av[1], "r")) == NULL)
	{
		fprintf(stderr, "No such file on directory.\n");
		return;
	}

	// ���� ���
	while ((ch = getc(fp)) != EOF)
	{
		putchar(ch);
	}

	fclose(fp);
}




typedef enum { NEUTRAL, GTGT, INQUOTE, INWORD } STATUS;

////////////////////////////////////////////////////////////////////////
//  ���ڿ��� �м��ϸ� �ɺ��� ã�� �Լ�
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
//  ���� �Լ�
////////////////////////////////////////////////////////////////////////
int main()
{
	int pid, fd;
	SYMBOL term;

	// ������Ʈ�� ���
	print_prompt();

	// ���� �ݺ��ϸ� ����� �Է��� �޴´�.
	while (TRUE)
	{
		// ����� ����� �Է¹޾� �м��Ѵ�.
		term = parse(&pid, FALSE, NULL);

		// ����� ������ �ɺ��� &�� ��� ��׶���� ����� �����Ѵ�. 
		// �� wait�� ���� �ʴ´�.
		if (term != S_AMP && pid != 0)
		{
			// ��׶��� ����� �ƴ� ��� �ڽ��� ���μ����� �����Ҷ����� ��ٸ���.
			waitfor(pid);
		}

		// ������ ���ڰ� �ٹٲ� ������ ��� ���ο� ������Ʈ�� ����Ѵ�.
		if (term == S_NL)
		{
			print_prompt();
		}

		// ���� ��ũ���͸� �ݴ´�.
		for (fd=3; fd<MAXFD; fd++)
		{
			close(fd);
		}
	}
}
