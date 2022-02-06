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
#include <stdio.h>

#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;

int pipe_count;//check current line's pipe count
int redirect;//check if there is file redirection
int isnumberpipe;
int isnumberpipe_stderr;
int current_counter;//current line's numberpipe counter
vector <int*> numberpipes;
vector <int> counter;
vector <int> target_index;

void childhandler(int signo){
    int status;
    while(waitpid(-1, &status, WNOHANG) > 0){

    }
}

int initial(){
    pipe_count = 0;
    redirect = 0;
    isnumberpipe = 0;
    isnumberpipe_stderr = 0;
    current_counter = 0;

    return(0);
}

void shell(){
    setenv("PATH", "bin:.", 1);

    while(1){
        initial();
        vector<vector<string>> cmds;
        vector<pid_t> pidlist;//list of child's pid for current line
        string redirect_file;//filename of outputfile

        cout << "% ";

        string input = "";
        getline(cin, input);
        if(input == "" || input == "\n" || input == "\r"){
            continue;
        }

        for(int i = 0; i < counter.size(); i++){
            counter[i]--;
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

        //set redirection to file if find ">", and get output file name
        for(int i = 0; i < cmds[cmds.size()-1].size(); i++){
            if(cmds[cmds.size()-1][i] == ">"){
                redirect = 1;
                redirect_file = cmds[cmds.size()-1][i + 1];
                break;
            }
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
        }
        else if(cmds[0][0] == "setenv"){
            if(cmds[0].size() >= 2)
                setenv(cmds[0][1].c_str(), cmds[0][2].c_str(), 1);
        }
        else if(cmds[0][0] == "exit")
            exit(0);
        else if(pipe_count == 0){//cmds with no pipe
            int *pipes = new int(2);
            if(isnumberpipe == 1 || isnumberpipe_stderr == 1){
                int create_new_pipe = 1;
                for(int i = 0; i < counter.size(); i++){
                    if(current_counter == counter[i]){
                        create_new_pipe = 0;
                        target_index.push_back(target_index[i]);
                        counter.push_back(current_counter);
                        break;
                    }
                }
                if(create_new_pipe == 1){
                    pipe(pipes);
                    numberpipes.push_back(pipes);
                    target_index.push_back(numberpipes.size() - 1);
                    counter.push_back(current_counter);
                }
            }
            /*
            for(int i = 0; i < counter.size(); i++){
                cout << "counter[" << i << "] = " << counter[i] << endl;
            }
            for(int i = 0; i < target_index.size(); i++){
                cout << "target_index[" << i << "] = " << target_index[i] << endl;
            }
            for(int i = 0; i < numberpipes.size(); i++){
                cout << "numberpipes[" << i << "][0] = " << numberpipes[i][0] << endl;
                cout << "numberpipes[" << i << "][1] = " << numberpipes[i][1] << endl;
            }
            */
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
                    int filefd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    dup2(filefd, STDOUT_FILENO);
                }
                if(isnumberpipe == 1){
                    dup2(numberpipes[target_index.back()][1], STDOUT_FILENO);
                    close(numberpipes[target_index.back()][0]);
                }
                if(isnumberpipe_stderr == 1){
                    dup2(numberpipes[target_index.back()][1], STDOUT_FILENO);
                    dup2(numberpipes[target_index.back()][1], STDERR_FILENO);
                    close(numberpipes[target_index.back()][0]);
                }

                for(int i = 0; i < counter.size(); i++){
                    if(counter[i] == 0){
                        dup2(numberpipes[target_index[i]][0], STDIN_FILENO);
                        close(numberpipes[target_index[i]][1]);
                        break;
                    }
                }

                execvp(cmd, argv);
                cerr << "Unknown command: [" << cmd << "]." << endl;
                exit(0);
            }
            else{
                for(int i = 0; i < counter.size(); i++){
                    if(counter[i] == 0){
                        close(numberpipes[target_index[i]][0]);
                        close(numberpipes[target_index[i]][1]);
                    }
                }
                if(!isnumberpipe && !isnumberpipe_stderr)
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
                for(int i = 0; i < counter.size(); i++){
                    if(current_counter == counter[i]){
                        create_new_pipe = 0;
                        target_index.push_back(target_index[i]);
                        counter.push_back(current_counter);
                        break;
                    }
                }
                if(create_new_pipe == 1){
                    pipe(last_pipe);
                    numberpipes.push_back(last_pipe);
                    target_index.push_back(numberpipes.size() - 1);
                    counter.push_back(current_counter);
                }
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
                        int size = cmds[0].size() + 1;
                        char **argv = (char**)calloc(size, sizeof(char*));
                        
                        for(int j = 0; j < size - 1; j++){
                            argv[j] = &cmds[0][j][0];
                        }
                        
                        argv[size - 1] = NULL;

                        dup2(pipes[0][1], STDOUT_FILENO);
                        close(pipes[0][0]);

                        for(int i = 0; i < counter.size(); i++){
                            if(counter[i] == 0){
                                dup2(numberpipes[target_index[i]][0], STDIN_FILENO);
                                close(numberpipes[target_index[i]][1]);
                                break;
                            }
                        }
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
                            int filefd = open(redirect_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                            dup2(filefd, STDOUT_FILENO);
                        }
                        if(isnumberpipe == 1){
                            dup2(numberpipes[target_index.back()][1], STDOUT_FILENO);
                            close(numberpipes[target_index.back()][0]);
                        }
                        if(isnumberpipe_stderr == 1){
                            dup2(numberpipes[target_index.back()][1], STDOUT_FILENO);
                            dup2(numberpipes[target_index.back()][1], STDERR_FILENO);
                            close(numberpipes[target_index.back()][0]);
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

                    for(int i = 0; i < counter.size(); i++){
                        if(counter[i] == 0){
                            close(numberpipes[target_index[i]][0]);
                            close(numberpipes[target_index[i]][1]);
                        }
                    }

                    pidlist.push_back(childpid);

                    if(i == 0){ //first child process
                        close(pipes[0][1]);
                    }
                    else if(i == pipe_count){
                        close(pipes[i-1][0]);

                        if(!isnumberpipe && !isnumberpipe_stderr){
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
    }

    return;
}

int main(int argc, char *argv[]){

    int sockfd = 0, forclientsockfd = 0;
    int port;

    if(argc == 2) 
        port = atoi(argv[1]);
    else{
        printf("Usage: ./np_simple [port]\n");
        exit(1);
    }

    signal(SIGCHLD, childhandler);

    //create socket
    sockfd = socket(AF_INET , SOCK_STREAM , 0);
    if (sockfd == -1){
        perror("Fail to create a socket.");
    }

    //serverinfo settings
    struct sockaddr_in serverInfo,clientInfo;
    socklen_t infolen = sizeof(struct sockaddr_in);
    //bzero(&serverInfo,sizeof(serverInfo));

    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr.s_addr = INADDR_ANY;
    serverInfo.sin_port = htons(port);

    //set socket option
    int option = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    //bind
    if(bind(sockfd, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) < 0){
        perror("bind failed. Error");
        return 1;
    }

    //listen
    listen(sockfd,5);

    while(1){
        pid_t child;

        if((forclientsockfd = accept(sockfd, (struct sockaddr*)&clientInfo, (socklen_t*)&infolen)) < 0)
            perror("accept failed");
        
        numberpipes.clear();
        counter.clear();
        target_index.clear();

        if((child = fork()) < 0)
            perror("fork failed");
        else if(child == 0){
            close(sockfd);
            dup2(forclientsockfd, STDIN_FILENO);
            dup2(forclientsockfd, STDOUT_FILENO);
            dup2(forclientsockfd, STDERR_FILENO);

            shell();
        }
        else{
            close(forclientsockfd);
            waitpid(child, NULL, 0);
        }
    }


    return 0;
}