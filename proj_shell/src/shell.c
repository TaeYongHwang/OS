/*
이 파일은 쉘을 구현한 파일입니다.
@author TaeYong Hwang
@since 2019.3.22
*/
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#define MAX_CMD_NUM  1024
const int MAX_CHAR_NUM = 1024;


char ** TransformWithCharP(char * origin);

int main(int argc, char * argv[])
{

	int i;
	int str_len;
	char *input_value;			//입력을 받을 변수
	char *parsing_ptr;			//";"기준으로 strtok한 ptr값 저장
	char *parsing_arr[MAX_CMD_NUM] ={NULL, };   //";"로 나누어진 포인터를 저장할 변수
	int arr_cnt = 0;			//";"기준으로나누어진 갯수저장.
	char **cmd;				//function TransformWithCharP의 반환값을 저장
	pid_t pid;
	int status;
	FILE * fp;
	
	input_value = (char*)malloc(sizeof(char) * MAX_CHAR_NUM);	
	if ((fp = fopen(argv[1], "r")) != NULL) {   //batch mode로 실행
		while (fgets(input_value, MAX_CHAR_NUM, fp) !=NULL) {
			
			//quit만나면종료
            if (strcmp(input_value, "quit\n") == 0) {
			printf("you get quit\n");
			exit(0);
			}

			printf("%s", input_value);
			str_len = strlen(input_value);
			input_value[str_len-1] = '\0';

			//";"기준으로 input_value를 파싱한다.
			parsing_ptr = strtok(input_value,";");
			while(parsing_ptr != NULL){
				parsing_arr[arr_cnt++] = parsing_ptr;
				parsing_ptr = strtok(NULL, ";");	
			}

			//명령어의 수만큼 (";"로 파싱된 수만큼) 반복한다.
			for(i= 0 ; i<arr_cnt; i++) {
				if (parsing_arr[i] == NULL) {
					break;
				}

				//execvp()에 넣기 편한 형태로 변환
				cmd = TransformWithCharP(parsing_arr[i]);	
				//프로세스를 생성해서 자식 프로세스가 execvp를, 부모프로세스는 계속 돌게끔
				pid = fork();		
				if(pid == -1) {
					printf("fork() error!");
					exit(0);
				}
		
				if(pid == 0) {
					execvp(cmd[0],cmd);
					printf("execvp() error!\n");
					exit(0); //execvp실패시 자식 프로세스가 종료하게하기 위해		
				} else {
					pid = waitpid(pid, &status, 0);
				}	
			}
			arr_cnt =0 ; //한 줄이 끝났기 때문에 다시 0으로 초기화해준다.
		}
		free(cmd);
	} else {		//interative mode로 실행
		while (1) { 
				printf("prompt> ");
				if (fgets(input_value,MAX_CHAR_NUM, stdin) == NULL) { 
					printf("fgets is NULL\n");
					exit(0);
				}
		
				//quit만나면종료
				if (strcmp(input_value, "quit\n") == 0) {
               		printf("you get quit\n");
					exit(0);
				}
	
				str_len = strlen(input_value);
				input_value[str_len-1] = '\0';

				//";"로 파싱.
				parsing_ptr = strtok(input_value,";");
				while (parsing_ptr != NULL) {
					parsing_arr[arr_cnt++] = parsing_ptr;
					parsing_ptr = strtok(NULL, ";");	
				}
				//명령어의 수만큼 반복해줌.
				for (i= 0 ; i<arr_cnt; i++) {
					if (parsing_arr[i] == NULL)
						break;
					cmd = TransformWithCharP(parsing_arr[i]);
	
					pid = fork();		
					if (pid == -1) {
						printf("fork() error!");
						exit(0);
					}
		
					if (pid == 0) {
						execvp(cmd[0],cmd);
						printf("execvp() error!\n");
						exit(0);	
					} else {
						pid = waitpid(pid, &status, 0);	
					}				
				}
				arr_cnt =0;
				free(cmd); 
		}
	}
	free(input_value);
	return 0;
}

/*
이 함수는 execvp()의 인자에 넣기 편한 형태로 명령어를 char**형에 저장하는 함수입니다.
@param[in]
	origin : 명령어를 저장하고 있는 변수

@return
	명령어를 공백으로 잘라서 char**형으로 반환
*/
char** TransformWithCharP(char *origin) {
	int i;
	int idx = 0;
	char *parsing_ptr;
	char **trans_value;

	trans_value = (char**)malloc(sizeof(char*) * MAX_CMD_NUM);
	for (i = 0; i < MAX_CMD_NUM; i++) {
		trans_value[i] = NULL;
	}
	parsing_ptr = strtok(origin, " ");

	while (parsing_ptr != NULL) {
		trans_value[idx++] = parsing_ptr;
		parsing_ptr = strtok(NULL, " ");
	}
	return trans_value;
}

