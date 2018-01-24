// UNIX
#define _POSIX_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
// STD
#include <stdio.h>
#include <stdlib.h>
// Others
#include <string.h>
#include <assert.h>
#include "svec.h"
#include "tokenize.h"

char* stringSlice(char* string, int start, int end)
{
    char* newString;
    if (end - start > 0)
    {
        newString = (char*)malloc(sizeof(char) * (end - start + 2));
        memcpy(newString, string + (sizeof(char) * start), sizeof(char) * (end - start + 1));
        newString[end - start + 1] = 0;
    }
    else
    {
        newString = malloc(sizeof(char));
        newString[0] = 0;
    }

    return newString;
}

int min(int n1, int n2)
{
    if (n1 > n2)
    {
        return n2;
    } else
    {
        return n1;
    }
}

int equalsWithPadding(char* inString, char* lookFor)
{
    for (int ii = 0; ii < strlen(inString); ii++)
    {
        if (!(inString[ii] == ' '))
        {
            int eq = 0;
            for (int jj = 0; jj < min(strlen(lookFor), strlen(inString) - ii); jj++)
            {
                if (lookFor[jj] != inString[ii+jj])
                {
                    return -1;
                }
            }
            return 0;
        }
    }

    return -1;
}

void execute(char* cmd)
{
    // set up piping
    int shouldPipe = 0;
    int pipes[2];
    char* prePipe;
    int p_read;
    int p_write;
    char* postPipe;

    // variable for redirection
    int outputFile;
    int inputFile;

    // variables for booleans
    int orCase = 0;
    int andCase = 0;
    char* beforeBool;
    char* afterBool;

    // parse the tokens
    for (int ii = 0; ii < strlen(cmd); ii++)
    {
        if (cmd[ii] == ';')
        {
            // execute the previous part
            char* prev = stringSlice(cmd, 0, ii-1);
            execute(prev);
            free(prev);

            // change cmd to the part after
            char* tempStr = stringSlice(cmd, ii + 1, strlen(cmd));
            execute(tempStr);
            free(tempStr);
            return ;

        }
    }

    for (int ii = 0; ii < strlen(cmd); ii++)
    {
        if (cmd[ii] == '|' && ii + 1 < strlen(cmd) && cmd[ii+1] == '|')
        {
            orCase = 1;
            beforeBool = stringSlice(cmd, 0, ii - 1);
            afterBool = stringSlice(cmd, ii + 2, strlen(cmd) - 1);
            break;
        }
        else if (cmd[ii] == '&' && ii + 1 < strlen(cmd) && cmd[ii+1] == '&')
        {
            andCase = 1;
            beforeBool = stringSlice(cmd, 0, ii - 1);
            afterBool = stringSlice(cmd, ii + 2, strlen(cmd) - 1);
            break;
        }
        else if (cmd[ii] == '>')
        {
            shouldPipe = 2;
            pipe(pipes);
            p_read = pipes[0];
            p_write = pipes[1];
            prePipe = stringSlice(cmd, 0, ii-1);

            int startIndex = ii + 1;
            for (int kk = ii + 1; kk < strlen(cmd); kk++)
            {
                if (cmd[kk] != ' ')
                {
                    startIndex = kk;
                    break;
                }
            }

            char* fileName = stringSlice(cmd, startIndex, strlen(cmd) - 1);
            outputFile = open(fileName, O_CREAT | O_TRUNC |  O_RDWR, 0644);
            if (outputFile == -1)
            {
                perror("Can't open file");
            }

            free(fileName);

            break;
        } else if (cmd[ii] == '<')
        {
            shouldPipe = 3;
            pipe(pipes);
            p_read = pipes[0];
            p_write = pipes[1];
            prePipe = stringSlice(cmd, 0, ii - 1);

            int startIndex = ii + 1;
            for (int kk = ii + 1; kk < strlen(cmd); kk++)
            {
                if (cmd[kk] != ' ')
                {
                    startIndex = kk;
                    break;
                }
            }

            char* fileName = stringSlice(cmd, startIndex, strlen(cmd) - 1);
            inputFile = open(fileName, O_RDONLY);
            free(fileName);

            break;

        } else if (cmd[ii] == '|')
        {
            // lets get piping boys
            shouldPipe = 1;
            pipe(pipes);
            p_read = pipes[0];
            p_write = pipes[1];
            prePipe = stringSlice(cmd, 0, ii - 1);
            postPipe = stringSlice(cmd, ii + 1, strlen(cmd)-1);
            break;
        } else if (cmd[ii] == '&') // fix this, its messed up when it quits
        {
            int cpid2;

            if ((cpid2 = fork()))
            {
                char* after = stringSlice(cmd, ii + 1, strlen(cmd) - 1);
                execute(after);
                free(after);

            }
            else
            {
                char* before = stringSlice(cmd, 0, ii - 1);
                execute(before);
                free(before);

                kill(getpid(), SIGKILL);
            }

            return ;
        }
    }

    // handle cd
    if (strlen(cmd) > 3 && equalsWithPadding(cmd, "cd") == 0)
    {
        char* toDir = stringSlice(cmd, 3, strlen(cmd) - 1);
        // change path
        chdir(toDir);
        free(toDir);
        return ;
    }

    // handle exit
    if (equalsWithPadding(cmd, "exit") == 0)
    {
        exit(0);
    }

    svec* tokenized;

    if (shouldPipe > 0)
    {
        tokenized = tokenize(prePipe);
        free(prePipe);
    }
    else if (orCase == 1 || andCase == 1)
    {
        tokenized = tokenize(beforeBool);
        free(beforeBool);
    }
    else
    {
        tokenized = tokenize(cmd);
    }

    int cpid;


    if ((cpid = fork())) {

        int status;
        waitpid(cpid, &status, 0);
        free_svec(tokenized);

        if (shouldPipe == 1)
        {
            int stdincopy = dup(0);

            close(p_write);
            close(0);
            dup(p_read);
            // execute pre
            execute(postPipe);
            free(postPipe);

            close(p_read);
            dup2(stdincopy, 0);
            close(stdincopy);
        }

        if (orCase)
        {
            if (status != 0)
            {
                execute(afterBool);
            }
            free(afterBool);
        }

        if (andCase)
        {
            if (status == 0)
            {
                execute(afterBool);
            }
            free(afterBool);
        }

    } /**            !!!          END OF PARENT        !!!          **/
    else {

        if (shouldPipe == 1)
        {
            free(postPipe);
            close(p_read);
            close(1);
            dup(p_write);
        }

        if (shouldPipe == 2)
        {
            close(p_read);
            close(1);
            dup2(outputFile, 1);
        }

        if (shouldPipe == 3)
        {
            close(p_write);
            dup2(inputFile, 0);
        }

        if (orCase == 1 || andCase == 1)
        {
            free(afterBool);
        }

        int numberOfArgs = tokenized->size;
        char** args = malloc(sizeof(char*) * (numberOfArgs + 1));
        args = tokenized->data;

        execvp(args[0], args);

        perror("fail");
        printf("Can't get here, exec only returns on error.");
    }
}

int main(int argc, char* argv[])
{
	if (argc == 1) {

        for(;;)
	    {
    	    char cmd[256];

			printf("nush$ ");
			fflush(stdout);
			if (fgets(cmd, 256, stdin) == NULL)
            {
                return 0;
            }

            execute(cmd);
		}
    }
	else {
        // go through each line and execute
        char* fileName = argv[1];
        FILE* myFile = fopen(fileName, "r");
        char line[512];

        while (fgets(line, sizeof(char) * 512, myFile) != NULL)
        {
            line[strlen(line) - 1] = 0;
            execute(line);
        }

        fclose(myFile);
    }
	return 0;
}
