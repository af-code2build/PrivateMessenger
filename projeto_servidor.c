#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>

#define SERVER_PORT 9000   // Porto do servidor
#define BUF_SIZE 1024      // Tamanho para um buffer genérico
#define MIN_SIZE 50        // Username e password minimo
#define MAX_SIZE 1500     // Username e passord maximo
#define MENS_BIG 10000     // Tamanho da mensagem

typedef struct info_cliente
{
    char name[MIN_SIZE];
    char username[MIN_SIZE];
    char password[MIN_SIZE];
    bool adimn;
    struct info_cliente *next;

} info_cliente;

typedef struct msg_cliente
{
    char assunto[BUF_SIZE];
    char mensagem[MENS_BIG];
    char transmissor[MIN_SIZE];      // Tem que ser do mesmo tamanho que o name ou username do info_cliente
    bool vista;
    time_t seconds;                  // Timestamp
    struct msg_cliente *next;

} msg_cliente;


void Sec_to_normal(time_t seconds,int client_fd);
void arvore_directorias(bool start, bool limpar, info_cliente *info);
bool check_login(int fd, bool *admin, info_cliente *id_cliente);
void check_vista(int client_fd, char *path);
void process_client(int client_fd);
void menu_client(int client_fd, info_cliente *id_cliente);
void menu_admin(int client_fd, info_cliente *id_cliente);
void erro(char *msg);
bool pedido_admissao(int client_fd);
info_cliente *busca_do_ficheiro_id(char *path_ficheiro);
msg_cliente *busca_do_ficheiro_msg(char path_ficheiro[]);
void liberta_lista(info_cliente *inicio);
void liberta_lista_msg(msg_cliente *inicio);
bool guarda_no_ficheiro(char *path_ficheiro, info_cliente *inicio_lista);
bool guarda_no_ficheiro_msg(char *path_ficheiro, msg_cliente *inicio_lista);
bool inserir_novo_cliente(char *path, info_cliente *novo_cliente);
bool inserir_nova_msg(char *path, msg_cliente *novo_msg,int client_fd);
bool pesquisar_lista(info_cliente *inicio, info_cliente *info, bool *check_admin);
void mostra_lista(int client_fd, char *path);
bool elimina_cliente(char *path, info_cliente *cliente_eliminar);
void elimina_msg_lidas(char *path, int client_fd,msg_cliente *elimina, msg_cliente *inicio);
bool escolha_cliente(int client_fd, char *path, info_cliente* resultado);
void remove_char_from_string(char c, char *str);
void envia_mensagem(int client_fd, char * username_atual);
void show_to_eliminar(char *path, int client_fd);
void alter_pass(int client_fd, info_cliente * atual);



// ************************************************************* FUNÇÃO MAIN ****************************************************************
// Implementação da programação por sockets e utilização de forks para criar um processo filho que "atende" cada cliente
int main()
{

    arvore_directorias(true, false, NULL);

    int fd, client;
    struct sockaddr_in addr, client_addr;
    int client_addr_size;

    bzero((void *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        erro("na funcao socket");

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        erro("na funcao bind");

    if (listen(fd, 5) < 0)
        erro("na funcao listen");

    while (1)
    {

        client_addr_size = sizeof(client_addr);

        client = accept(fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);

        if (client > 0)
        {
            if (fork() == 0)
            {
                close(fd);
                int read_bytes = 0;
                char buf[BUF_SIZE];
                bool admin = false;
                info_cliente id_cliente = {" ", " ", " ", false, NULL}; // guarda informação sobre o username e password do atual cliente

                if (check_login(client, &admin, &id_cliente)){

                    if (admin)
                    {
                        sprintf(buf, "\n\n\nBem vindo ao Serviço de Mensagens %s!\n", id_cliente.username);
                        write(client, buf, 1 + strlen(buf));
                        menu_admin(client, &id_cliente);
                    }
                    else
                    {
                        sprintf(buf, "\n\n\nBem vindo ao Serviço de Mensagens %s!\n", id_cliente.username);
                        write(client, buf, 1 + strlen(buf));
                        menu_client(client, &id_cliente);     //Client é para comunicar, id_cliente é o toda a informação do username
                    }
                }
                else{
                    while (1)
                    {
                        strcpy(buf, "\n\n\nNão tem premissão para aceder a este Serviço de Mensagens!\n\n Pretende:\n 1 -> Fazer um perdido de admissão;\
                                    \n 2 -> Terminar.\n\n");

                        write(client, buf, 1 + strlen(buf));
                        read_bytes = read(client, buf, MIN_SIZE - 1);
                        buf[read_bytes] = '\0';

                        remove_char_from_string('\r',buf);
                        remove_char_from_string('\n',buf);

                        if (strcmp(buf, "1") == 0)
                        {
                            bool aux;
                            char path_espera[] = "Raiz/Login/fila_espera.txt";
                            info_cliente *inicio_lista_espera = busca_do_ficheiro_id(path_espera);

                            if (pesquisar_lista(inicio_lista_espera, &id_cliente, &aux))
                            {
                                strcpy(buf, "\nEncontra-se em lista de espera!\n Obrigado pela preferência dos nossos serviços! \n\n");
                                write(client, buf, 1 + strlen(buf));
                            }
                            else
                            {
                                if(pedido_admissao(client))
                                    printf("Um novo cliente foi acrescentado á lista de espera\n");
                                strcpy(buf, "\nEncontra-se agora em lista de espera!\n Obrigado pela preferência dos nossos serviços! \n\n");
                                write(client, buf, 1 + strlen(buf));
                            }

                            liberta_lista(inicio_lista_espera);
                            break;
                        }

                        if (strcmp(buf, "2") == 0)
                            break;
                    }
                }
                exit(0);
            }
            close(client);
        }
    }
    return 0;
}

// ************************************************************* FUNÇÃO CHECK_LOGIN ******************************************************************
// Esta função realiza a verificação das credenciais do utilizador (username e password). Se estas existirem dentro da lista de utilizadores
// admitidos, então ela devolve true, caso contrário devolve false. É também identificado se o utilizador é o administrador.
// As informações do cliente que acabou de entrar no servidor (o seu username e password) são passadas para fora da função para serem utilizadas na main().

bool check_login(int client_fd, bool *admin, info_cliente *pessoa)
{
    char path_login[] = "Raiz/Login/clientes.txt";
    info_cliente *inicio_lista_login = busca_do_ficheiro_id(path_login);
    int nread = 0;
    char buf[BUF_SIZE];
    bool accepted = false;

    strcpy(buf, "\nIntroduza as suas credenciais de acesso:\n\nUsername: ");
    write(client_fd, buf, 1 + strlen(buf));

    nread = read(client_fd, pessoa->username, MIN_SIZE - 1);
    pessoa->username[nread] = '\0';
    remove_char_from_string('\r',pessoa->username);
    remove_char_from_string('\n',pessoa->username);

    strcpy(buf, "\n\nPassword: ");
    write(client_fd, buf, 1 + strlen(buf));

    nread = read(client_fd, pessoa->password, MIN_SIZE - 1);
    pessoa->password[nread] = '\0';
    remove_char_from_string('\r',pessoa->password);
    remove_char_from_string('\n',pessoa->password);
    nread = 0;

    accepted = pesquisar_lista(inicio_lista_login, pessoa, admin);

    liberta_lista(inicio_lista_login);

    return accepted;
}
// ************************************************************* FUNÇÃO CHECK_VISTA ******************************************************************
// Esta função realiza a verificação se as existe mensagens com a flag vista com o valor false. Se estas existir informa ao cliente que existem
// 'x' mensagens nao lidas

void check_vista(int client_fd, char *path)
{
    int i=0;

    msg_cliente *inicio = busca_do_ficheiro_msg(path);

    msg_cliente *atual = inicio;

    char buffer[BUF_SIZE];

    if (inicio == NULL){
        strcpy(buffer, "\n\nNao tem qualquer mensagem\n\n");
        write(client_fd, buffer, 1 + strlen(buffer));
    }
    else
    {
        while (atual != NULL)
        {
            if(atual->vista == false){
                i=i+1;
            }
            atual = atual->next;
        }

        if(i==0){
            strcpy(buffer, "\n\nTodas as mensagens foram lidas\n\n");
            write(client_fd, buffer, 1 + strlen(buffer));
        }
        else{
            sprintf(buffer, "\n\nTem %d mensagens não lidas\n\n",i);
            write(client_fd, buffer, 1 + strlen(buffer));
        }
    }

    liberta_lista_msg(inicio);
}

// *********************************************************** FUNÇÃO  ENVIA_MENSAGEM ********************************************************************
// O utilizador terá a possibilidade de enviar uma mensagem para outros utilizadores, seja para um ou para vários utilziadores.

void envia_mensagem(int client_fd, char * username_atual)
{
    char buffer[BUF_SIZE], pessoas[MENS_BIG];

    msg_cliente novo;

    const char c[2] = ";";
    char *token;

    ssize_t bytes_read = 0;

    strcpy(buffer, "\n\nINSTRUÇÕES: \nPara inserir vários destinatários, use ';' para separar cada um deles. No final coloque também um ';' \n\nInsira os usernames para quem quer enviar: ");

    write(client_fd, buffer, 1 + strlen(buffer));

    if ((bytes_read = read(client_fd, pessoas, BUF_SIZE - 1)) == -1){

        perror("Erro a ler os usernames dos destinatários da mensagem");
        return;
    }
    pessoas[bytes_read] = '\0';

    remove_char_from_string(' ',pessoas);     // Retira os seguintes carcteres para evitar erros na leitura
    remove_char_from_string('\r',pessoas);
    remove_char_from_string('\n',pessoas);

    strcpy(buffer, "\n\nInsira o assunto: ");
    write(client_fd, buffer, 1 + strlen(buffer));

    if ((bytes_read = read(client_fd, novo.assunto, BUF_SIZE - 1)) == -1){

        perror("\n\nErro a ler o assunto da mensagem\n\n");
        return;
    }

    novo.assunto[bytes_read] = '\0';
    remove_char_from_string('\r',novo.assunto);
    remove_char_from_string('\n',novo.assunto);

    strcpy(buffer, "\n\nInsira a mensagem a enviar: ");
    write(client_fd, buffer, 1 + strlen(buffer));

    if ((bytes_read = read(client_fd, novo.mensagem, MENS_BIG - 1)) == -1){

        perror("\n\nErro a ler a sua mensagem que é para enviar\n\n");
        return;
    }
    novo.mensagem[bytes_read] = '\0';

    novo.vista=false;
    strcpy(novo.transmissor,username_atual);
    novo.next=NULL;

    time(&novo.seconds);           // Devolve os segundos

    token = strtok(pessoas, c);    //Obtem o primeiro username

    while( token != NULL ) {

        sprintf(buffer, "Raiz/%s/msg.txt", token);

        if(!(inserir_nova_msg(buffer,&novo,client_fd))){
            printf("\n\nNao foi possivel guardar a mensagem\n\n");
        }

        token = strtok(NULL, c);
    }
}

// *********************************************************** FUNÇÃO  MENU_CLIENT ********************************************************************
// Esta função vai apresentar todas as funcionalidades que estão disponíveis ao Cliente

void menu_client(int client_fd, info_cliente * atual)
{

    char menu[BUF_SIZE]; // Para passar o que fazer no menu

    char buffer[BUF_SIZE];
    char op[30];


    ssize_t bytes_read = 0;

    while (1)
    {
        strcpy(menu, "\nEscolha uma das seguintes opções:\n\n 1 -> Ver sem tem mensagens nao lidas\n\n 2 -> Ver caixa de entrada e abrir mensagens\n\n 3 -> Eliminar as mensagens vistas\n\n 4-> Enviar mensagem\n\n 5-> Alterar password\n\n 6-> Sair\n\n");

        write(client_fd, menu, 1 + strlen(menu));

        if ((bytes_read = read(client_fd, op, BUF_SIZE - 1)) == -1)
            perror("Erro a ler a opção do cliente");

        op[bytes_read] = '\0';

        remove_char_from_string('\r',op);
        remove_char_from_string('\n',op);

        if (strcmp(op, "1") == 0)
        {
            sprintf(buffer, "Raiz/%s/msg.txt",atual->username);
            check_vista(client_fd,buffer);
        }

        if (strcmp(op, "2") == 0)
        {
            sprintf(buffer, "Raiz/%s/msg.txt",atual->username);
            mostra_lista(client_fd,buffer);
        }

        if (strcmp(op, "3") == 0)
        {
            sprintf(buffer, "Raiz/%s/msg.txt", atual->username);
            show_to_eliminar(buffer, client_fd);

        }

        if (strcmp(op, "4") == 0)
        {
            envia_mensagem(client_fd, atual->username);
        }

        if(strcmp(op,"5")==0){
            alter_pass(client_fd, atual);
        }

        if (strcmp(op, "6") == 0)
        {
            return;
        }
    }
}

// *********************************************************** FUNÇÃO  MENU_ADMIN ********************************************************************
// Esta função vai apresentar todas as funcionalidades que estão disponíveis ao Administrador

void menu_admin(int client_fd, info_cliente *id_cliente)
{

    char menu[BUF_SIZE];
    char op[30];

    ssize_t bytes_read = 0;
    info_cliente cliente_escolhido = {" ", " ", " ", false, NULL};
    char *path_login = "Raiz/Login/clientes.txt";
    char *path_espera = "Raiz/Login/fila_espera.txt";

    while (1)
    {

        strcpy(menu, "\nEscolha uma das seguintes opções:\n\n 1 -> Lista de pedidos de admissão\n\n 2 -> Eliminar acesso a utilizadores\n\n 3 -> Utilizar os serviços normais \n\n 4 -> Sair\n\n");

        write(client_fd, menu, 1 + strlen(menu));

        if ((bytes_read = read(client_fd, op, BUF_SIZE - 1)) == -1)
            perror("Erro a ler a opção do cliente");

        op[bytes_read] = '\0';

        remove_char_from_string('\r',op);
        remove_char_from_string('\n',op);

        if (strcmp(op, "1") == 0)
        {
            if (escolha_cliente(client_fd, path_espera, &cliente_escolhido))
            {
                if(inserir_novo_cliente(path_login, &cliente_escolhido))
                    printf("\n\nUm novo cliente foi inserido na lista de admitidos\n\n");

                if(elimina_cliente(path_espera, &cliente_escolhido))
                    printf("\n\nA lista de espera foi atualizada\n\n");

                arvore_directorias(false, false, &cliente_escolhido);
            }
        }

        if (strcmp(op, "2") == 0)
        {
            if (escolha_cliente(client_fd, path_login, &cliente_escolhido))
            {
                if(elimina_cliente(path_login, &cliente_escolhido))
                    printf("\n\nUm dos nossos clientes perdeu acesso a este serviço\n\n");

                arvore_directorias(false, true, &cliente_escolhido);
            }
        }

        if (strcmp(op, "3") == 0){
            menu_client(client_fd, id_cliente);
            return;
        }

        if (strcmp(op, "4") == 0)
            return;
    }
}

// ************************************************************ FUNÇÃO ERRO **********************************************************************
// Mostra os avisos de erro na criação do socket

void erro(char *msg)
{
    printf("Erro: %s\n", msg);
    exit(-1);
}

// ****************************************************** FUNÇÃO BUSCAR_DO_FICHEIRO ****************************************************************
// Vai buscar toda a informação guardada num determinado ficheiro e passa isso para memória dinâmica (não permanente)

info_cliente *busca_do_ficheiro_id(char *path_ficheiro)
{

    info_cliente *inicio = NULL;
    info_cliente *atual;
    info_cliente *novo;
    FILE *fd_file;
    int size = 0;

    if ((fd_file = fopen(path_ficheiro, "r")) == NULL)
    {
        fprintf(stderr, "\n\nError a abrir o ficheiro na função busca_do_ficheiro()\n\n");
        return NULL;
    }

    if (fd_file != NULL)
    {
        fseek(fd_file, 0, SEEK_END);
        size = ftell(fd_file);
        fseek(fd_file, 0, SEEK_SET);
    }

    if (size == 0)
    {
        printf("\n\n Na função busca_do_ficheiro(), encontramos o ficheiro vazio\n\n");
        return NULL;
    }
    else
    {
        while (1)
        {

            novo = malloc(sizeof(info_cliente));

            if (fread(novo, sizeof(info_cliente), 1, fd_file) != 1)
            {
                free(novo);
                break;
            }

            novo->next = NULL;

            if (inicio == NULL)
            {
                inicio = novo;
                atual = inicio;
            }
            else
            {
                atual->next = novo;
                atual = novo;
            }
        }
    }

    fclose(fd_file);
    return inicio;
}

// ****************************************************** FUNÇÃO BUSCAR_DO_FICHEIRO_MSG ****************************************************************
// Vai buscar toda a informação guardada num determinado ficheiro e passa isso para memória dinâmica (não permanente). Abre as mensagens que estão
// guardadas no ficheiro e passa todo para a memória dinâmica.

msg_cliente *busca_do_ficheiro_msg(char path_ficheiro[])
{
    msg_cliente *inicio = NULL;
    msg_cliente *atual;
    msg_cliente *novo;
    FILE *fd_file;

    int size = 0;

    if ((fd_file = fopen(path_ficheiro, "r")) == NULL)
    {
        fprintf(stderr, "\n\nErro a abrir o ficheiro na função busca_do_ficheiro_msg\n\n");
        return NULL;
    }

    if (fd_file != NULL)                // Confirma se o ficheiro esta vazio
    {
        fseek(fd_file, 0, SEEK_END);
        size = ftell(fd_file);
        fseek(fd_file, 0, SEEK_SET);
    }

    if (size == 0)
    {
        printf("\n\nO ficheiro está vazio na função busca_do_ficheiro_msg\n\n");
        return NULL;
    }
    else
    {
        while (1)
        {

            novo = malloc(sizeof(msg_cliente));

            if (fread(novo, sizeof(msg_cliente), 1, fd_file) != 1)
            {
                free(novo);
                break;
            }

            novo->next = NULL;

            if (inicio == NULL)
            {
                inicio = novo;
                atual = inicio;
            }
            else
            {
                atual->next = novo;
                atual = novo;
            }
        }
    }

    fclose(fd_file);
    return inicio;
}

// ***************************************************** FUNÇÃO INSERIR_NOVO_CLIENTE **************************************************************
// Esta função vai inserir um novo nó na lista que está em memória. Esta nova atualização que for feita á lista, vai também
// ser automáticamente guardada no ficheiro correto. A função devolve true ou false se toda a operação foi feita com sucesso ou não.
// Ao fim de tudo estar concluído, a função liberta toda a informação da lista que está em memória dinâmica.

bool inserir_novo_cliente(char *path, info_cliente *novo_cliente)
{

    info_cliente *atual, *anterior, *pesquisa, *novo;
    char path_login[] = "Raiz/Login/clientes.txt";
    char path_espera[] = "Raiz/Login/fila_espera.txt";
    info_cliente *inicio_lista = NULL;

    if(strcmp(path, path_login) == 0)
        inicio_lista = busca_do_ficheiro_id(path_login);

    if(strcmp(path, path_espera) == 0)
        inicio_lista = busca_do_ficheiro_id(path_espera);

    novo = malloc(sizeof(info_cliente));
    *novo = *novo_cliente;
    novo->next = NULL;

    if (inicio_lista == NULL)
    {
        inicio_lista = novo;

        if (!guarda_no_ficheiro(path, inicio_lista))
            printf("Erro, nao foi possivel guardar a nova informação no ficheiro\n");

        liberta_lista(inicio_lista);

        return true;
    }

    pesquisa = inicio_lista;

    while (pesquisa != NULL)
    {
        if ((strcmp(pesquisa->username, novo_cliente->username) == 0) && (strcmp(pesquisa->password, novo_cliente->password) == 0))
        {
            printf("\nEsta Pessoa ja esta registada!\n");
            free(novo);
            liberta_lista(inicio_lista);
            return false;
        }
        pesquisa = pesquisa->next;
    }

    atual = inicio_lista;
    anterior = NULL;

    while (atual != NULL)
    {
        anterior = atual;
        atual = atual->next;
    }

    if (anterior == NULL)
        inicio_lista = novo;
    else
        anterior->next = novo;

    printf("\nUm novo cliente foi inserido na lista\n");

    if (!guarda_no_ficheiro(path, inicio_lista))
    {
        printf("Erro, nao foi possivel guardar a nova informação no ficheiro\n");
        liberta_lista(inicio_lista);
        return false;
    }

    liberta_lista(inicio_lista);
    return true;
}

// ***************************************************** FUNÇÃO INSERIR_NOVA_MSG **************************************************************
// Esta função vai inserir um novo nó na lista que está em memória. Esta nova atualização que for feita á lista, vai também
// ser automáticamente guardada no ficheiro correto. A função devolve true ou false se toda a operação foi feita com sucesso ou não.
// Ao fim de tudo estar concluído, a função liberta toda a informação da lista que está em memória dinâmica.

bool inserir_nova_msg(char *path, msg_cliente *novo_msg, int client_fd)
{

    char buffer[BUF_SIZE];
    msg_cliente *atual, *anterior, *novo;
    msg_cliente *inicio_lista = NULL;

    inicio_lista = busca_do_ficheiro_msg(path);

    novo = malloc(sizeof(msg_cliente));
    *novo = *novo_msg;
    novo->next = NULL;

    if (inicio_lista == NULL)
    {
        inicio_lista = novo;

        if (!guarda_no_ficheiro_msg(path, inicio_lista))
            printf("\n\nErro, nao foi possivel guardar a nova informação no ficheiro de mensagens\n\n");

        liberta_lista_msg(inicio_lista);
        return true;
    }

    atual = inicio_lista;
    anterior = NULL;

    while (atual != NULL)
    {
        anterior = atual;
        atual = atual->next;
    }

    if (anterior == NULL)         //Confirmação do o caso anterior
        inicio_lista = novo;
    else
        anterior->next = novo;

    if (!guarda_no_ficheiro_msg(path, inicio_lista))
    {
        strcpy(buffer, "\n\nErro, nao foi possivel enviar a sua mensagem\n\n");
        write(client_fd, buffer, 1 + strlen(buffer));

        liberta_lista_msg(inicio_lista);
        return false;
    }

    liberta_lista_msg(inicio_lista);
    return true;
}

// ***************************************************** FUNÇÃO GUARDAR_NO_FICHEIRO **************************************************************
// Guarda toda a informação da lista (que atualmente está em memória dinâmica) num determinado ficheiro. A cabeça da lista é passada através do
// endereço do nó inicial guardado na variável inicio_lista.

bool guarda_no_ficheiro(char *path_ficheiro, info_cliente *inicio_lista)
{

    info_cliente *atual;

    atual = inicio_lista;
    FILE *fd_file;

    if ((fd_file = fopen(path_ficheiro, "w")) == NULL)
    {
        fprintf(stderr, "\nError to open the file\n");
        return false;
    }

    while (atual != NULL)
    {
        fwrite(atual, sizeof(struct info_cliente), 1, fd_file);
        atual = atual->next;
    }

    fclose(fd_file);
    return true;
}

// ***************************************************** FUNÇÃO GUARDAR_NO_FICHEIRO_MSG **************************************************************
// Guarda toda a informação da lista (que atualmente está em memória dinâmica) num determinado ficheiro. A cabeça da lista é passada através do
// endereço do nó inicial guardado na variável inicio_lista.
bool guarda_no_ficheiro_msg(char *path_ficheiro, msg_cliente *inicio_lista)
{
    msg_cliente *atual;

    atual = inicio_lista;
    FILE *fd_file;

    if ((fd_file = fopen(path_ficheiro, "w")) == NULL)
    {
        fprintf(stderr, "\n\nErro a abrir o ficheiro na função guardar_no_ficheiro_msg\n\n");
        return false;
    }

    while (atual != NULL)
    {
        fwrite(atual, sizeof(struct msg_cliente), 1, fd_file);
        atual = atual->next;
    }

    fclose(fd_file);
    return true;
}

// ***************************************************** FUNÇÃO LIBERTA_LISTA **************************************************************
// Vai libertar toda a memória dinamica que estava a ser utilizada para uma respetiva lista, cujo nó inicial é indicado
// pela variável "inicio"

void liberta_lista(info_cliente *inicio)
{
    info_cliente *atual, *proximo;

    atual = inicio;
    while (atual != NULL)
    {
        proximo = atual->next;
        free(atual);
        atual = proximo;
    }
}

// ***************************************************** FUNÇÃO LIBERTA_LISTA_MSG **************************************************************
// Vai libertar toda a memória dinamica que estava a ser utilizada para uma respetiva lista, cujo nó inicial é indicado
// pela variável "inicio"

void liberta_lista_msg(msg_cliente *inicio)
{
    msg_cliente *atual, *proximo;

    atual = inicio;
    while (atual != NULL)
    {
        proximo = atual->next;
        free(atual);
        atual = proximo;
    }
}

// ***************************************************** FUNÇÃO PESQUISAR_LISTA **************************************************************
// Verifica se o username e password de um cliente já existe na lista de utilizadores aceites. Pode também ser utilizada para
// descobrir se um determinado clintente está na lista de espera para ser admitido no sistema.

bool pesquisar_lista(info_cliente *inicio, info_cliente *info, bool *check_admin)
{

    info_cliente *atual = inicio;

    if (inicio == NULL)
        return false;
    else
    {
        while (atual != NULL)
        {

            if ((strcmp(atual->username, info->username) == 0) && (strcmp(atual->password, info->password) == 0)){
                if (atual->adimn){
                    *check_admin = true;
                }
                else{
                    *check_admin = false;
                }
                return true;
            }
            atual = atual->next;
        }
        return false;
    }
}

// ***************************************************** FUNÇÃO MOSTRA_LISTA **************************************************************
// A função mostra todas a mensagens onde apenas apresenta o transmissor, o assunto e se ja foi visto. Apos escolher qual quer ver será mostrada a
// mensagem que contém.

void mostra_lista(int client_fd, char *path)
{
    int i;
    ssize_t bytes_read = 0;

    msg_cliente *inicio = busca_do_ficheiro_msg(path);
    msg_cliente *atual = inicio;

    char buffer[MAX_SIZE + MENS_BIG];
    char op[MIN_SIZE];

    i=0;

    if (inicio == NULL){
        strcpy(buffer, "\n\nNao tem qualquer mensagem a listar\n\n");
        write(client_fd, buffer, 1 + strlen(buffer));
        return;
    }

    else
    {
        strcpy(buffer, "\n\n*********************************\n      MENSAGENS RECEBIDAS        \n*********************************\n\n");
        int j = strlen(buffer);

        while (atual != NULL)
        {
            i=i+1;
            sprintf(&buffer[j], "Num: %d\nRemetente: %s\nAssunto: %s\nVisualizada: %s\n\n",i,atual->transmissor,atual->assunto,(atual->vista ? "Verdadeiro" : "Falso"));
            j = strlen(buffer);
            atual = atual->next;
        }
        write(client_fd, buffer, 1 + strlen(buffer));
    }

    while(1){
        strcpy(buffer,"\nQual mensagem deseja ler? Indique o num (Se quiser sair coloque 0): ");
        write(client_fd,buffer, 1 + strlen(buffer));

        if ((bytes_read = read(client_fd, &op, sizeof(op))) == -1){
            perror("\n\nErro a ler a opção do cliente na função mostra_lista()\n\n");
        }
        else{
            break;
        }
    }

    op[bytes_read] = '\0';

    remove_char_from_string(' ',op);     // Retira os seguintes carcteres para evitar erros na leitura
    remove_char_from_string('\r',op);
    remove_char_from_string('\n',op);

    int res = 0;
    res = strtol(op, NULL, 10);

    atual = inicio;

    if(res==0){
        return;
    }
    else{
        for (i = 1; atual != NULL; i = i + 1)
        {
            if (i == res){
                    sprintf(buffer, "\n\n*************   MENSAGEM   ****************\n\nTransmissor: %s\nAssunto: %s\nMensagem: %s\n",atual->transmissor,atual->assunto,atual->mensagem);
                    write(client_fd, buffer, 1 + strlen(buffer));
                    Sec_to_normal(atual->seconds,client_fd);
                    atual->vista = true;
                    guarda_no_ficheiro_msg(path,inicio);
                    liberta_lista_msg(inicio);
                    return;
            }
            atual = atual->next;
        }
        strcpy(buffer,"\n\nNao existe essa mensagem, enganou-se no numero.\n\n");
        write(client_fd,buffer, 1 + strlen(buffer));
    }

    liberta_lista_msg(inicio);
}

// ***************************************************** FUNÇÃO ARVORE_DIRECTORIAS **************************************************************
// Esta função vai garantir que vamos sempre ter uma arvore de diretorias bem organizada, com os ficheiros associados a cada cliente admitido.
// A função: int stat(const char *pathname, struct stat *statbuf); vai devolver -1 no caso de a diretoria indicada em *pathname não existir,
// caso contrário devolve zero. O outro parâmetro é só uma struct utilizada para guardar informação sobre um determinado tipo de ficheiro.

void arvore_directorias(bool start, bool limpar, info_cliente *info)
{

    FILE *fd_file;
    struct stat st = {0};
    info_cliente admin = {"ADMIN", "admin", "123", true, NULL};
    char buf[BUF_SIZE];

    if (start)
    {

        // Diretorias Raiz
        if (stat("Raiz", &st) == -1)
        {
            mkdir("Raiz", 0777);
        }

        // Diretorias para os ficheiros do login
        if (stat("Raiz/Login", &st) == -1)
        {
            mkdir("Raiz/Login", 0777);


            if ((fd_file = fopen("Raiz/Login/clientes.txt", "w+")) == NULL)
            {
                fprintf(stderr, "\nError to open the file clientes.txt\n");
                exit(1);
            }

            fwrite(&admin, sizeof(struct info_cliente), 1, fd_file);
            fclose(fd_file);

            if ((fd_file = fopen("Raiz/Login/fila_espera.txt", "w+")) == NULL)
            {
                fprintf(stderr, "\nError to open the file fila_espera.txt\n");
                exit(1);
            }
            fclose (fd_file);
        }

        if (stat("Raiz/admin", &st) == -1)
        {
            mkdir("Raiz/admin", 0777);
            if ((fd_file = fopen("Raiz/admin/msg.txt", "w+")) == NULL)
            {
                fprintf(stderr, "\nError to open the file msg.txt do administrador\n");
                exit(1);
            }
            fclose (fd_file);
        }

    }
    else if (limpar)
    {
        // Remover diretorias e ficheiros

        sprintf(buf, "Raiz/%s/msg.txt", info->username);

        if (remove(buf) == 0){
            printf("\n\nO ficheiro de mensagens de %s foi eliminado.", info->username);

            sprintf(buf, "Raiz/%s", info->username);

            if (rmdir(buf) == 0)
                printf("\n\nA diretoria %s foi eliminada.\n\n", info->username);
            else
                printf("\n\nNão foi possível eliminar a diretoria %s\n\n", info->username);
        }
        else
            printf("\n\nNão foi possível eliminar o ficheiro de mensagens de %s\n\n", info->username);

    }
    else
    {
        // Diretorias para as mensagens entre clientes
        sprintf(buf, "Raiz/%s/msg.txt", info->username);

        if (stat(buf, &st) == -1){

            sprintf(buf, "Raiz/%s", info->username);
            mkdir(buf, 0777);

            sprintf(buf, "Raiz/%s/msg.txt", info->username);

            if ((fd_file = fopen(buf, "a")) == NULL)
            {
                fprintf(stderr, "\nErro a criar o ficheiro msg.txt\n");
                exit(1);
            }
            fclose (fd_file);
        }
    }
}

// ***************************************************** FUNÇÃO PEDIDO_ADIMISSAO **************************************************************
// Esta função vai pedir os dados da pessoa que se quere inscrever para usufruir dos serviços de mensagens. Vai pedir informação
// sobre o nome, username, e password do futuro utilizador para serem registados na lista de espera. O registo permanente é feito
// fazendo a chamada á função: inserir_novo_cliente().

bool pedido_admissao(int client_fd)
{

    int nread = 0;
    char buf[BUF_SIZE];

    info_cliente pessoa = {" ", " ", " ", false, NULL};

    strcpy(buf, "\nCriar uma nova conta:\n\n Nome: ");
    write(client_fd, buf, 1 + strlen(buf));

    nread = read(client_fd, pessoa.name, MIN_SIZE - 1);
    pessoa.name[nread] = '\0';
    remove_char_from_string('\r',pessoa.name);
    remove_char_from_string('\n',pessoa.name);

    strcpy(buf, "\n\n Username: ");
    write(client_fd, buf, 1 + strlen(buf));

    nread = read(client_fd, pessoa.username, MIN_SIZE - 1);
    pessoa.username[nread] = '\0';
    remove_char_from_string('\r',pessoa.username);
    remove_char_from_string('\n',pessoa.username);

    strcpy(buf, "\n\n Password: ");
    write(client_fd, buf, 1 + strlen(buf));

    nread = read(client_fd, pessoa.password, MIN_SIZE - 1);
    pessoa.password[nread] = '\0';
    remove_char_from_string('\r',pessoa.password);
    remove_char_from_string('\n',pessoa.password);

    char path_espera[] = "Raiz/Login/fila_espera.txt";

    return inserir_novo_cliente(path_espera, &pessoa);

}

// ***************************************************** FUNÇÃO ESCOLHA_CLIENTE **************************************************************
// Esta função começa por apresentar a lista de todos os clientes dentro da lista login ou na lista de espera (conforme o que é passado na
// variável "path"). O servidor pede ao cliente para escolher uma pessoa dessa lista e devolve (através do parâmetro resultado) todas
// as informações (nome, password,...) dessa pessoa.

bool escolha_cliente(int client_fd, char *path, info_cliente* resultado)
{

    char path_login[] = "Raiz/Login/clientes.txt";
    char path_espera[] = "Raiz/Login/fila_espera.txt";
    info_cliente *inicio = NULL;
    char titulo[MAX_SIZE];
    char buf[MAX_SIZE];
    int i = 0, count, nread, op;

    if(strcmp(path, path_login)==0){
        inicio = busca_do_ficheiro_id(path_login);
        strcpy(titulo, "\n\nLISTA DE UTILIZADORES DO SERVIDOR:\n");
    }

    if(strcmp(path, path_espera)==0){
        inicio = busca_do_ficheiro_id(path_espera);
        strcpy(titulo, "\n\nLISTA DE PEDIDOS DE ADMISSÃO:\n");
    }

    if(inicio == NULL && strcmp(path, path_espera)==0){
        strcpy(buf, "\nNeste momento, não existem pedidos de admissão!\n");
        write(client_fd, buf, 1 + strlen(buf));
        return false;
    }

    info_cliente *atual = inicio;
    sprintf(buf, "%s", titulo);
    i = strlen(buf);

    for (count = 1; atual != NULL; i = strlen(buf), count = count + 1)
    {

        sprintf(&buf[i], "\n %d -> NOME: %s\n", count, atual->name);
        atual = atual->next;
    }

    if (strcmp(titulo, "\n\nLISTA DE PEDIDOS DE ADMISSÃO:\n") == 0)
    {
        strcpy(&buf[i], "\nEscolha o número da pessoa para ser admitida: ");
    }
    else
    {
        strcpy(&buf[i], "\nEscolha o número da pessoa para ser eliminada da lista: ");
    }

    write(client_fd, buf, 1 + strlen(buf));
    nread = read(client_fd, buf, BUF_SIZE - 1);
    buf[nread] = '\0';

    op = strtol(buf, NULL, 10);     //vai converter uma string para um número

    atual = inicio;
    for (count = 1; atual != NULL; count = count + 1)
    {
        if (count == op)
            break;
        atual = atual->next;
    }

    if (atual == NULL){
        printf("\n\nA opção que escolheu não se encontra na lista!\n");
        liberta_lista(inicio);
        return false;
    }

    strcpy(resultado->name, atual->name);
    strcpy(resultado->username, atual->username);
    strcpy(resultado->password, atual->password);
    resultado->adimn = atual->adimn;
    liberta_lista(inicio);
    return true;
}

// ***************************************************** FUNÇÃO ELIMINA_CLIENTE **************************************************************
// Esta função vai eliminar um nó que se encontra na lista que está em memória. Esta nova atualização que for feita à lista, vai também
// ser automáticamente guardada no ficheiro correto. A função devolve verdadeiro ou falso se a atualização da lista foi bem feita e no
// fim de todas as operações, liberta toda a lista em memória

bool elimina_cliente(char *path, info_cliente *cliente_eliminar)
{
    char path_login[] = "Raiz/Login/clientes.txt";
    char path_espera[] = "Raiz/Login/fila_espera.txt";
    info_cliente *inicio_lista = NULL;

    if(strcmp(path, path_login) == 0)
        inicio_lista = busca_do_ficheiro_id(path_login);

    if(strcmp(path, path_espera) == 0)
        inicio_lista = busca_do_ficheiro_id(path_espera);

    info_cliente *atual = inicio_lista;
    info_cliente *anterior = NULL;

    if (atual == NULL)
    {
        printf("\nA lista esta vazia!\n");
        return false;
    }
    else
    {
        while (atual != NULL)
        {
            if ((strcmp(atual->username, cliente_eliminar->username) == 0) && (strcmp(atual->password, cliente_eliminar->password) == 0))
            {
                if (anterior == NULL){
                    inicio_lista = atual->next;
                }
                else{
                    anterior->next = atual->next;
                }
                free(atual);

                if (!guarda_no_ficheiro(path, inicio_lista))
                {
                    printf("Erro, nao foi possivel guardar a nova lista depois de eliminar um dos seus nós.\n Prima uma tecla:");
                }
                liberta_lista(inicio_lista);
                return true;
            }
            anterior = atual;
            atual = atual->next;
        }
    }

    liberta_lista(inicio_lista);
    return false;
}

// ***************************************************** FUNÇÃO SHOW_TO_ELIMINAR **************************************************************
// Esta função vai eliminar um nó que se encontra na lista que está em memória e o que o utlizador decide. Esta nova atualização que for feita à lista, vai também
// ser automáticamente guardada no ficheiro correto. No fim de todas as operações, liberta toda a lista em memória.

void show_to_eliminar(char *path, int client_fd)
{
    int i,op;
    ssize_t bytes_read = 0;

    msg_cliente *inicio = busca_do_ficheiro_msg(path);

    msg_cliente *atual = inicio;

    char buffer[BUF_SIZE];

    i=0;

    if (inicio == NULL){
        strcpy(buffer, "\n\nNao tem qualquer mensagem a listar\n\n");
        write(client_fd, buffer, 1 + strlen(buffer));
        return;
    }
    else
    {
        strcpy(buffer, "\n\n************************************\n      MENSAGENS PARA ELIMINAR        \n************************************\n\n");
        int j = strlen(buffer);

        while (atual != NULL)
        {
            i=i+1;
            sprintf(&buffer[j], "Num: %d\nRemetente: %s\nAssunto: %s\nVisualizada: %s\n\n",i,atual->transmissor,atual->assunto,(atual->vista ? "Verdadeiro" : "Falso"));
            j = strlen(buffer);
            atual = atual->next;
        }
        write(client_fd, buffer, 1 + strlen(buffer));
    }

    while(1){
        strcpy(buffer,"\nQual mensagem deseja eliminar? Indique o num (Se quiser sair coloque 0): ");
        write(client_fd,buffer, 1 + strlen(buffer));

        if ((bytes_read = read(client_fd, buffer, BUF_SIZE - 1)) == -1){
            perror("\n\nErro a ler a opção do cliente\n\n");
        }
        else{
            break;
        }
    }

    buffer[bytes_read] = '\0';

    op = strtol(buffer, NULL, 10);


    atual = inicio;

    if(op==0){
        return;
    }
    else{
        for (i = 1; atual != NULL; i = i + 1)
        {
            if ((i == op) && (atual->vista==true)){
                    sprintf(buffer, "\nA mensagem será eliminada\n");
                    write(client_fd, buffer, 1 + strlen(buffer));
                    elimina_msg_lidas(path,client_fd,atual,inicio); // ja guarda e elimina a lista da memoria dinamica
                    return;
            }
            if((i == op) && (atual->vista==false)){
                    sprintf(buffer, "\nA mensagem que tentou eliminar ainda nao foi vista\n");
                    write(client_fd, buffer, 1 + strlen(buffer));
                    liberta_lista_msg(inicio);
                    return;
            }
            atual = atual->next;
        }
        strcpy(buffer,"\nNao existe essa mensagem.\n");
        write(client_fd,buffer, 1 + strlen(buffer));
    }

    liberta_lista_msg(inicio);
}

// ***************************************************** FUNÇÃO ELIMINA_MSG **************************************************************
// Esta função vai eliminar um nó que se encontra na lista que está em memória. Esta nova atualização que for feita à lista, vai também
// ser automáticamente guardada no ficheiro correto. No fim de todas as operações, liberta toda a lista em memória.

void elimina_msg_lidas(char *path, int client_fd,msg_cliente *elimina, msg_cliente *inicio_lista)
{
    char buffer[BUF_SIZE];

    msg_cliente *atual = inicio_lista;
    msg_cliente *anterior = NULL;

    if (atual == NULL)
    {
        printf("\nA lista esta vazia!\n");

        strcpy(buffer, "Nao exite mensagens no seu ficheiro");
        write(client_fd, buffer, 1 + strlen(buffer));

        return;
    }
    else
    {
        while (atual != NULL)
        {
            if (atual==elimina)
            {
                if (anterior == NULL){
                    inicio_lista = atual->next;
                }
                else{
                    anterior->next = atual->next;
                }
                free(atual);

                if (!guarda_no_ficheiro_msg(path, inicio_lista))
                {
                    printf("Erro, nao foi possivel guardar a lateração depois de eliminar um dos seus nós.\n Prima uma tecla:");
                }
                liberta_lista_msg(inicio_lista);
                return;
            }
            anterior = atual;
            atual = atual->next;
        }
    }

    liberta_lista_msg(inicio_lista);
    return;
}

// ***************************************************** FUNÇÃO REMOVE_CHAR_FROM_STRING **************************************************************
// Esta função vai ajudar a eliminar os caracteres indesejados que são passados entre o cliente e o servidor.

void remove_char_from_string(char c, char *str)
{
    int i=0;
    int len = strlen(str)+1;

    for(i=0; i<len; i++)
    {
        if(str[i] == c)
        {
            strncpy(&str[i],&str[i+1],len-i);
        }
    }
}

// ************************************************************* FUNÇÃO SEC_TO_NORMAL ****************************************************************
// Esta função irá fazer print do tempo num formato mais conhecido, visto que apenas guardarmos a informação no formato de segundos.

void Sec_to_normal(time_t seconds,int client_fd)
{

    struct tm  ts;
    char buffer[80];

    // Get current time
    time(&seconds);
    ts = *localtime(&seconds);
    strftime(buffer, sizeof(buffer), "DATA DE ENVIO:  %a %Y-%m-%d %H:%M:%S\n\n", &ts);

    write(client_fd,buffer,1 + strlen(buffer));
}

// ************************************************************* FUNÇÃO ALTER_PASS ****************************************************************
// Esta função irá se só acessível aos que ja estão admitos pelo o administrador, como é obvio. Terão a possibilidade de alterar a password.
// Começamos por buscar o que esta guardado no ficheiro login, procuramos pelo o cliente atual para sabermos o no em que se encontra a informação
// dele. Assim podemos alterar a password, guardando de novo no ficheiro e libertando a memória dinâmica. Se a palavra pass nova for igual a antiga
// o progama apenas avisa, mas faz alteração na mesma, visto que logo a frente é pedido para confirmar. Apos fazer a alteração da password terá que iniciar
// novamente

void alter_pass(int client_fd, info_cliente *atual)
{
    char buffer[BUF_SIZE];
    ssize_t bytes_read = 0;

    info_cliente *inicio = busca_do_ficheiro_id("Raiz/Login/clientes.txt");
    info_cliente * percorre =inicio;

    if (inicio == NULL)
        return;
    else
    {
        while (atual != NULL)
        {

            if ((strcmp(atual->username, percorre->username) == 0) && (strcmp(atual->password, percorre->password) == 0)){
                break;    // descobriu o nó que contem a informação do nosso cliente
            }
            percorre = percorre->next;
        }
    }

    while(1){
        strcpy(buffer,"\n\nQual a nova password?  ");
        write(client_fd,buffer, 1 + strlen(buffer));

        if ((bytes_read = read(client_fd, buffer, BUF_SIZE - 1)) == -1){
            perror("\n\nErro a ler a opção do cliente\n\n");

            strcpy(buffer,"\n\nErro ao ler a nova password, introduza de novo. \n\n");
            write(client_fd,buffer, 1 + strlen(buffer));
        }
        else{
            break;
        }
    }

    buffer[bytes_read] = '\0';
    remove_char_from_string('\r',buffer);
    remove_char_from_string('\n',buffer);

    if(strcmp(buffer,percorre->password)==0){
        strcpy(buffer,"\n\nA nova palavra pass é igual a antiga\n\n");
        write(client_fd,buffer, 1 + strlen(buffer));
    }

    strcpy(percorre->password, buffer);
    strcpy(atual->password, buffer);

    while(1){
        strcpy(buffer,"\n\nTer a certeza que quer alterar? Escreva 1 para sim e 0 para não: ");
        write(client_fd,buffer, 1 + strlen(buffer));

        if ((bytes_read = read(client_fd, buffer, BUF_SIZE - 1)) == -1){
            perror("\n\nErro a ler a opção do cliente\n\n");

            strcpy(buffer,"\n\nErro ao ler ao ler a sua opção, introduza de novo. \n\n");
            write(client_fd,buffer, 1 + strlen(buffer));
        }
        else{
            break;
        }
    }

    int res = 0;
    res = strtol(buffer, NULL, 10);

    if (res==1)
    {
        if (!guarda_no_ficheiro("Raiz/Login/clientes.txt",inicio))
        {
            strcpy(buffer, "\n\nErro, nao foi guardar a nova password\n\n");
            write(client_fd, buffer, 1 + strlen(buffer));
        }
        liberta_lista(inicio);
        return;
    }
    else
    {
        strcpy(buffer,"\n\nA palavra pass nao foi alterada\n\n");
        write(client_fd,buffer, 1 + strlen(buffer));
        liberta_lista(inicio);
    }

}
