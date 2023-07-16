#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <map>
#include <iostream>
#define MAX_LEN 4096
#define NUM_COLORS 6

using namespace std;

struct Clients
{
	string name;
	int socket;
	thread th;
	string name_channel;
	string ip;
	bool muted = false;
	bool adm = false;
};

struct Channel{
	list <Clients*> cl;
	int socket_adm = -1;
};



list<Clients> clients;
string def_col="\033[0m";
string colors[]={"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m","\033[36m"};
mutex cout_mtx,clients_mtx;


map<string, Channel> channel;


void printMessage(string message);
void printMessage(string message);
void printNumber(int n);
void closeClientConnection(int client_socket);
void setClientName(int client_socket, char name[]);
void send_message_client(int client_socket, string message);
void broadcast_Message(int client_socket, string message, bool fromServer);
//Retorna o cliente por numero de socket
Clients *return_client(int client_socket);
//Retorna o cliente dado determinado nome
Clients *return_client(string name);
void command_decide(int client_socket, char message[MAX_LEN]);
void handle_client(int client_socket);
void input_server(thread t);



int server_socket;
int main()
{
	if((server_socket=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		perror("socket: ");
		exit(-1);
	}

	struct sockaddr_in server;
	server.sin_family=AF_INET;
	server.sin_port=htons(10000);
	server.sin_addr.s_addr=INADDR_ANY;
	bzero(&server.sin_zero,0);

	int opt = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if((bind(server_socket,(struct sockaddr *)&server,sizeof(struct sockaddr_in)))==-1)
	{
		perror("bind error: ");
		exit(-1);
	}

	if((listen(server_socket,8))==-1)
	{
		perror("listen error: ");
		exit(-1);
	}

	struct sockaddr_in client;
	int client_socket;
	unsigned int len=sizeof(sockaddr_in);


	printMessage("\t||Server Started||\n\n");
	//Criação da thread de leitura de input do server 
	thread t_server(input_server, (move(t_server)));
	while(1)
	{
		if((client_socket=accept(server_socket,(struct sockaddr *)&client,&len))==-1)
		{
			perror("accept error: ");
			exit(-1);
		}
	
		thread t(handle_client,client_socket);

		//Armazena o endereço ip de cada cliente
		char ip[INET_ADDRSTRLEN];
		if (getpeername(client_socket, (struct sockaddr*)&client, &len) == 0) {
			inet_ntop(AF_INET, &(client.sin_addr), ip, INET_ADDRSTRLEN);
		} else {
			perror("Erro ao obter o endereço IP remoto");
		}

		lock_guard<mutex> guard(clients_mtx);
		clients.push_back({string("Anonymous"),client_socket,(move(t)), string(""), string(ip)});
	}
	


	//Espera que suas threads sejam finalizadas
	for (auto itr = clients.begin(); itr != clients.end(); itr++) {
		if(itr->th.joinable())
			itr->th.join();
    }
	if(t_server.joinable())
		t_server.join();

	close(server_socket);
	return 0;
}


void end_connection()
{
	//Envia uma mensagem que o cliente foi desconectado e realiza a desconexao dele do servidor e dos canais
	for (auto it = clients.begin(); it != clients.end(); it++) {
		string msg = "Você foi desconectado do servidor\n";
		send_message_client(it->socket, msg);
		closeClientConnection(it->socket);
    }
	close(server_socket);	
}


void input_server(thread t){
	char command[30];
	while(1){
		cin.getline(command, sizeof(command));
		if(strcmp(command, "/close") == 0){
			end_connection();
			//t.detach();
		}
	}
}


void printMessage(string message){
	lock_guard<mutex> guard(cout_mtx);
	cout << message;
	cout.flush();
}


void printNumber(int n){
	lock_guard<mutex> guard(cout_mtx);
	cout << n;
	cout.flush();
}


void closeClientConnection(int client_socket){
	//Retorna o cliente a ser removido
	Clients *cl = return_client(client_socket);
	if(cl == NULL) return;

	string msg = cl->name + " saiu do servidor\n";
	broadcast_Message(client_socket, msg, true);
	printMessage(msg);

	string name_channel = cl->name_channel;
	//lock_guard<mutex> guard(clients_mtx);
	clients_mtx.lock();
	if(name_channel != ""){
		//Remoção do cliente retornado do canal, caso ele esteja em algum canal
		channel[name_channel].cl.remove(cl);
		if(channel[name_channel].cl.size() == 0){
			channel[name_channel].socket_adm = -1;
		} else{
			//Se o usuario a ser removido é o adm, promovemos outro adm
			if(channel[name_channel].socket_adm == cl->socket){
				//Adiciona o socket do segundo usuario como adm
				channel[name_channel].socket_adm = channel[name_channel].cl.front()->socket;
				channel[name_channel].cl.front()->adm = true;
			}
		}
	}
		
	//Fecha conexao do socket do cliente
	close(cl->socket);
	//Fecha thread do cliente
	cl->th.detach();

	//Remoção do cliente da lista geral de clientes
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if(it->socket == client_socket){
			clients.erase(it);
			break;
		}
    }
	clients_mtx.unlock();
}


void setClientName(int client_socket, char name[]){
	lock_guard<mutex> guard(clients_mtx);
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if(it->socket == client_socket){
			it->name = string(name);
			string msg = it->name + " entrou no servidor\n";
			printMessage(msg);
			return;
		}
    }
}

Clients *return_client(int client_socket){
	lock_guard<mutex> guard(clients_mtx);
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if(it->socket == client_socket){
			return &(*it);
		}
    }
	return nullptr;
}

void send_message_client(int client_socket, string message){
	char *msg = (char *) malloc(message.length()+1);
	strcpy(msg, message.c_str());

	int qtd_messages = ceil((strlen(msg)/4096.0));
    for(int x = 0; x < qtd_messages; x++){
        
		int bytes_sent = 0;
		//Envia a mensagem 5 vezes ou envia ate q o cliente a receba
		for(int y = 0; y < 5 && bytes_sent <= 0; y++){	
			bytes_sent = send(client_socket,msg+4096*x,4096,0);
		}	
		if(bytes_sent <= 0){
			closeClientConnection(client_socket);
			char msg[25] = "Cliente Desconectado\0";
			printMessage(msg);
		}
    } 
}


void broadcast_Message(int client_socket, string message, bool fromServer){
	Clients *actual_client;
	actual_client = return_client(client_socket);
	
	lock_guard<mutex> guard(clients_mtx);
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if(it->name_channel == actual_client->name_channel && it->name != actual_client->name && actual_client->muted == false){
			string msg;
			if(fromServer == false){
				msg += actual_client->name;
				if(actual_client->adm == true)
					msg += "(admin): ";
				else
					msg += "(user): ";
			} else{
				msg += "Server: ";
			}
			msg += message + '\n';
			send_message_client(it->socket, msg);
		}
    }
}

Clients *return_client(string name){
	lock_guard<mutex> guard(clients_mtx);
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if(it->name == name){
			return &(*it);
		}
    }
	return nullptr;
}


void command_decide(int client_socket, char message[MAX_LEN]){

	//Verificar se é um comando, verificando o caractere inicial ou verificando que a string é muito grande pra ser um comando
	if(message[0] != '/' || strlen(message) > 30){
		Clients *cl = return_client(client_socket);
		if(cl == NULL || cl->name_channel == "") return;
		broadcast_Message(client_socket, string(message), false);
		return;
	}

	char *pch; 
    pch = strtok(message, " ");
	char firstWord[20];
	strcpy(firstWord, pch);

	if(strcmp(firstWord, "/quit") == 0){
		closeClientConnection(client_socket);
		return;
	}
	if(strcmp(firstWord, "/ping") == 0){
		string msg = "Server: pong\n";
		send_message_client(client_socket, msg);
		Clients *cl = return_client(client_socket);
		msg = cl->name + ": " + "/ping\n";
		printMessage(msg);
		return;
	}

	//Pega a segunda palavra da linha de comando
	pch = strtok(NULL, " ");
	//Caso o usuario envie /join, sem o nome do canal
	if(pch == NULL)
		return;
	char secondWord[20];
	strcpy(secondWord, pch);
	
	
	if(strcmp(firstWord, "/join") == 0){
		//Variavel que recebe os dados do cliente armazenado na lista geral
		Clients *new_Client;
		new_Client = return_client(client_socket);

		//Verificação usuario ja se encontra conectado a algum canal
		if(new_Client->name_channel != ""){
			string msg = "Server: Você já está conectado ao canal " + new_Client->name_channel + "\n";
			send_message_client(client_socket, msg);
			return;
		}

		map<string, Channel>::iterator it;
		it = channel.find(string(secondWord));

		//Opção: encontrou o canal, apenas deve entrar
		if (it != channel.end()){
			
			//Adiciona o nome do canal ao cliente
			new_Client->name_channel = string(secondWord);

			//Adiciona como adm o usuario caso a sala esteja vazia
			if(channel[string(secondWord)].socket_adm == -1){
				channel[string(secondWord)].socket_adm	= client_socket;	
				new_Client->adm = true;
			}

			//Adiciona o cliente ao canal
			channel[string(secondWord)].cl.push_back(new_Client);
		//Nao ha canal existente, entao cria um novo canal
		} else{

			//Cria um novo canal
			Channel new_Channel;

			//Adiciona o nome do canal ao cliente
			new_Client->name_channel = string(secondWord);
			//Adiciona o cliente como admin
			new_Client->adm = true;

			//Adiciona o cliente ao canal
			new_Channel.cl.push_back(new_Client);
			new_Channel.socket_adm = client_socket;
			channel.insert(pair<string, Channel>(string(secondWord), new_Channel));
			
			string msg = "Canal " + string(secondWord) + " criado com sucesso\n";
			printMessage(msg);
		}
		string msg = new_Client->name + " entrou no canal " + string(secondWord) + "\n"; 
		printMessage(msg);

		broadcast_Message(client_socket, msg, true);

		string welcome = "\tBem-vindo ao canal: " + new_Client->name_channel + "\n";
		send_message_client(client_socket, welcome);

		return;
	}
	//Seleciona a referencia ao usuario da conexao atual
	Clients *actual_Client;
	actual_Client = return_client(client_socket);
	if(strcmp(firstWord, "/nickname") == 0){
		string old_nickname = actual_Client->name;
		string new_nickname = string(secondWord);
		
		//Modifica o apelido do usuario que fez o pedido
		actual_Client->name = new_nickname;

		string msg = "Server: \tApelido alterado com sucesso\n";
		msg += "Server: Apelido antigo: " + old_nickname + "\n" + "Server: Novo apelido: " + new_nickname + "\n";
		send_message_client(client_socket, msg);
		return;
	}
	if(strcmp(firstWord, "/kick") == 0){
		Clients *target_Client = return_client(string(secondWord));
		if(target_Client == NULL){
			string msg = "Usuario " + string(secondWord) + " nao encontrado\n";
			send_message_client(client_socket, msg);
			return;
		}
		//Fecha a conexao do cliente
		if(actual_Client->name_channel == target_Client->name_channel && actual_Client->adm == true){
			closeClientConnection(target_Client->socket);
		}
		return;
	}
	if(strcmp(firstWord, "/mute") == 0){
		if(actual_Client->adm == true){
			Clients *target_Client = return_client(string(secondWord));
			target_Client->muted = true;
		}
		return;
	}
	if(strcmp(firstWord, "/unmute") == 0){
		if(actual_Client->adm == true){
			Clients *target_Client = return_client(string(secondWord));
			target_Client->muted = false;
		}
		return;
	}
	if(strcmp(firstWord, "/whois") == 0){
		if(actual_Client->adm == true){
			Clients *target_Client = return_client(string(secondWord));
			if(target_Client == NULL){
				string msg = "Usuario " + string(secondWord) + " nao encontrado\n";
				send_message_client(client_socket, msg);
				return;
			}
			string ip = "Server: Endereco IP de " + string(secondWord) + ": " + target_Client->ip+ "\n";
			send_message_client(client_socket, ip);
		}
		return;
	}
	
}


void handle_client(int client_socket)
{	
	//Recebe o nome apos o usuario se conectar ao servidor
	char name[MAX_LEN];
	int bytes_received = recv(client_socket,name,sizeof(name),0);
	if(bytes_received <= 0)
		return;
	
	//Define o nome do usuario
	setClientName(client_socket, name);

	//Mensagem de resposta do servidor 
	// string welcome = "\tBem-vindo ao servidor!!\n";
	// send_message_client(client_socket, welcome);
	
	while(1){
		char message[MAX_LEN];
		int bytes_received = recv(client_socket,message,sizeof(message),0);
		if(bytes_received <= 0)
			return;
		command_decide(client_socket, message);
	}
}