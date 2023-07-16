# ProjetoRedes-Sockets
## Softwares utilizados:
 - Linux: Ubuntu 22.04.2 LTS
 - g++ 11.3.0
   
## Instalação
- Compile os arquivos server.cpp e o client.cpp:
  - Utilize o comando 'make all'(sem as aspas) para isso

## Execução
- Abra um terminal e execute 'make run_server'(sem as aspas) para o servidor
- Abra quantos terminais quiser e execute 'make run_client'(sem as aspas) em cada terminal para os clientes 

## Comandos implementados
- /connect - Estabelece a conexão com o servidor
- /quit - O cliente fecha a conexão e fecha a aplicação
- /ping - O servidor retorna "pong"assim que receber a mensagem
- /join nomeCanal - Entra no canal
- /nickname apelidoDesejado - O cliente passa a ser reconhecido pelo apelido especificado
- /kick nomeUsurio - Fecha a conexão de um usuário especificado
- /mute nomeUsurio - Faz com que um usuário não possa enviar mensagens neste canal
- /unmute nomeUsurio - Retira o mute de um usuário
- /whois nomeUsurio - Retorna o endereço IP do usuário apenas para o administrador

## Comandos Extras implementados
- /whoAmI - Exibe os dados para a pessoa que realizou o comando
- /help - Exibe a lista de comandos
- /bigMessage - Envia uma mensagem de 9000 caracteres no canal conectado. A mensagem é dividida em blocos de 4096 caracteres.

