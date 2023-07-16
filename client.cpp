#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <signal.h>
#include <mutex>
#include <chrono>

// #include <ncurses.h>


#define MAX_LEN 4096


using namespace std;

bool exit_flag=false, connected = false;
thread t_send, t_recv;
int client_socket = -1;


mutex send_mtx,receive_mtx;
mutex cout_mtx;

void catch_ctrl_c(int signal);
string color(int code);
int eraseText(int cnt);
void send_message(int client_socket);
void recv_message(int client_socket);



//Cria o socket e configura ele
struct sockaddr_in create_socket(){
    struct sockaddr_in client;
	client.sin_family=AF_INET;
	client.sin_port=htons(10000); // Port no. of server
	client.sin_addr.s_addr=INADDR_ANY;
	bzero(&client.sin_zero,0);
    return client;
}

void set_socket(){
    if((client_socket=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		//Imprimi socket + erro do socket
		perror("socket: ");
		exit(-1);
    }
}

void connect_socket(struct sockaddr_in client){
    if((connect(client_socket,(struct sockaddr *)&client,sizeof(struct sockaddr_in)))==-1)
	{
		perror("connect: ");
		exit(-1);
	}
}

struct sockaddr_in client = create_socket(); 

int main()
{
  
    set_socket();
    //connect_socket(client);


    //Create signal dealer of ctrl+c interruption
    signal(SIGINT, catch_ctrl_c);
    

   
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

// Handler para "Ctrl + C"
void catch_ctrl_c(int signal) 
{
    cout << "Operção nao permitida" << endl;    	
}


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
            line[0] = '\0';
            return line;
        } else
            line[cont-1] = aux;
        cont++;
    } while(aux != '\n' && aux != EOF);
   
    return line;
}

void close_socket(){
    exit_flag=true;
	t_send.detach();
	t_recv.detach();
    cout << "Conexão com o servidor finalizada" << endl;
    fflush(stdout);
}


void command_decide(char *input){

    if(connected == false){
        if(strcmp(input, "/connect") == 0){
            connect_socket(client);
            cout << "\tConectado ao servidor!!!" << endl;
            connected = true;
            //Send name after connect to server
            char name[MAX_LEN];
	        cout << "Digite seu apelido: ";
	        cin.getline(name,MAX_LEN);
            if(strlen(name) != 0)
	            send(client_socket,name,sizeof(name),0);
        } else if(strlen(input) == 0 || strcmp(input, "/quit") == 0){
            char msg[MAX_LEN] = "/quit\0";
            send(client_socket,msg,MAX_LEN,0);
            close_socket();
        } else{
            string msg = "Conexão não realizada\nComando Inválido\n";
            cout << msg;
        }
    } else{
        if(strcmp(input, "/connect") == 0){
            cout << "Cliente ja conectado ao servidor" << endl;
            return;
        }
        //Se for 0 significa que o usuario pressionou CTRL+D
        else if(strlen(input) == 0 || strcmp(input, "/quit") == 0){
            char msg[MAX_LEN] = "/quit\0";
            send(client_socket,msg,MAX_LEN,0);
            close_socket();
        } else if(strcmp(input, "/bigMessage") == 0){
            // Realiza o envio de varios pacotes com 1 mesma mensagem
            char *bigMessage = (char *)malloc(9000);
            for(int x = 0; x < 9000; x++){
                if(x % 2 == 0)
                    bigMessage[x] = 'a';
                else
                    bigMessage[x] = 'b';
            }
            bigMessage[8999] = '\0';

            //Envia todas as demais operações, alem das mensagens
            int qtd_messages = ceil((strlen(bigMessage)/4096.0));
            for(int x = 0; x < qtd_messages; x++){
                send(client_socket,bigMessage+4096*x,4096,0);
            }   
            free(bigMessage);
        } else{
            // Envia todas as demais operações, alem das mensagens
            int qtd_messages = ceil((strlen(input)/4096.0));
            for(int x = 0; x < qtd_messages; x++){
                send(client_socket,input+4096*x,4096,0);
            }   
        }
    }
}

void print_options(){
    
    cout << " - Comandos:" << endl;
    cout << "  - /Pré-conexao:" << endl;
    cout << "   - /connect: Se conecta ao servidor, caso ainda nao esteja conectado" << endl << endl;
    
    cout << "  - /Pós-conexao:" << endl;
    cout << "   - /Usuários comuns:" << endl;
    cout << "    - /join nomeCanal: se junta a um canal de nome enviado" << endl;
    cout << "    - /nickname apelidoDesejado: muda o apelido inicial para outro" << endl;
    cout << "    - /ping: solicita uma mensagem /pong de resposta do servidor" << endl << endl;

    cout << "   - /Usuários administradores:" << endl;
    cout << "    - /kick nomeUsuario: finaliza a conexão de um usuário especificado" << endl;
    cout << "    - /mute nomeUsuario: impede um determinado usuário de enviar mensagens no canal" << endl;
    cout << "    - /unmute nomeUsuario: retira o mute do usuário" << endl;
    cout << "    - /whois nomeUsuario: retorna o endereço ip do usuário" << endl << endl;
       
}


//Função que controla o envio de mensagens para o servidor
void send_message(int client_socket)
{
    print_options();
	while(1)
	{   
        fflush(stdout);
        cout << "Você : ";
        char *input = getLine();   
        command_decide(input);
        
        if(exit_flag)
            return;
        //std::chrono::milliseconds foo(1000);
        this_thread::sleep_for(chrono::milliseconds(200));
	}		
}

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
        cout << message;
        fflush(stdout);
	}	
}
