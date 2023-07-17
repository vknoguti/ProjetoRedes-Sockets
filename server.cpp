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
#include <signal.h>

#include <iomanip>
#define MAX_LEN 4096

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

//Lista geral de cliente
list<Clients> clients;
//Mutex de sincronização de saida e de acesso aos conteudos de usuarios e canais
mutex cout_mtx,clients_mtx;

//Cria um map que usa como chave o nome do canal, retornando o canal correspondente
map<string, Channel> channel;

void catch_ctrl_c(int signal);
void printMessage(string message);
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
string generate_nickName(string name);


int server_socket;
int main()
{
	//Cria um socket para o servidor
	if((server_socket=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		perror("socket: ");
		exit(-1);
	}

	//Dados do socket do servidor
	struct sockaddr_in server;
	server.sin_family=AF_INET;
	server.sin_port=htons(9997);
	server.sin_addr.s_addr=INADDR_ANY;
	bzero(&server.sin_zero,0);

	//Configura opções do socket, ajuda resolver problemas como “address already in use”.
	int opt = 1;
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	//Vincula o numero da porta e o endereço ao socket
	if((bind(server_socket,(struct sockaddr *)&server,sizeof(struct sockaddr_in)))==-1)
	{
		perror("bind error: ");
		exit(-1);
	}

	//Coloca o socket em um estado passivo de espera pela conexao do usuario
	if((listen(server_socket,8))==-1)
	{
		perror("listen error: ");
		exit(-1);
	}
	
	//Cria a estrutura de endeço socket para cada cliente
	struct sockaddr_in client;
	int client_socket;
	unsigned int len=sizeof(sockaddr_in);

	//Cria um sinal de chamada caso o servidor pressione CTRL+C
	signal(SIGINT, catch_ctrl_c);

	printMessage("\t||Server Started||\n\n");
	while(1)
	{
		//Espera a conexao do usuario
		if((client_socket=accept(server_socket,(struct sockaddr *)&client,&len))==-1)
		{
			cout <<  "Socket do servidor finalizado\n";
			exit(-1);
		}
		//Cria uma thread para lidar com envios e recebimentos de mensagens com o usuario
		thread t(handle_client,client_socket);

		//Armazena o endereço ip de cada cliente
		char ip[INET_ADDRSTRLEN];
		if (getpeername(client_socket, (struct sockaddr*)&client, &len) == 0) {
			inet_ntop(AF_INET, &(client.sin_addr), ip, INET_ADDRSTRLEN);
		} else {
			perror("Erro ao obter o endereço IP remoto");
		}
		//Adiciona o usuaio a lista geral de ususarios
		lock_guard<mutex> guard(clients_mtx);
		clients.push_back({string("Anonymous"),client_socket,(move(t)), string(""), string(ip)});
	}
	
	//Espera que suas threads sejam finalizadas
	for (auto itr = clients.begin(); itr != clients.end(); itr++) {
		if(itr->th.joinable())
			itr->th.join();
    }
	//Fecha o socket do servidor
	close(server_socket);
	return 0;
}

//signal: inteiro que armazena o signal à ação CTRL+C
//Caso seja pressionado CTRL + C, o servidor desconecta todos usuarios e finaliza seu socket
void catch_ctrl_c(int signal){
	while(clients.size() != 0)
		closeClientConnection(clients.front().socket);
	close(server_socket);
	close(signal);
}

//message: mensagem a ser exibida no terminal do servidor
//Escreve uma mensagem no terminal do servidor
void printMessage(string message){
	lock_guard<mutex> guard(cout_mtx);
	cout << message;
	cout.flush();
}

//Fecha a conexao de um cliente dado seu socket
void closeClientConnection(int client_socket){
	//Retorna o cliente a ser removido
	Clients *cl = return_client(client_socket);
	if(cl == NULL) return;
	
	//Exibe para todos do canal que o cliente saiu do servidor
	string msg = cl->name + " saiu do servidor\n";
	broadcast_Message(client_socket, msg, true);
	printMessage(msg);

	string name_channel = cl->name_channel;
	clients_mtx.lock();
	if(name_channel != ""){
		//Remoção do cliente retornado do canal, caso ele esteja em algum canal
		channel[name_channel].cl.remove(cl);
		if(channel[name_channel].cl.size() == 0){
			channel[name_channel].socket_adm = -1;
		} else{
			//Se o usuario a ser removido é o adm, promovemos outro adm
			if(channel[name_channel].socket_adm == cl->socket){
				//Caso o adm tenha sido removido:
				if(channel[name_channel].cl.size() == 0){
					//Se a sala se tornou vazia o adm é retirado
					channel[name_channel].socket_adm = -1;
				} else{
					//Adiciona o socket do segundo usuario como adm
					channel[name_channel].socket_adm = channel[name_channel].cl.front()->socket;
					channel[name_channel].cl.front()->adm = true;
				}
				
			}
		}
	}
	//Envia ao cliente que foi removido que sua conexao foi finalizada
	send_message_client(client_socket, string("kicked"));
		
	//Fecha conexao do socket do cliente
	close(cl->socket);
	//Finaliza a thread do cliente
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

//client_socket: numero de socket do cliente
//name: novo apelido do cliente
//Muda o nome do cliente para outro 
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

//client_socket: numero de socket do cliente
//Retorna o cliente que corresponde ao numero de socket enviado
Clients *return_client(int client_socket){
	lock_guard<mutex> guard(clients_mtx);
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if(it->socket == client_socket){
			return &(*it);
		}
    }
	return nullptr;
}

//client_socket: numero de socket do cliente
//message: mensagem a ser enviada
//Envia uma mensagem ao cliente correspondente àquele socket
void send_message_client(int client_socket, string message){
	char *msg = (char *) malloc(message.length()+1);
	strcpy(msg, message.c_str());

	int qtd_messages = ceil((strlen(msg)/4096.0));
    for(int x = 0; x < qtd_messages; x++){
        
		int bytes_sent = 0;
		//Envia a mensagem 5 vezes ou a envia ate q o cliente receba
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

//client_socket: numero de socket do cliente
//message: mensagem a ser enviada
//fromServer: Determina se será uma mensagem enviada pelo servidor ou pelo usuario
//Função de broadcast da mensagem:
//Todos que estao no mesmo canal que o dono do socket recebem a mensagem
void broadcast_Message(int client_socket, string message, bool fromServer){
	Clients *actual_client;
	actual_client = return_client(client_socket);
	
	lock_guard<mutex> guard(clients_mtx);
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if(it->name_channel == actual_client->name_channel && /*it->name != actual_client->name && */actual_client->muted == false){
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

//name: nome do cliente
//Retorna o cliente correspondente ao nome enviado
Clients *return_client(string name){
	lock_guard<mutex> guard(clients_mtx);
	for (auto it = clients.begin(); it != clients.end(); it++) {
		if(it->name == name){
			return &(*it);
		}
    }
	return nullptr;
}

//client_socket: numero de socket vinculado ao cliente
//message: mensagem enviada pelo cliente
//Função que determina o comportamento do servidor dado a mensagem que o usuario enviou
void command_decide(int client_socket, char message[MAX_LEN]){

	//Lida com mensagens nao-comandos do usuario
	if(message[0] != '/' || strlen(message) > 30){
		Clients *cl = return_client(client_socket);
		//Verifica se o usuario se encontra em um servidor
		//caso contrario envia uma mensagem ao usuario de mensagem invalida
		if(cl == NULL) return;
		if(cl->name_channel == ""){
			send_message_client(client_socket, string("Digite um comando válido\n"));
			return;
		}
		//Envia ao usuario que ele se encontra mutado
		if(cl->muted == true){
			string msg = "Você esta mutado\n";
			send_message_client(client_socket, msg);
		} 
		broadcast_Message(client_socket, string(message), false);
		return;
	}
	//Pega a primeira palavra da mensagem, ou seja, o comando (/kick, /join ...)
	char *pch; 
    pch = strtok(message, " ");
	char firstWord[20];
	strcpy(firstWord, pch);
	//quit: finaliza a conexao do usuario
	if(strcmp(firstWord, "/quit") == 0){
		closeClientConnection(client_socket);
		return;
	}
	//ping: retorna ao usuario uma mensagem /pong
	if(strcmp(firstWord, "/ping") == 0){
		string msg = "Server: pong\n";
		send_message_client(client_socket, msg);
		Clients *cl = return_client(client_socket);
		msg = cl->name + ": " + "/ping\n";
		printMessage(msg);
		return;
	}
	//retorna ao usuario uma lista de comandos que podem ser executados no servidor
	if(strcmp(firstWord, "/help") == 0){
	
		string msg = "\n->Comandos habilitados:\n";
		msg += " -Usuários Comuns:\n";
		msg += "  /ping                       - solicita uma mensagem /pong de resposta do servidor\n";
		msg += "  /join nomeCanal             - se junta a um canal de nome enviado\n";
		msg += "  /nickname apelidoDesejado:  - muda seu apelido inicial para outro\n";
		msg += "  /bigmessage                 - envia uma mensagem de mais de 4096 caracteres ao servidor\n";
		msg += "  /whoami                     - exibe ao cliente suas informações pessoais\n\n";
		msg += " -Usuários administradores:\n";
		msg += "  /kick nomeUsuario           - finaliza a conexão de um usuário especificado\n";
		msg += "  /mute nomeUsuario           - impede o usuário especificado no mesmo canal de enviar mensagens no canal\n";
		msg += "  /unmute nomeUsuario         - retira o mute de um usuário do mesmo canal\n";
		msg += "  /whois nomeUsuario          - retorna o endereço ip do usuario no mesmo canal\n";
		msg += "  /whoami                     - exibe ao cliente suas informações pessoais\n\n";
		send_message_client(client_socket, msg);
		return;
	}
	if(strcmp(firstWord, "/whoami") == 0){
		Clients *cl = return_client(client_socket);
		string msg = "\n==Informações Pessoais==\n";
		msg += "Nome:         " + cl->name + "\n";
		msg += "Canal atual:  " + cl->name_channel + "\n";
		msg += "IP:           " + cl->ip + "\n";
		string aux = cl->muted ? "True" : "False";
		msg += "muted:        " + aux + "\n";
		aux = cl->adm ? "True" : "False";
		msg += "ADM:          " + aux + "\n\n";
		send_message_client(client_socket, msg);
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

		//Verifica se o nick ja existe
		if(return_client(new_nickname) != NULL){
			send_message_client(client_socket, string("O nickname digitado ja existe\n"));
			return;
		}
		//Modifica o apelido do usuario que fez o pedido
		actual_Client->name = new_nickname;

		string msg = "Server: \tApelido alterado com sucesso\n";
		msg += "Server: Apelido antigo: " + old_nickname + "\n" + "Server: Novo apelido: " + new_nickname + "\n";
		send_message_client(client_socket, msg);
		return;
	}

	//Verifica se o usuario buscado existe
	Clients *target_Client = return_client(string(secondWord));
	if(target_Client == NULL){
		string msg = "Usuario " + string(secondWord) + " nao encontrado\n";
		send_message_client(client_socket, msg);
		return;
	}

	//Comandos que dependem que todos estejam na mesma sala
	if(target_Client->name_channel != actual_Client->name_channel) return;
	if(strcmp(firstWord, "/kick") == 0){
		//Fecha a conexao do cliente
		if(actual_Client->name_channel == target_Client->name_channel && actual_Client->adm == true){
			closeClientConnection(target_Client->socket);
			string msg = "O usuario " + target_Client->name + " foi kickado\n";
			send_message_client(client_socket, msg);
		}
		return;
	}
	if(strcmp(firstWord, "/mute") == 0){
		if(actual_Client->adm == true){
			if(target_Client->muted){
				send_message_client(client_socket, string("Usuário ja se encontra mutado\n"));
				return;
			}
			target_Client->muted = true;
			string msg = "O usuario " + target_Client->name + " foi mutado\n";
			send_message_client(client_socket, msg);
		}
		return;
	}
	if(strcmp(firstWord, "/unmute") == 0){
		if(actual_Client->adm == true){
			if(!target_Client->muted){
				send_message_client(client_socket, string("Usuário ja se encontra desmutado\n"));
				return;
			}
			target_Client->muted = false;
			string msg = "O usuario " + target_Client->name + " foi desmutado\n";
			send_message_client(client_socket, msg);
		}
		return;
	}
	if(strcmp(firstWord, "/whois") == 0){
		if(actual_Client->adm == true){
			string ip = "Server: Endereco IP de " + string(secondWord) + ": " + target_Client->ip+ "\n";
			send_message_client(client_socket, ip);
		}
		return;
	}

	
}
//name: apelido anterior que sera utilizado para gerar o novo apelido
//Gera um novo apelido combinando o apelido anterior com uma sequencia de numeros
string generate_nickName(string name){
	int random = 1 + (rand() % 1001);
	string new_name = name + to_string(random);
	return new_name;
}


//client_socket: numero de socket do cliente
//Função que lida com as mensagens enviadas pelo cliente 
void handle_client(int client_socket)
{	
	//Recebe o nome apos o usuario se conectar ao servidor
	char name[MAX_LEN];
	int bytes_received = recv(client_socket,name,sizeof(name),0);
	if(bytes_received <= 0)
		return;
	
	//Cria um novo apelido caso o anterior ja exista no servidor
	string new_name = string(name);
	while(return_client(new_name) != NULL){
		new_name = generate_nickName(new_name);
	}

	char aux[MAX_LEN];
	strcpy(aux, new_name.c_str());

	if(strcmp(name, new_name.c_str()) != 0){
		send_message_client(client_socket, string("Apelido ja existente, seu apelido será " + new_name));
	}
	//Define o nome do usuario
	setClientName(client_socket, aux);
	string msg = "\n\tBem vindo ao servidor " + string(aux) + "\n\n";
	send_message_client(client_socket, msg);

	while(1){
		char message[MAX_LEN];
		int bytes_received = recv(client_socket,message,sizeof(message),0);
		if(bytes_received <= 0)
			return;
		command_decide(client_socket, message);
	}
}