/*
�� ������ ���� ������ �����Դϴ�.
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
	char *input_value;			//�Է��� ���� ����
	char *parsing_ptr;			//";"�������� strtok�� ptr�� ����
	char *parsing_arr[MAX_CMD_NUM] ={NULL, };   //";"�� �������� �����͸� ������ ����
	int arr_cnt = 0;			//";"�������γ������� ��������.
	char **cmd;				//function TransformWithCharP�� ��ȯ���� ����
	pid_t pid;
	int status;
	FILE * fp;
	
	input_value = (char*)malloc(sizeof(char) * MAX_CHAR_NUM);	
	if ((fp = fopen(argv[1], "r")) != NULL) {   //batch mode�� ����
		while (fgets(input_value, MAX_CHAR_NUM, fp) !=NULL) {
			
			//quit����������
            if (strcmp(input_value, "quit\n") == 0) {
			printf("you get quit\n");
			exit(0);
			}

			printf("%s", input_value);
			str_len = strlen(input_value);
			input_value[str_len-1] = '\0';

			//";"�������� input_value�� �Ľ��Ѵ�.
			parsing_ptr = strtok(input_value,";");
			while(parsing_ptr != NULL){
				parsing_arr[arr_cnt++] = parsing_ptr;
				parsing_ptr = strtok(NULL, ";");	
			}

			//��ɾ��� ����ŭ (";"�� �Ľ̵� ����ŭ) �ݺ��Ѵ�.
			for(i= 0 ; i<arr_cnt; i++) {
				if (parsing_arr[i] == NULL) {
					break;
				}

				//execvp()�� �ֱ� ���� ���·� ��ȯ
				cmd = TransformWithCharP(parsing_arr[i]);	
				//���μ����� �����ؼ� �ڽ� ���μ����� execvp��, �θ����μ����� ��� ���Բ�
				pid = fork();		
				if(pid == -1) {
					printf("fork() error!");
					exit(0);
				}
		
				if(pid == 0) {
					execvp(cmd[0],cmd);
					printf("execvp() error!\n");
					exit(0); //execvp���н� �ڽ� ���μ����� �����ϰ��ϱ� ����		
				} else {
					pid = waitpid(pid, &status, 0);
				}	
			}
			arr_cnt =0 ; //�� ���� ������ ������ �ٽ� 0���� �ʱ�ȭ���ش�.
		}
		free(cmd);
	} else {		//interative mode�� ����
		while (1) { 
				printf("prompt> ");
				if (fgets(input_value,MAX_CHAR_NUM, stdin) == NULL) { 
					printf("fgets is NULL\n");
					exit(0);
				}
		
				//quit����������
				if (strcmp(input_value, "quit\n") == 0) {
               		printf("you get quit\n");
					exit(0);
				}
	
				str_len = strlen(input_value);
				input_value[str_len-1] = '\0';

				//";"�� �Ľ�.
				parsing_ptr = strtok(input_value,";");
				while (parsing_ptr != NULL) {
					parsing_arr[arr_cnt++] = parsing_ptr;
					parsing_ptr = strtok(NULL, ";");	
				}
				//��ɾ��� ����ŭ �ݺ�����.
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
�� �Լ��� execvp()�� ���ڿ� �ֱ� ���� ���·� ��ɾ char**���� �����ϴ� �Լ��Դϴ�.
@param[in]
	origin : ��ɾ �����ϰ� �ִ� ����

@return
	��ɾ �������� �߶� char**������ ��ȯ
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

