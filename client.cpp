#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <signal.h>

#define MAX_LEN 4096

using namespace std;

bool exit_flag=false, connected = false;
thread t_send, t_recv;
int client_socket = -1;

//Cria o socket e configura ele
struct sockaddr_in create_socket(){
    struct sockaddr_in client;
	client.sin_family=AF_INET;
	client.sin_port=htons(9997); // Port no. of server
	client.sin_addr.s_addr=INADDR_ANY;
	bzero(&client.sin_zero,0);
    return client;
}

//Seta o socket
void set_socket(){
    if((client_socket=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		//Imprimi que houve erro na criação do socket
		cout << "Criação do socket não foi concluída";
		exit(-1);
    }
}

//Conecta ao socket
int connect_socket(struct sockaddr_in client){
    if((connect(client_socket,(struct sockaddr *)&client,sizeof(struct sockaddr_in)))==-1)
	{
		return -1;
	}
    return 1;
}

// Handler para "Ctrl + C"
void catch_ctrl_c(int signal) 
{
    cout << "Operção nao permitida" << endl;    	
}

//Função getline dinamico
char *getLine(){
    char aux;
    char *line = NULL;
    int cont = 1;
    do{
        aux = getchar();  
        
        line = (char *) realloc(line, cont * sizeof(char));
        if(aux == '\n'){
            line[cont-1] = '\0';
        } else  if(aux == EOF){
            line = (char *) realloc(line, 1 * sizeof(char));
            line[0] = '\n';
            return line;
        } else
            line[cont-1] = aux;
        cont++;
    } while(aux != '\n' && aux != EOF);
   
    return line;
}

//Finaliza o socket do cliente
void close_socket(){
    exit_flag=true;
    connected = false;
	t_send.detach();
	t_recv.detach();
    cout << "Conexão com o servidor finalizada\n" << endl;
    fflush(stdout);
}

void command_decide(char *input){
    if(exit_flag)
			return;

    //Caso o cliente esteja conectado com o servidor
    if(connected == false){
        if(strcmp(input, "/connect") == 0){
            //Realiza a tentativa de conexao com o servidor
            if(connect_socket(client) == -1){
                cout << "error: Connection refused\n";
                return;
            }
            cout << "\tConectado ao servidor!!!" << endl;
            connected = true;
            //Envia o nome apos a conexao sucedida com o servidor
            
	        cout << "Digite seu apelido: ";
            char *name = getLine();
            //Verifica se o usuario pressinou CTRL + D
            if(name[0] != '\n')
	            send(client_socket,name,sizeof(name),0);
            else
                close_socket();
            //Finaliza o socket com o servidor caso o cliente digite /quit ou pressione CTRL + D
        } else if(input[0] == '\n' || strcmp(input, "/quit") == 0){
            if(exit_flag != true) close_socket();
            else return;
        } else{
            cout << "Comando Inválido\n";
        }
    //Caso o cliente nao esteja conectado com o servidor
    } else{
        if(strcmp(input, "/connect") == 0){
            cout << "Cliente ja conectado ao servidor" << endl;
            return;
        }
        //Finaliza a conexao com o servidor caso o cliente digite /quit ou pressione CTRL + D
        else if(input[0] == '\n' || strcmp(input, "/quit") == 0){
            char msg[MAX_LEN] = "/quit\0";
            send(client_socket,msg,MAX_LEN,0);
            close_socket();
        //Função para testar o envio de mensagens com corpo maiores que 4096 caracteres
        } else if(strcmp(input, "/bigMessage") == 0){
            // Realiza o envio da mensagem em varios pacotes de tamanho maximo de 4096 caracteres
            char *bigMessage = (char *)malloc(9000);
            for(int x = 0; x < 9000; x++){
                if(x % 2 == 0)
                    bigMessage[x] = 'a';
                else
                    bigMessage[x] = 'b';
            }
            bigMessage[8999] = '\0';

            int qtd_messages = ceil((strlen(bigMessage)/4096.0));
            for(int x = 0; x < qtd_messages; x++){
                send(client_socket,bigMessage+4096*x,4096,0);
            }   
            free(bigMessage);
        } else{
            // Envia as mensagens comuns ou operações ao servidor
            int qtd_messages = ceil((strlen(input)/4096.0));
            for(int x = 0; x < qtd_messages; x++){
                send(client_socket,input+4096*x,4096,0);
            }   
        }
    }
}

//Função que imprime as opções disponiveis de comandos ao cliente antes da conexao
void print_options(){
    string msg = "\n->Comandos habilitados:\n";
    msg += "  /connect: Realiza a conexão com o servidor\n\n";
    cout << msg;
}

//client_socket: numero de socket do cliente
//Função que controla o envio de mensagens para o servidor
void send_message(int client_socket)
{
    print_options();
	while(1)
	{      
        char *input = getLine();   
        if(exit_flag)
			return;
        
        command_decide(input);
        free(input);
         if(exit_flag)
			return;
	}		
}

//client_socket: numero de socket do cliente
//Função que controla o recebimento de mensagens do servidor
void recv_message(int client_socket)
{
	while(1)
	{
        if(exit_flag)
			return;
        char message[MAX_LEN];
      
        int bytes_received=recv(client_socket,message,sizeof(message),0);
        if(bytes_received == 0){
            close_socket();
            return;
        }
        if(strcmp(message, "kicked") == 0){
            cout << "Você foi kickado\n" << endl;
            fflush(stdout);
            close_socket();
            return;
        }
        cout << message;
        fflush(stdout);
	}	
}


struct sockaddr_in client = create_socket(); 

int main()
{   
    set_socket();

    //Cria um sinal de interrupção
    signal(SIGINT, catch_ctrl_c);
    
    //Threads de leitura da mensagem e de envio para o servidor
	thread t1(send_message, client_socket);
	thread t2(recv_message, client_socket);

    t_send=move(t1);
	t_recv=move(t2);

	if(t_send.joinable())
	    t_send.join();
	if(t_recv.joinable())
	    t_recv.join();

	return 0;
}

