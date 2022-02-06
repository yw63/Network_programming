#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <string>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <map>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

const char *welcome = "****************************************\n"
                      "** Welcome to the information server. **\n"
                      "****************************************\n";

struct userinfo{
	int id;
	string name;
	string ip;
	string env_path;
};
map<int, userinfo> users; //<fd, userinfo>

struct project1{
	int id;

	vector <int*> numberpipes;
	vector <int> counter;
	vector <int> target_index;
};
map<int, struct project1> userspace;

int isuserpipe[31][31] = {0};
int userpipe[31][31][2] = {-1};

void childhandler(int signo){
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0){

    }
}

void broadcast(string msg){
	for(auto it = users.begin(); it != users.end(); it++){
		write(it->first, msg.c_str(), msg.length());
	}
}

void remove_user(int fd){
	string logout = "*** User '" + users[fd].name + "' left. ***\n";
	broadcast(logout);
	
	for(int i = 0; i <= 31; i++){
		if(isuserpipe[users[fd].id][i] == 1){
			isuserpipe[users[fd].id][i] = 0;
			close(userpipe[users[fd].id][i][0]);
			userpipe[users[fd].id][i][0] = -1;
			userpipe[users[fd].id][i][1] = -1;
		}
		if(isuserpipe[i][users[fd].id] == 1){
			isuserpipe[i][users[fd].id] = 0;
			close(userpipe[i][users[fd].id][0]);
			userpipe[i][users[fd].id][0] = -1;
			userpipe[i][users[fd].id][1] = -1;
		}
	}
	
	users.erase(fd);
	userspace.erase(fd);
}

void add_user(int fd, struct sockaddr_in clientInfo){
	struct userinfo temp;

	temp = {};
	temp.name = "(no name)";
	temp.ip = string(inet_ntoa(clientInfo.sin_addr)) + ":" + to_string(ntohs(clientInfo.sin_port));
	temp.env_path = "bin:.";

	write(fd, welcome, strlen(welcome));
	for(int i = 1; i <= 30; i++){
		map<int, userinfo>::iterator it;
		for(it = users.begin(); it != users.end(); it++){
			if(it->second.id == i)
				break;
		}
		if(it == users.end()){
			temp.id = i;
			users[fd] = temp;
			break;
		}
	}

	struct project1 infos = {};
	infos.id = temp.id;
	userspace[fd] = infos;

	string login = "*** User '(no name)' entered from " + temp.ip + ". ***\n";
    broadcast(login);
    write(fd, "% ", 2);

}

bool isbuiltin(string input, int fd){
	size_t found;

	if(input.substr(0,3) == "who"){
		string message;
		message = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
        write(fd, message.c_str(), message.length());
		for(int i = 1; i <= 30; i++){
			for(auto it = users.begin(); it != users.end(); it++){
				if(it->second.id == i){
					if(it->first == fd){
    					message = to_string(it->second.id) + "\t" + it->second.name + "\t" + it->second.ip + "\t" + "<-me\n";
    					write(fd, message.c_str(), message.length());
					}
    				else{
    					message = to_string(it->second.id) + "\t" + it->second.name + "\t" + it->second.ip + "\n";
    					write(fd, message.c_str(), message.length());
    				}
    				break;
				}
    		}
		}
		return 1;
	}
	else if(input.substr(0,4) == "name"){
		string message;
		string newname = input.substr(5, input.size()-5);
		map<int, userinfo>::iterator it;
		for(it = users.begin(); it != users.end(); it++){
			if(newname == it->second.name){
				message = "*** User '" + string(newname) + "' already exists. ***\n";
				write(fd, message.c_str(), message.length());
				break;
			}
		}
		if(it == users.end()){
			users[fd].name = newname;
			string message = "*** User from " + users[fd].ip + " is named '" +  users[fd].name + "'. ***\n";
			broadcast(message);
		}
		return 1;
	}
	else if(input.substr(0,4) == "yell"){
		string origin_message = input.substr(5, input.size()-5);
		string message = "*** " + users[fd].name + " yelled ***: " + origin_message + "\n";
		broadcast(message);

		return 1;
	}
	else if(input.substr(0,4) == "tell"){
		string temp;
		temp = input.substr(5, input.size()-5); //temp = input - "tell"

		size_t space_index;
		space_index = temp.find(" ");
		string s_targetuserid = temp.substr(0, space_index); 
		string origin_message = temp.substr(space_index+1, temp.size()-space_index-1);

		int i_targetuserid = stoi(s_targetuserid);
		string message;

		map<int, userinfo>::iterator it;
		for(it = users.begin(); it != users.end(); it++){
			if(it->second.id == i_targetuserid){
				message = "*** " + users[fd].name + " told you ***: " + origin_message + "\n";
				write(it->first, message.c_str(), message.length());
				break;
			}
		}
		if(it == users.end()){
			message = "*** Error: user #" + s_targetuserid + " does not exist yet. ***\n";
			write(fd, message.c_str(), message.length());
		}

		return 1;
	}

	return 0;
}

int func1(int fd){

	setenv("PATH", users[fd].env_path.c_str(), 1);

	int pipe_count = 0;//check current line's pipe count
	int redirect = 0;//check if there is file redirection
	int isnumberpipe = 0;
	int isnumberpipe_stderr = 0;
	int current_counter = 0;//current line's numberpipe counter
	int isuserin = 0;
	int isuserout = 0;
	int userin_id;
	int userout_id;

	vector<vector<string>> cmds;
	vector<pid_t> pidlist;//list of child's pid for current line
	string redirect_file;//filename of outputfile

	string origin_input = "";
	string input = "";
	getline(cin, input);
	if(input == "" || input == "\n" || input == "\r")
		return 0;
	if(input[input.length()-1] == '\n' || input[input.length()-1] == '\r'){
		input = input.substr(0, input.length()-1);
	}
	origin_input = input;
	dprintf(1022, "%s\n", input.c_str());
	fflush(stdout);

	bool flag = 0;
	if((flag = isbuiltin(input, fd)) == 1)
		return 0;

	for(int i = 0; i < userspace[fd].counter.size(); i++){
		userspace[fd].counter[i]--;
	}
	for(int i = 0; i < input.length(); i++){
		if(input[i] == '|')
			pipe_count++;
	}
		//parse with "|" first
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

	//check if numberpipe
	if(pipe_count != 0 && parameters[pipe_count][0] != ' '){
		string temp;
		temp = parameters[pipe_count];
		current_counter = stoi(temp);
		isnumberpipe = 1;
		pipe_count--;
		parameters.pop_back();
	}

	//check if numerpipe_stderr
	for(int i = 0; i < parameters[pipe_count].size(); i++){
		if(parameters[pipe_count][i] == '!'){
			string temp = parameters[pipe_count].substr(i+1, parameters[pipe_count].size());
			current_counter = stoi(temp);
			string temp2 = parameters[pipe_count].substr(0, i);
			parameters[pipe_count] = temp2;
			isnumberpipe_stderr = 1;
		}
	}

	//for each parameters cut by "|", parse again by space
	for(int i = 0; i < parameters.size(); i++){
		istringstream iss(parameters[i]);
		string temp1;
		vector<string> temp2;

		while(iss >> temp1)
			temp2.push_back(temp1);

		cmds.push_back(temp2);
	}

	//deal with printenv, setenv and exit
	if(cmds[0][0] == "printenv"){
		string str;
		if(cmds[0].size() > 1){
			if(getenv(cmds[0][1].c_str()) != NULL){
				str = getenv(cmds[0][1].c_str());
				cout << str << endl;
			}
		}
		return 0;
	}
	else if(cmds[0][0] == "setenv"){
		if(cmds[0].size() >= 2){
			setenv(cmds[0][1].c_str(), cmds[0][2].c_str(), 1);
			if(cmds[0][1] == "PATH")
				users[fd].env_path = cmds[0][2];
		}
		return 0;
	}
	else if(cmds[0][0] == "exit"){
		remove_user(fd);
		return -1;
	}

	//set redirection to file if find ">", and get output file name
	for(int i = 0; i < cmds[cmds.size()-1].size(); i++){
		if(cmds[cmds.size()-1][i] == ">"){
			//write(fd, "redirect\n", 9);
			redirect = 1;
			redirect_file = cmds[cmds.size()-1][i + 1];
			break;
		}
	}

	//check if userpipe input
	for(int i = 0; i < cmds[0].size(); i++){
		string message;
		if(cmds[0][i][0] == '<'){
			userin_id = stoi(cmds[0][i].substr(1, cmds[0][i].length()-1));
			if(isuserpipe[userin_id][users[fd].id] == 1)
				isuserin = 1;
			else{
				map<int, userinfo>::iterator it;
				for(it = users.begin(); it != users.end(); it++){
					if(userin_id == it->second.id){ //id exist but no userpipe
						message = "*** Error: the pipe #" + to_string(userin_id) + "->#" + to_string(users[fd].id) + " does not exist yet. ***\n";
						write(fd, message.c_str(), message.length());
						return 0;
					}
				}
				if(it == users.end()){ //id doesn't exist
					message = "*** Error: user #" + to_string(userin_id) + " does not exist yet. ***\n";
					write(fd, message.c_str(), message.length());
					return 0;
				}
			}
		}
	}

	//check if userpipe output
	for(int i = 0; i < cmds[cmds.size()-1].size(); i++){
		string message;
		if(cmds[cmds.size()-1][i][0] == '>' && !redirect){
			userout_id = stoi(cmds[cmds.size()-1][i].substr(1, cmds[cmds.size()-1][i].length()-1));
			map<int, userinfo>::iterator it;
			for(it = users.begin(); it != users.end(); it++){
				if(it->second.id == userout_id){
					if(isuserpipe[users[fd].id][userout_id] == 1){
						message = "*** Error: the pipe #" + to_string(users[fd].id) + "->#" + to_string(userout_id) + " already exists. ***\n";
            			write(fd, message.c_str(), message.length());
            			return 0;
					}
					else{
						isuserout = 1;
						break;
					}
				}
			}
			if(it == users.end()){
				message = "*** Error: user #" + to_string(userout_id) + " does not exist yet. ***\n";
            	write(fd, message.c_str(), message.length());
            	return 0;
			}
			break;
		}
	}

	
	if(pipe_count == 0){//cmds with no pipe
		int *pipes = new int(2);
		if(isnumberpipe == 1 || isnumberpipe_stderr == 1){
			int create_new_pipe = 1;
			for(int i = 0; i < userspace[fd].counter.size(); i++){
				if(current_counter == userspace[fd].counter[i]){
					create_new_pipe = 0;
					userspace[fd].target_index.push_back(userspace[fd].target_index[i]);
					userspace[fd].counter.push_back(current_counter);
					break;
				}
			}
			if(create_new_pipe == 1){
				pipe(pipes);
				userspace[fd].numberpipes.push_back(pipes);
				userspace[fd].target_index.push_back(userspace[fd].numberpipes.size() - 1);
				userspace[fd].counter.push_back(current_counter);
			}
		}
		else if(isuserout == 1){
			if(pipe(pipes) < 0)
				perror("userpipe fail");
			userpipe[users[fd].id][userout_id][0] = pipes[0];
			userpipe[users[fd].id][userout_id][1] = pipes[1];
			isuserpipe[users[fd].id][userout_id] = 1;
		}
		
		int pid;
		if((pid = fork()) == -1){
			perror("can't fork");
			exit(1);
		}

		if(pid == 0){//child
			char *cmd;
			cmd = &cmds[0][0][0];
			int size = 0;
			if(redirect == 1 || (isuserin == 1 && isuserout == 1))
				size = cmds[0].size() - 2;
			else if((isuserin == 1 || isuserout == 1) && !(isuserin == 1 && isuserout == 1))//isuserin xor isuserout
				size = cmds[0].size() - 1;
			else
				size = cmds[0].size();

			char **argv = (char**)calloc(size + 1, sizeof(char*));
				
			for(int i = 0; i < size; i++){
				argv[i] = &cmds[0][i][0];
			}
				
			argv[size] = NULL;

			if(redirect == 1){
				int filefd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	            dup2(filefd, STDOUT_FILENO);
			}
			if(isnumberpipe == 1){
				dup2(userspace[fd].numberpipes[userspace[fd].target_index.back()][1], STDOUT_FILENO);
				close(userspace[fd].numberpipes[userspace[fd].target_index.back()][0]);
			}
			if(isnumberpipe_stderr == 1){
				dup2(userspace[fd].numberpipes[userspace[fd].target_index.back()][1], STDOUT_FILENO);
				dup2(userspace[fd].numberpipes[userspace[fd].target_index.back()][1], STDERR_FILENO);
				close(userspace[fd].numberpipes[userspace[fd].target_index.back()][0]);
			}

			for(int i = 0; i < userspace[fd].counter.size(); i++){
				if(userspace[fd].counter[i] == 0){
					dup2(userspace[fd].numberpipes[userspace[fd].target_index[i]][0], STDIN_FILENO);
					close(userspace[fd].numberpipes[userspace[fd].target_index[i]][1]);
					break;
				}
			}

			if(isuserin == 1){
				dup2(userpipe[userin_id][users[fd].id][0], STDIN_FILENO);
				close(userpipe[userin_id][users[fd].id][1]);
			}

			if(isuserout == 1){
				dup2(userpipe[users[fd].id][userout_id][1], STDOUT_FILENO);
				close(userpipe[users[fd].id][userout_id][0]);
			}

			execvp(cmd, argv);
			cerr << "Unknown command: [" << cmd << "]." << endl;
			exit(0);
		}
		else{//parent
			for(int i = 0; i < userspace[fd].counter.size(); i++){
				if(userspace[fd].counter[i] == 0){
					close(userspace[fd].numberpipes[userspace[fd].target_index[i]][0]);
					close(userspace[fd].numberpipes[userspace[fd].target_index[i]][1]);
				}
			}
			if(isuserin == 1){
				isuserpipe[userin_id][users[fd].id] = 0;
				close(userpipe[userin_id][users[fd].id][0]);
				userpipe[userin_id][users[fd].id][0] = -1;

				string message;
				string sender_name;
				for(auto it = users.begin(); it != users.end(); it++){
					if(it->second.id == userin_id){
						sender_name = it->second.name;
						break;
					}
				}
				message = "*** " + users[fd].name + " (#" + to_string(users[fd].id) + ") just received from " + sender_name + " (#" + to_string(userin_id) + ") by '" + origin_input + "' ***\n";
				broadcast(message);
			}
			if(isuserout == 1){
				close(userpipe[users[fd].id][userout_id][1]);
				userpipe[users[fd].id][userout_id][1] = -1;

				string message;
				string receiver_name;
				for(auto it = users.begin(); it != users.end(); it++){
					if(it->second.id == userout_id){
						receiver_name = it->second.name;
						break;
					}
				}
				message = "*** " + users[fd].name + " (#" + to_string(users[fd].id) + ") just piped '" + origin_input + "' to " + receiver_name + " (#" + to_string(userout_id) + ") ***\n";
				broadcast(message);
			}

			if(!isnumberpipe && !isnumberpipe_stderr && !isuserout)
				waitpid(pid, NULL, 0);
		}
	}
	else{//cmds with pipe
		/*
		vector<int*> pipes;
		for(int i = 0; i < pipe_count; i++){
			int *temp = new int [2];

			if(pipe(temp) < 0)
				perror("can't create pipe");

			pipes.push_back(temp);
		}
		*/
		int *last_pipe = new int(2);
		if(isnumberpipe == 1 || isnumberpipe_stderr == 1){
			int create_new_pipe = 1;
			for(int i = 0; i < userspace[fd].counter.size(); i++){
				if(current_counter == userspace[fd].counter[i]){
					create_new_pipe = 0;
					userspace[fd].target_index.push_back(userspace[fd].target_index[i]);
					userspace[fd].counter.push_back(current_counter);
					break;
				}
			}
			if(create_new_pipe == 1){
				pipe(last_pipe);
				userspace[fd].numberpipes.push_back(last_pipe);
				userspace[fd].target_index.push_back(userspace[fd].numberpipes.size() - 1);
				userspace[fd].counter.push_back(current_counter);
			}
		}
		else if(isuserout == 1){
			if(pipe(last_pipe) < 0)
				perror("userpipe fail");
			userpipe[users[fd].id][userout_id][0] = last_pipe[0];
			userpipe[users[fd].id][userout_id][1] = last_pipe[1];
			isuserpipe[users[fd].id][userout_id] = 1;
		}

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
    		if (childpid == 0){//child

    			if(i == 0){ //first child process
    				char *cmd;
					cmd = &cmds[0][0][0];
					int size = 0;
					if(isuserin == 1)
						size = cmds[0].size() - 1;
					else
						size = cmds[0].size();
					char **argv = (char**)calloc(size + 1, sizeof(char*));
						
					for(int j = 0; j < size; j++){
						argv[j] = &cmds[0][j][0];
					}
						
					argv[size] = NULL;

    				dup2(pipes[0][1], STDOUT_FILENO);
    				close(pipes[0][0]);

    				for(int i = 0; i < userspace[fd].counter.size(); i++){
						if(userspace[fd].counter[i] == 0){
							dup2(userspace[fd].numberpipes[userspace[fd].target_index[i]][0], STDIN_FILENO);
							close(userspace[fd].numberpipes[userspace[fd].target_index[i]][1]);
							break;
						}
					}

					if(isuserin == 1){
						dup2(userpipe[userin_id][users[fd].id][0], STDIN_FILENO);
						close(userpipe[userin_id][users[fd].id][1]);
					}
    				
					execvp(cmd, argv);
					cerr << "Unknown command: [" << cmd << "]." << endl;
					exit(0);
    			}
    			else if(i == pipe_count){ //last child process

    				char *cmd;
					cmd = &cmds[i][0][0];
					int size = 0;
					if(redirect == 1)
						size = cmds[i].size() - 2;
					else if(isuserout == 1)
						size = cmds[i].size() - 1;
					else
						size = cmds[i].size();
					char **argv = (char**)calloc(size + 1, sizeof(char*));
					
					for(int j = 0; j < size; j++){
						argv[j] = &cmds[i][j][0];
					}
						
					argv[size] = NULL;

    				dup2(pipes[i - 1][0], STDIN_FILENO);
    				close(pipes[i - 1][1]);

    				if(redirect == 1){
						int filefd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		                dup2(filefd, STDOUT_FILENO);
					}
					if(isnumberpipe == 1){
						dup2(userspace[fd].numberpipes[userspace[fd].target_index.back()][1], STDOUT_FILENO);
						close(userspace[fd].numberpipes[userspace[fd].target_index.back()][0]);
					}
					if(isnumberpipe_stderr == 1){
						dup2(userspace[fd].numberpipes[userspace[fd].target_index.back()][1], STDOUT_FILENO);
						dup2(userspace[fd].numberpipes[userspace[fd].target_index.back()][1], STDERR_FILENO);
						close(userspace[fd].numberpipes[userspace[fd].target_index.back()][0]);
					}

					if(isuserout == 1){
						dup2(userpipe[users[fd].id][userout_id][1], STDOUT_FILENO);
						close(userpipe[users[fd].id][userout_id][0]);
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
    		else{//parent

    			for(int i = 0; i < userspace[fd].counter.size(); i++){
					if(userspace[fd].counter[i] == 0){
						close(userspace[fd].numberpipes[userspace[fd].target_index[i]][0]);
						close(userspace[fd].numberpipes[userspace[fd].target_index[i]][1]);
					}
				}

    			pidlist.push_back(childpid);

    			if(i == 0){ //first child process
    				close(pipes[0][1]);
    				if(isuserin == 1){
						isuserpipe[userin_id][users[fd].id] = 0;
						close(userpipe[userin_id][users[fd].id][0]);
						userpipe[userin_id][users[fd].id][0] = -1;

						string message;
						string sender_name;
						for(auto it = users.begin(); it != users.end(); it++){
							if(it->second.id == userin_id){
								sender_name = it->second.name;
								break;
							}
						}
						message = "*** " + users[fd].name + " (#" + to_string(users[fd].id) + ") just received from " + sender_name + " (#" + to_string(userin_id) + ") by '" + origin_input + "' ***\n";
						broadcast(message);
					}
    			}
    			else if(i == pipe_count){
    				close(pipes[i-1][0]);
    				if(isuserout == 1){
						close(userpipe[users[fd].id][userout_id][1]);
						userpipe[users[fd].id][userout_id][1] = -1;

						string message;
						string receiver_name;
						for(auto it = users.begin(); it != users.end(); it++){
							if(it->second.id == userout_id){
								receiver_name = it->second.name;
								break;
							}
						}
						message = "*** " + users[fd].name + " (#" + to_string(users[fd].id) + ") just piped '" + origin_input + "' to " + receiver_name + " (#" + to_string(userout_id) + ") ***\n";
						broadcast(message);
					}

	           		if(!isnumberpipe && !isnumberpipe_stderr && !isuserout){
	      				for(int j = 0; j < pidlist.size(); j++){
	  	    				waitpid(pidlist[j], 0, 0);
	  	    			}
	              	}
    			}
    			else{
    				close(pipes[i-1][0]);
    				close(pipes[i][1]);
    			}

    		}

		}

	}

	return 0;
}

int shell(int fd){
	dup2(0, 1021);
	dup2(1, 1022);
	dup2(2, 1023);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);

	int ret = func1(fd);

	write(fd, "% ", 2);
	dup2(1021, 0);
	dup2(1022, 1);
	dup2(1023, 2);

	return ret;
}

int main(int argc, char **argv){
	int msock; //master sock
    int port;

    if(argc == 2) 
        port = atoi(argv[1]);
    else{
        printf("Usage: ./np_simple [port]\n");
        exit(1);
    }

    signal(SIGCHLD, childhandler);

    //create socket
    msock = socket(AF_INET , SOCK_STREAM , 0);
    if (msock == -1){
        perror("Fail to create a socket.");
    }

    //serverinfo settings
    struct sockaddr_in serverInfo,clientInfo;
    //bzero(&serverInfo,sizeof(serverInfo));
    memset(&serverInfo, 0, sizeof(serverInfo));

    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(port);

    socklen_t infolen = sizeof(struct sockaddr_in);

    //set socket option
    int option = 1;
    if (setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    //bind
    if(bind(msock, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) < 0){
        perror("bind failed. Error");
        return 1;
    }

    //listen
    listen(msock,5);

    fd_set afds, rfds;
    int nfds;

    nfds = getdtablesize();
    FD_ZERO(&afds); //init afds
    FD_SET(msock, &afds); // add msock to afds

    while(1){
    	memcpy(&rfds, &afds, sizeof(rfds)); //initial rfds base on afds

    	if((select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0)) < 0 )
    	{
    		// perror("select error");
    		continue;
		}
    	if(FD_ISSET(msock, &rfds) && users.size() <= 30){
    		int ssock;

    		ssock = accept(msock, (struct sockaddr*)&clientInfo, &infolen);
    		if(ssock < 0)
    			perror("accept fail");
    		FD_SET(ssock, &afds);
    		add_user(ssock, clientInfo);
    	}

    	//test user login
    	/*
    	cout << "<ID>\t<nickname>\t<IP:port>\t\t<fd>\t<indicate me>" << endl;
    	for(auto it = users.begin(); it != users.end(); it++){
    		cout << it->second.id << "\t" << it->second.name << "\t" << it->second.ip << "\t" << it->first << endl;
    	}
    	*/
    	
    	for(int fd = 0; fd < nfds; ++fd){
    		if(fd != msock && FD_ISSET(fd, &rfds)){
    			if(shell(fd) == -1){
    				close(fd);
    				FD_CLR(fd, &afds);
    			}
    		}
    	}
    }

    return 0;
}