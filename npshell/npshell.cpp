#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

using namespace std;

int pipe_count;
int redirect;

int initial(){
	pipe_count = 0;
	redirect = 0;

	return(0);
}

void childhandler(int signo){
	int status;
	while(waitpid(-1, &status, WNOHANG) > 0){

	}
}

int main(){

	signal(SIGCHLD, childhandler);
	setenv("PATH", "bin:.", 1);

	while(1){
		initial();
		vector<vector<string>> cmds;
		vector<pid_t> pidlist;
		string redirect_file;

		cout << "% ";

		string input = "";
		getline(cin, input);
		if(input == "")
			continue;

		for(int i = 0; i < input.length(); i++){
			if(input[i] == '|')
				pipe_count++;
		}

		vector<string> parameters;
		string delimiter = "|";
		size_t pos = 0;
		string token;
		while ((pos = input.find(delimiter)) != string::npos) {
		    token = input.substr(0, pos);
		    parameters.push_back(token);
		    input.erase(0, pos + delimiter.length());
		}
		parameters.push_back(input); 

		for(int i = 0; i < parameters.size(); i++){
			istringstream iss(parameters[i]);
			string temp1;
			vector<string> temp2;

			while(iss >> temp1)
				temp2.push_back(temp1);

			cmds.push_back(temp2);
		}

		for(int i = 0; i < cmds[cmds.size()-1].size(); i++){
			if(cmds[cmds.size()-1][i] == ">"){
				redirect = 1;
				redirect_file = cmds[cmds.size()-1][i + 1];
				break;
			}
		}

		if(cmds[0][0] == "printenv"){
			string str;
			if(cmds[0].size() > 1){
				if(getenv(cmds[0][1].c_str()) != NULL){
					str = getenv(cmds[0][1].c_str());
					cout << str << endl;
				}
			}
		}
		else if(cmds[0][0] == "setenv"){
			if(cmds[0].size() >= 2)
				setenv(cmds[0][1].c_str(), cmds[0][2].c_str(), 1);
		}
		else if(cmds[0][0] == "exit")
			break;
		else if(pipe_count == 0){
			int pid;
 
			if((pid = fork()) == -1){
				perror("can't fork");
				exit(1);
			}

			if(pid == 0){
				char *cmd;
				cmd = &cmds[0][0][0];
				int size = 0;
				if(redirect == 1)
					size = cmds[0].size() - 1;
				else
					size = cmds[0].size() + 1;
				char **argv = (char**)calloc(size, sizeof(char*));
				
				for(int i = 0; i < size - 1; i++){
					argv[i] = &cmds[0][i][0];
				}
				
				argv[size - 1] = NULL;

				if(redirect == 1){
					int filefd = open(redirect_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
                    dup2(filefd, STDOUT_FILENO);
				}

				execvp(cmd, argv);
				cerr << "Unknown command: [" << cmd << "]." << endl;
				exit(0);
			}
			else{
				wait(0);
			}
		}
		else{//pipe
			/*
			vector<int*> pipes;
			for(int i = 0; i < pipe_count; i++){
				int *temp = new int [2];

				if(pipe(temp) < 0)
					perror("can't create pipe");

				pipes.push_back(temp);
			}
			*/

			int **pipes = new int*[pipe_count];
			for(int i = 0; i < pipe_count; i++){
				pipes[i] = new int[2];
			}	

			pid_t childpid;

			for(int i = 0; i < pipe_count + 1; i++){
				if(i < pipe_count){
                    pipe(pipes[i]);
                }
				while ((childpid = fork()) < 0){ // handle fork error
					waitpid(-1, 0, 0);
				}
    			if (childpid == 0){

    				if(i == 0){ //first child process
    					char *cmd;
						cmd = &cmds[0][0][0];
						int size = cmds[0].size() + 1;
						char **argv = (char**)calloc(size, sizeof(char*));
						
						for(int j = 0; j < size - 1; j++){
							argv[j] = &cmds[0][j][0];
						}
						
						argv[size - 1] = NULL;

    					dup2(pipes[0][1], STDOUT_FILENO);
    					close(pipes[0][0]);

    					/*
    					for(int j = 0; j < pipe_count; j++){
    						if (j != i){
    							close(pipes[j][0]);
    							close(pipes[j][1]);
    						}
    					}
						*/
						execvp(cmd, argv);
						cerr << "Unknown command: [" << cmd << "]." << endl;
						exit(0);
    				}
    				else if(i == pipe_count){ //last child process

    					char *cmd;
						cmd = &cmds[i][0][0];
						int size = 0;
						if(redirect == 1)
							size = cmds[i].size() - 1;
						else
							size = cmds[i].size() + 1;
						char **argv = (char**)calloc(size, sizeof(char*));
						
						for(int j = 0; j < size - 1; j++){
							argv[j] = &cmds[i][j][0];
						}
						
						argv[size - 1] = NULL;

    					dup2(pipes[i - 1][0], STDIN_FILENO);
    					close(pipes[i - 1][1]);

    					if(redirect == 1){
							int filefd = open(redirect_file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		                    dup2(filefd, STDOUT_FILENO);
						}

						execvp(cmd, argv);
						cerr << "Unknown command: [" << cmd << "]." << endl;
						exit(0);
    				}
    				else{ //mid

    					char *cmd;
						cmd = &cmds[i][0][0];
						int size = cmds[i].size() + 1;
						char **argv = (char**)calloc(size, sizeof(char*));
						
						for(int j = 0; j < size - 1; j++){
							argv[j] = &cmds[i][j][0];
						}
						argv[size - 1] = NULL;

    					dup2(pipes[i-1][0], STDIN_FILENO);
    					dup2(pipes[i][1], STDOUT_FILENO);
    					close(pipes[i-1][1]);
    					close(pipes[i][0]);

    					/*
    					for(int j = 0; j < pipe_count; j++){
    						if(j == i - 1)
    							close(pipes[j][1]);
    						else if(j == i)
    							close(pipes[j][0]);
    						else{
    							close(pipes[j][0]);
    							close(pipes[j][1]);
    						}
    					}
    					*/

						execvp(cmd, argv);
						cerr << "Unknown command: [" << cmd << "]." << endl;
						exit(0);
    				}
    			}
    			else{
    				pidlist.push_back(childpid);

    				if(i == 0){ //first child process
    					close(pipes[0][1]);
    				}
    				else if(i == pipe_count){
    					close(pipes[i-1][0]);
    					for(int j = 0; j < pidlist.size(); j++){
    						waitpid(pidlist[j], 0, 0);
    					}
    				}
    				else{
    					close(pipes[i-1][0]);
    					close(pipes[i][1]);
    				}

    			}

			}

		}
    }

	return(0);
}