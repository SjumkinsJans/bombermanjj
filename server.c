#include "config.h"

#define SERVER_ID "127.0.0.1"

// create a socket via socket()
// bind the socket (associates the socket with a specific IP address and port number)
// Listen for connections via listen()
// accept a client connection via accept()
// send data to the client via send() 


int find_free_id();
void* process_client(void* arg);
void load_grid();

char* recv_hello(int clientSocket,uint8_t sender_id,uint8_t target_id);
void recv_leave(int clientSock,uint8_t sender_id,uint8_t target_id);
void recv_set_ready(int clientSock,uint8_t sender_id,uint8_t target_id);
uint16_t recv_move_attempt(uint8_t clientSocket,uint8_t client_id,uint8_t target_id);

void send_welcome(int clientSocket,uint8_t clientId,const char *senderName);
void send_disconnect(int clientSocket,uint8_t clientId);
void broadcast_send_set_status(uint8_t gm_status);
void broadcast_send_map(char *grid,uint8_t w,uint8_t h);
void broadcast_send_moved(uint8_t clientId, uint16_t new_pos);

void prepare_map(char *grid,uint8_t w,uint8_t h);

uint8_t game_status = GAME_LOBBY;
uint8_t clientCount = 0;
uint8_t players_ready = 0;
uint8_t freeClientIds[MAX_PLAYERS];

uint8_t y_border;
uint8_t x_border;
uint8_t global_speed;


uint8_t global_exp_danger;
uint8_t global_exp_r;
uint8_t global_exp_timer;

uint64_t last_movement_ms[MAX_PLAYERS];
player_t players[MAX_PLAYERS];

char *grid = NULL;

int client_sockets[MAX_PLAYERS];
pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
int main(void) {
    memset(freeClientIds, 0, sizeof(freeClientIds));
    memset(players, 0, sizeof(players));
    memset(last_movement_ms, 0, sizeof(last_movement_ms));
    memset(client_sockets, -1, sizeof(client_sockets));
    //load_grid();
    printf("It's a server !\n");
    int serverSockD = socket(AF_INET,SOCK_STREAM,0);
    // for(int i = 0;i < y_border;i++) {
    //     for(int j = 0;j < x_border;j++) {
    //         printf("%c",grid[i*x_border+j]);
    //     }
    //     printf("\n");
    // }

    //define server address
    struct sockaddr_in servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(9001);
    servAddr.sin_addr.s_addr = INADDR_ANY;

    //bind socket to the specified IP and port
    bind(serverSockD,(struct sockaddr*)&servAddr,sizeof(servAddr));

    //listen for connections
    listen(serverSockD,MAX_PLAYERS);
    //accepting loop
    while(1) {

        int clientSocket = accept(serverSockD,NULL,NULL);
        if(game_status != 0) {
            printf("Won't connect new player because game is running !\n");
            close(clientSocket);
        }

        pthread_mutex_lock(&server_mutex);
        uint8_t new_id = find_free_id();
        if(new_id == -1) {
            pthread_mutex_unlock(&server_mutex);
            printf("No free id ! :( \n");
            close(clientSocket);
        }
        freeClientIds[new_id] = 1;
        players[new_id].id = new_id;
        clientCount++;
        pthread_mutex_unlock(&server_mutex);
        
        pthread_mutex_lock(&server_mutex);
        client_sockets[new_id] = clientSocket;
        pthread_mutex_unlock(&server_mutex);

        // This line of comment stands as a memorial for fork(). Goodbye, fork(), you will be missed.
        // Create a thread for a new client
        pthread_t client_thread;
        pthread_create(&client_thread,NULL,process_client,(void*)(intptr_t)new_id);
        pthread_detach(client_thread); // When this thread exits, automatically free its resources
    }
    if (grid) free(grid);
    return 0;

} // end of main ================================================================================================================================================================================================================================

int find_free_id() {
    for(int i = 0;i < MAX_PLAYERS;i++) {
        if(freeClientIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

void load_grid() {
    srand(time(NULL));
    int x = rand()%3;
    char path[10] = "./grids/";
    path[8] = x+48;
    FILE* fin = fopen(path,"r");

    fscanf(fin, "%hhd %hhd %hhd %hhd %hhd %hhd",&y_border, &x_border,&global_speed, &global_exp_danger,&global_exp_r, &global_exp_timer);
    fgetc(fin); 
    pthread_mutex_lock(&server_mutex);
    if (grid) free(grid); // if it's not a first game, clear the previous map and reserve memory for a new one 
    grid = malloc(y_border * x_border);

    for(int i = 0;i < MAX_PLAYERS;i++) {
        if(client_sockets[i] == -1) {
            continue;
        }
        players[i].speed = global_speed;
        players[i].bomb_timer_ticks = global_exp_danger;
        players[i].bomb_count = global_exp_timer;
        players[i].bomb_radius = global_exp_r;
    }
    // I know I should read it into a buffer because reading from a file is a costly operation.
    // But last I tried it, I swear to good, it had such unforseen results that
    // I had to 
    char buffer[x_border + 2];   // +2 for newline and null terminator
    for (int row = 0; row < y_border; row++) {
        fgets(buffer, sizeof(buffer), fin);
        // Remove trailing newline (if present)
        buffer[strcspn(buffer, "\n")] = '\0';
        // Copy exactly x_border characters into the flat grid
        for (int col = 0; col < x_border; col++) {
            grid[row * x_border + col] = buffer[col];
        }
    }
    pthread_mutex_unlock(&server_mutex);
    fclose(fin);
}

void* process_client(void* arg) {
    //send's message to client socker
    uint8_t client_id = (uint8_t)(intptr_t)arg;
    int clientSocket = client_sockets[client_id];
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    while(1) {
        if (recv(clientSocket, &msg_type, 1, 0) <= 0) break;
        if (recv(clientSocket, &sender_id, 1, 0) <= 0) break;
        if (recv(clientSocket, &target_id, 1, 0) <= 0) break;
        //printf("Receiving msg type : %d\n",msg_type);

        switch (msg_type)
        {
        case MSG_HELLO:
        
            const char * name = recv_hello(clientSocket,client_id,target_id);
            send_welcome(clientSocket,client_id,name);
            break;
        
        case MSG_LEAVE:
            recv_leave(clientSocket,client_id,target_id);
            //usleep(200000);
            send_disconnect(clientSocket,client_id);
            close(clientSocket);
            break;

        case MSG_SET_READY:
            recv_set_ready(clientSocket,client_id,target_id);
            break;
        case MSG_MOVE_ATTEMPT:
            uint16_t new_pos = recv_move_attempt(clientSocket,client_id,target_id);
            if(new_pos == -1) {
                break;
            } 
            if(grid[new_pos] == '.')
                broadcast_send_moved(client_id,new_pos);
            break;
        default:
            break;
        }
    }
    close(client_sockets[client_id]);

    pthread_mutex_lock(&server_mutex);
    client_sockets[client_id] = -1;
    pthread_mutex_unlock(&server_mutex);

    pthread_mutex_lock(&server_mutex);
    freeClientIds[client_id] = 0;
    players[client_id].id = -1;
    clientCount--;
    pthread_mutex_unlock(&server_mutex);

    pthread_mutex_lock(&server_mutex);
    if(players[client_id].ready == 1) {
        players[client_id].ready = 0;
        players_ready--;
    }
    pthread_mutex_unlock(&server_mutex);
}

char* recv_hello(int clientSock,uint8_t sender_id,uint8_t target_id) {
    pthread_mutex_lock(&server_mutex);
    //printf("sizeof(msg_hello_t) = %zu\n", sizeof(msg_hello_t));

    char *client_id = malloc(20);
    client_id[19] = '\0';
    recv_all(clientSock,client_id,20,0);

    char *name = malloc(30);
    name[29] = '\0';
    recv_all(clientSock,name,30,0);
    players[sender_id].alive = 1;
    players[sender_id].lives = 1;
    players[sender_id].bomb_count = 1;
    strcpy(players[sender_id].name,name);
    printf("%s is with us !\n",players[sender_id].name);



    printf("%s has connected ! Version : %s\n",name,client_id);
    printf("Total clients connected : %d\n",clientCount);
    pthread_mutex_unlock(&server_mutex);
    return name;
}

void recv_leave(int clientSock,uint8_t sender_id,uint8_t target_id) {
    printf("Clients %d wants to leave :( \n",sender_id);
}

void recv_set_ready(int clientSock,uint8_t sender_id,uint8_t target_id) {
    pthread_mutex_lock(&server_mutex);
    if(players[sender_id].ready == 0 ) {
        players[sender_id].ready = 1;
        players_ready++;
        printf("Clients %d is ready ! ",sender_id);
    } else {
        players[sender_id].ready = 0;
        players_ready--;
        printf("Clients %d is ready ! ",sender_id);
    }
    printf("Total players ready : %d \n",players_ready);
    
    pthread_mutex_unlock(&server_mutex);
    
    if(players_ready > 1 && players_ready == clientCount) {
        printf("Visi ir gatavi ! Let's goooo ! \n");
        broadcast_send_set_status(GAME_RUNNING);
        load_grid();
        prepare_map((char*)grid,x_border,y_border);
        broadcast_send_map((char*)grid,x_border,y_border);
    }
    
}

uint16_t recv_move_attempt(uint8_t clientSocket,uint8_t client_id,uint8_t target_id) {
    uint8_t dir;
    int move;
    recv_all(clientSocket,&dir,1,0);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    pthread_mutex_lock(&server_mutex);
    if(now-last_movement_ms[client_id] < 1000/players[client_id].speed) {
        pthread_mutex_unlock(&server_mutex);
        return -1;
    }
    last_movement_ms[client_id] = now;
    pthread_mutex_unlock(&server_mutex);
    switch (dir) {
        case 0:
            move = -x_border;
            break;
        case 1:
            move = +x_border;
            break;
        case 2:
            move = -1;
            break;
        case 3:
            move = +1;
            break;
    }
    //printf("%s want's to move to %d ! \n",players[client_id].name,players[client_id].pos+move);
    return players[client_id].pos+move;
}

void send_welcome(int clientSocket,uint8_t clientId,const char *senderName) {
    msg_welcome_t header;
    header.msg_type = MSG_WELCOME;
    header.sender_id = 255;
    header.target_id = clientId;
    header.game_status = GAME_LOBBY;

    send(clientSocket,&header.msg_type,1,0);
    send(clientSocket,&header.sender_id,1,0);
    send(clientSocket,&header.target_id,1,0);
    send(clientSocket,SERVER_ID,20,0);
    send(clientSocket,&header.game_status,1,0);
}

void send_disconnect(int clientSocket,uint8_t clientId) {
    msg_disconnect_t header;
    header.msg_type = MSG_DISCONNECT;
    header.sender_id = 255;
    header.target_id = clientId;

    send(clientSocket,&header.msg_type,1,0);
    send(clientSocket,&header.sender_id,1,0);
    send(clientSocket,&header.target_id,1,0);
}

void broadcast_send_set_status(uint8_t gm_status) {
    pthread_mutex_lock(&server_mutex);
    for(int i = 0;i < MAX_PLAYERS;i++) {
        if(client_sockets[i] != -1) {
            msg_disconnect_t header;
            header.msg_type = MSG_SET_STATUS;
            header.sender_id = 254;
            header.target_id = i;

            send(client_sockets[i],&header.msg_type,1,0);
            send(client_sockets[i],&header.sender_id,1,0);
            send(client_sockets[i],&header.target_id,1,0);
            send(client_sockets[i],&gm_status,1,0);
        }
    }
    game_status = gm_status;
    pthread_mutex_unlock(&server_mutex);
}

void prepare_map(char *grid,uint8_t w,uint8_t h) {
    pthread_mutex_lock(&server_mutex);
    for(int y = 0;y < h;y++) {
        for(int x = 0;x < w;x++) {
            int pos = y*w+x;
            if(grid[pos] >=48 && grid[pos] < 57) {
                int guy = grid[pos]-48; 
                if(client_sockets[guy] != -1) {
                    printf("There is player %d row %d col %d\n",guy,y,x);
                    players[guy].pos = y*x_border+x;
                    continue;
                } else {
                    grid[pos] = '.';
                }
            }
        }
    }
    pthread_mutex_unlock(&server_mutex);
}

void broadcast_send_map(char *grid,uint8_t w,uint8_t h) {
    printf("Sending map !\n");
    pthread_mutex_lock(&server_mutex);
    for(int i = 0;i < MAX_PLAYERS;i++) {
        if(client_sockets[i] != -1) {
            msg_map_t header;
            header.msg_type = MSG_MAP;
            header.sender_id = 254;
            header.target_id = i;
            header.w = w;
            header.h = h;

            send(client_sockets[i],&header.msg_type,1,0);
            send(client_sockets[i],&header.sender_id,1,0);
            send(client_sockets[i],&header.target_id,1,0);
            send(client_sockets[i],&header.w,1,0);
            send(client_sockets[i],&header.h,1,0);
            send(client_sockets[i],grid,w*h,0);
        }
    }
    pthread_mutex_unlock(&server_mutex);
}

void broadcast_send_moved(uint8_t clientId, uint16_t new_pos) {
    pthread_mutex_lock(&server_mutex);
    uint16_t old_posi = players[clientId].pos;
    grid[old_posi] = '.';
    grid[new_pos] = clientId+48;
    pthread_mutex_unlock(&server_mutex);
    players[clientId].pos = new_pos;
    for(int i = 0; i < MAX_PLAYERS;i++) {
        if(client_sockets[i] == -1) {
            continue;
        }
        msg_moved_t header;
        header.msg_type = MSG_MOVED;
        header.sender_id = 254;
        header.target_id = i;

        header.client_id = clientId;
        header.old_pos = old_posi;
        header.new_pos = players[clientId].pos;
        
        
        send(client_sockets[i],&header.msg_type,1,0);
        send(client_sockets[i],&header.sender_id,1,0);
        send(client_sockets[i],&header.target_id,1,0);

        send(client_sockets[i],&header.client_id,1,0);
        send(client_sockets[i],&header.old_pos,sizeof(uint16_t),0);
        send(client_sockets[i],&header.new_pos,sizeof(uint16_t),0);
    }
    
}