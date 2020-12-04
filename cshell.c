/*
 * so.c
 *
 *  Created on: Nov 6, 2020
 *      Author: Minhesota Geusic
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
//require for string operations
#include <string.h>
//require to check if directory exist
#include <dirent.h>
#include <errno.h>
//require for directory structures
#include <dirent.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

char * cmdSeparator [] = {";","|",">",">>"};
char * delimiter = " ";
char * exitCMD = "exit\n";
char cwd [1024];
static volatile sig_atomic_t jump_active = 0;

char replaceAllCharacter(char * str, char target, char replacement){
	if(str == NULL) return 0;
	int i = 0;
	while(str[i] != '\0'){
		if(str[i] == target)
			str[i] = replacement;
		i++;
	}
	return 1;
}
void getUserInput(char * format, char * buffer, int lenght){
	if(format == NULL || buffer == NULL || lenght <= 0) return;
	scanf(format, buffer);
	printf("%s\n", buffer);
}

void cdCMD_Func(char * cmd){
	//cmd coming in is assumed to be in the format
	//cd <file path>
	if(cmd == NULL) return;
	int len = 0;
	char cmdCpy [1024];
	char dirPath [1024];
	char * cd_delimiter = " ";
	char * token = NULL;
	DIR * dir = NULL;
	//copy cmd into a temporary cmd
	//so when we do strtok we dont interfered with cmd
	strcpy (cmdCpy,cmd);
	//retrieve the 0th token (cd)
	token = strtok(cmdCpy, cd_delimiter);
	//retrieve the 1st token (<file path>)
	token = strtok(NULL, cd_delimiter);
	//if only 1 token exist then just return
	if ( token == NULL ) return;
	//copy the second token into directory path
	strcpy((char*) dirPath, token);
	len = strlen(token);
	token = strtok(NULL, cd_delimiter);
	//if there are more than 2 tokens than we need to check
	//if those tokens are part of the dirPath
	//this is probably not needed
	if (token != NULL) {
		while(token != NULL) {
			//this maybe part of the directory path
			//only append it to dirPath if dirPath contain a trailing '\'
			if (dirPath[len - 1] == '\\') {
				//if size of dirPath is greater than the allowed space break;
				if(len >= 1024 || len + 1 >= 1024 || strlen(token) + len + 1 >= 1024) break;
				strcat(dirPath, (char *) (' ' + token) );
				len = strlen(dirPath);
			}else break;
			token = strtok(NULL, cd_delimiter);
		}
	}
	//check if filePath is valid
	dir = opendir(dirPath);
	if(dir){
		//dir exists
		//change the current working directory
		chdir(dirPath);
		getcwd(cwd, sizeof(cwd));
		replaceAllCharacter(cwd, '\\','/');
	} else if (ENOENT == errno){
		//dir does not exist
		printf("No such file or directory\n");
	} else {
		//opendir failed, cannot open file
		//it does exists.
	}
}

int findCMD_Separator(char ** currCMD_Separator, char * input){
	if(input == NULL || currCMD_Separator == NULL) return -1;
	int i = 0;
	char foundSeparator = 0;
	int len = strlen(input);

	for ( i = 0 ; i < len; i ++){
		if(input[i] == ';'){
			*currCMD_Separator = cmdSeparator[0];
			foundSeparator = 1;
			break;
		}else if(input[i] == '|'){
			*currCMD_Separator = cmdSeparator[1];
			foundSeparator = 1;
			break;
		}else if(input[i] == '>'){
			printf("input: %s\tlen: %d\ti: %d\n", input, len, i);
			*currCMD_Separator = cmdSeparator[2];
			if(i + 1 < len && input[i+1] == '>') {
				*currCMD_Separator = cmdSeparator[3];
			}
			printf("selected separator: %s\n", currCMD_Separator);
			foundSeparator = 1;
			break;
		}
	}
	if(foundSeparator){
		return i;
	}
	return -1;
}

void execCMD (char * cmdStr, int len, char * out, int * outLen, char * outFilled, char * prevOut, char * prevCMDSeparator){
	char cmdStrCpy [1024];
	char * args [1024];
	char * cmd = NULL;
	char * token = NULL;
	char * delim = " ";
	int link[2];
	int i = 0;
	int prevOutLen = 0;
	FILE * inFD = 0;
	pid_t pid;
	//need to set everything null, since some memory carry over
	//because it is statically allocated
	memset(args, 0, 1024);
	memset(cmdStrCpy, 0, 1024);
	//copy the cmd str into the copy version
	memcpy(cmdStrCpy, cmdStr, len);
	cmd = strtok(cmdStrCpy, delim);
	token = cmd;
	if(cmd == NULL) return;
	//if the previous cmd was a redirection
	//than this current cmd is a file (or soon to be file)
	if (prevCMDSeparator != NULL) {
		//if the previous cmd separator was a redirection (>)
		//then this current cmd is a file (or soon to be file)
		if (strcmp(prevCMDSeparator, cmdSeparator[2]) == 0) {
			//overwrite the file specified in cmd and copy the previous out to
			//curr out and return
			inFD = fopen(cmd, "w");
			//if prevOut is not null then write to file
			if (prevOut != NULL)
				fprintf(inFD, "%s", prevOut);
			//close the file
			fclose(inFD);
			//if prevOut is not null then copy that to the current out
			if (prevOut != NULL) {
				memcpy(out, prevOut, strlen(prevOut) * sizeof(char));
			}
			if(strlen(out) > 0) *outFilled = 1;
			return;
		}
		//if the previous cmd was an redirect and append (>>)
		//then this current cmd is a file (or soon to be file)
		else if (strcmp(prevCMDSeparator, cmdSeparator[3]) == 0) {
			//append the prev output to the file, and read the content of the file
			// including the prev output, then copy that to the curr out and return
			char buffer [2048];
			//open the file
			inFD = fopen(cmd, "a");
			if (inFD == NULL) {
				printf("There was an error attempting to open the file\n");
				return;
			}
			//if we have a previous output than append it to the file
			if (prevOut != NULL)
				fprintf(inFD, "%s", prevOut);
			//close the file
			fclose(inFD);
			//reopen the file so we can read the content for next output
			inFD = fopen(cmd, "r");

			if(inFD == NULL) return;
			while (fgets(buffer, sizeof(buffer), inFD) != NULL ) {
				printf("content: %s\n", buffer);
				strcat(out, buffer);
			}
			//close the file
			fclose(inFD);
			if(strlen(out) > 0) *outFilled = 1;

			//return since there is nothign else to dod
			return;
		}
	}
	//create the args including the cmd itself as the first args
	while(token != NULL){
		args[i] = token;
		token = strtok(NULL, delim);
		//printf("%d\n", i);
		i++;
	}
	//according to execvp the args array must contain a NULL value at the end
	args[i] = NULL;
	//create a pipeline so parent can talk to child process
	if(pipe(link) == -1){
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	//debug
	/*printf("%s, %d\n", cmdStrCpy, len);
	printf("cmd: %s\n", cmd);
	int c = 0;
	for (c = 0; c < i; c++) {
		printf("args[%d]: %s\n", c, args[c]);
	}*/

	pid = fork();
	//if child process execute command
	if(pid == 0){
		//set the child's process to be the stdout to this process
		if(prevCMDSeparator != NULL && strcmp(prevCMDSeparator, cmdSeparator[1]) == 0){
			args[i] = prevOut;
			args[i+1] = NULL;
		}
		dup2(link[1], STDOUT_FILENO);
		close(link[1]);
		close(link[0]);
		//execute command
		execvp(cmd, args);
		exit(0);
	}else{
		close(link[1]);
		*outLen = read(link[0], out, *outLen);
		if(*outLen > 0) *outFilled = 1;
		wait(NULL);
		close(link[0]);
	}
}
void AnalyzeInput(char * input){
	char * currCMDSeparator = NULL;
	char * prevCMDSeparator = NULL;
	char prevOut [4096];
	char currOut [4096];
	char inputCPY [1024];
	char * token = NULL;
	char containCurrOut = 0;
	int lenOfCurrCMD = 0;
	int outLen = 2048;
	int currCMD_Separator_index = 0;

	memset(prevOut, 0, 4096 * sizeof(char));
	memset(currOut, 0, 4096 * sizeof(char));

	memcpy(inputCPY, input, 1024 * sizeof(char));

	token = strtok(inputCPY, delimiter);

	while(token != NULL){
		//get the index of the next separator of this current input
		//as well as recoding what the separator was
		currCMD_Separator_index = findCMD_Separator (&currCMDSeparator, input);
		lenOfCurrCMD = currCMD_Separator_index;
		//printf("\nprevious cmd separator: %s\n", prevCMDSeparator);
		//printf("previous output: %s\n", prevOut);
		//if there exists no separator then execute only one command
		if(lenOfCurrCMD < 0){
			//printf("called here 2nd\n");
			containCurrOut = 0;
			outLen = 4096;
			memset(currOut, 0, 4096);
			//no separator, we can use whatever its left of the input
			//since it is only one command
			if(strcmp(token,"cd") == 0){
				cdCMD_Func(input);
				break;
			}
			//if it is something else then we exec

			//there were no previous command that precede us or
			//the previous command was a semicolon so we can
			//just exec the current command and print out any output
			if(prevCMDSeparator == NULL || strcmp(prevCMDSeparator, cmdSeparator[0]) == 0){
				execCMD(input, strlen(input), currOut, &outLen, &containCurrOut, NULL, NULL);
				if(containCurrOut == 1) printf("%s", currOut);
				return;
			}
			//if the prevCMDSeparator was "|",">",">>" then we
			//need to use the previous output as well
			execCMD(input, strlen(input), currOut, &outLen, &containCurrOut, prevOut, prevCMDSeparator);
			if(containCurrOut == 1) printf("%s", currOut);
			input = input + strlen(input);
			token = NULL;
		}else{
			//printf("called here\n");
			//reset current output
			//and outlen
			containCurrOut = 0;
			outLen = 4096;
			memset(currOut, 0, 4096);

			if (strcmp(token, "cd") == 0) {
				cdCMD_Func(input);
				memcpy(currOut, cwd, strlen(cwd));
				break;
			}

			//at least one separator, ie there are 2 command present
			//left cmd and right cmd
			execCMD(input, lenOfCurrCMD, currOut, &outLen, &containCurrOut, prevOut, prevCMDSeparator);

			//go to next cmd by the length of this current cmd
			input = input + lenOfCurrCMD + strlen(currCMDSeparator);
			//find the next token in the next command based on the delimiter
			token = strtok(inputCPY + lenOfCurrCMD + strlen(currCMDSeparator), delimiter);
			//printf("next input: %s\n", input);
			//printf("next token: %s\n", token);
			//printf("current cmd separator: %s\t%d\n", currCMDSeparator, strlen(currCMDSeparator));
			//set the previous cmd separator to the current cmd separator
			prevCMDSeparator = currCMDSeparator;
			//copy the curr out to the prev out
			memcpy(prevOut, currOut, 2048 * sizeof(char));
			currCMDSeparator = NULL;
		}
	}
}
static sigjmp_buf env;

void sigint_handler(int signo){
	if(!jump_active){
		return;
	}
	printf("%c",'\n');
	siglongjmp(env, 42);
}
int main() {
	char input [1024];
	getcwd(cwd,sizeof(cwd));
	replaceAllCharacter(cwd, '\\','/');

	//take care of the sigaction to restart the loop	
	struct sigaction s;
    	s.sa_handler = sigint_handler;
    	sigemptyset(&s.sa_mask);
    	s.sa_flags = SA_RESTART;
    	sigaction(SIGINT, &s, NULL);

	while(1){
		if(sigsetjmp(env,1)!=42){
			jump_active =1;
		}
		printf("%s $ ",cwd);
		//get input from user
		fgets(input, 1024, stdin);
		//return if we encounter exit cmd
		if(strcmp(input,exitCMD) == 0) break;

		replaceAllCharacter(input,'\n','\0');
		//analyze token
		AnalyzeInput(input);
		memset(input, 0, 1024);
	}

	return 0;
}
