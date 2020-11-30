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

//require for string operations
#include <string.h>
//require to check if directory exist
#include <dirent.h>
#include <errno.h>

typedef enum CMD_INDEX { lsCMD = 0, cdCMD = 1, exitCMD = 2, echoCMD = 3 } CMD_INDEX;

char * cmdSeparator [] = {";","|",">",">>"};
char * validCMD [] = {"ls", "cd", "exit\n", "echo"};
char * delimiter = " ";

char cwd [1024];

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
	char dirPath [1024];
	char cmdCpy [1024];
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
				strcat(dirPath + len, (char *) (' ' + token) );
				len = strlen(dirPath);
			}else break;
			token = strtok(NULL, cd_delimiter);
		}
	}
	//only 2 token exists
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

void lsCMD_Input(char * cmd){

}

int findCMD_Separator(char * currCMD_Separator, char * input){
	if(input == NULL || currCMD_Separator == NULL) return -1;
	int i = 0;
	char foundSeparator = 0;
	int len = strlen(input);
	for ( i = 0 ; i < len; i ++){
		if(input[i] == ';'){
			*currCMD_Separator = ';';
			foundSeparator = 1;
			break;
		}else if(input[i] == '|'){
			*currCMD_Separator = '|';
			foundSeparator = 1;
			break;
		}else if(input[i] == '>'){
			*currCMD_Separator = '>';
			if(i + 1 < len && input[i+1] == '>') {
				*(currCMD_Separator + 1) = '>';
			}
			foundSeparator = 1;
			break;
		}
	}
	if(foundSeparator){
		return i;
	}
	return -1;
}
void AnalyzeInput(char * input){
	char * currCMD;
	char currCMD_Str [1024];
	char currCMD_Separator [10];
	char * inputCpy [1024];
	char * token;
	int len = 0;
	int currCMD_Separator_index = 0;

	strcpy(inputCpy, input);
	token = strtok(inputCpy, delimiter);

	while(token != NULL){

		//TODO: change input for each iteration so it goes to the next command

		currCMD_Separator_index = findCMD_Separator (currCMD_Separator, input);
		len = currCMD_Separator_index;
		if(len < 0){
			//no separator, we can use whatever its left of the input
			//since it is only one command
			if(strcmp(token,"cd") == 0){
				cdCMD_Func(input);
			}
		}else{
			//at least one separator, ie there are 2 command present
			//left cmd and right cmd
		}
		return;
	}
}

int main() {
	char input [1024];
	getcwd(cwd,sizeof(cwd));
	replaceAllCharacter(cwd, '\\','/');

	while(1){
		printf("%s $ ",cwd);
		//get input from user
		fgets(input, 1024, stdin);
		//return if we encounter exit cmd
		if(strcmp(input,validCMD[exitCMD]) == 0) break;

		replaceAllCharacter(input,'\n','\0');
		//debug
		printf("\nyou've entered %s\n", input, strlen(input));

		//analyze token
		AnalyzeInput(input);
		printf("%s\n", cwd);
	}

	return 0;
}
