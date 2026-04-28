#include "config.h"

// create a socket via socket()
// connect to the server via connect()
// receive data via recv()
// display the message

void gameloop();
// sending
void send_hello(int sock,int sender_id,const char* clied_id,const char* player_name);
void send_leave();
void send_set_ready();
void send_move_attempt(int sock,uint8_t dir);
void send_bomb_attempt(int sock);

// receiving
void* recv_msgs();
void recv_welcome();
void recv_disconnect();
void recv_set_status(int sockD);
void recv_map(int sockD);
void recv_moved(int sockD);
void recv_explosion_start(int sock);
void recv_explosion_end(int sock);
void recv_death(int sock);

uint8_t *grid = NULL;
uint8_t player_id = 0;
uint8_t game_status = 0;
uint8_t x_border = 0;
uint8_t y_border = 0;
uint8_t map_received = 0;
uint8_t quit_server = 0;
uint8_t alive = 1;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc,char *argv[]) {
    printf("It's a client !\n");
    int sockD = socket(AF_INET, SOCK_STREAM, 0); // create a socket(communication endpoint)
    struct sockaddr_in servAddr;

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(9001); //use some unused port
    servAddr.sin_addr.s_addr = INADDR_ANY;

    int connectStatus = connect(sockD, (struct sockaddr*)&servAddr,sizeof(servAddr));

    if(connectStatus == -1) {
        printf("Error connection to the server !\n");
    } else {
        send_hello(sockD,1,argv[1],argv[2]);
        // Fork ? Oh, I know about Fork.
        // It was useful. It did well. But then came the need for broadcasts, for shared memory...
        // And it could handle it, but it was hard. For Fork, for Jans...
        // And so Fork had to go...
        // Pthread took Fork's place
        // And it did well, and Jans was happy, but he still couldn't forget Fork...
        // Jans still sometimes thinks : "Maybe I could've saved Fork ?", but he knows that he couldn't, because...
        // Because Jans has skill issue

        pthread_t recv_msgs_thr;        
        pthread_create(&recv_msgs_thr,NULL,recv_msgs,(void*)(intptr_t)sockD);
        pthread_detach(recv_msgs_thr);
        
        gameloop(sockD); // let it run on main thread so that code would be running
        
    }

    if(grid != NULL) {
        free(grid);
    }
    return 0;
}

void gameloop(int sockD) {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    timeout(50); // 50 milisecs
    noecho();
    curs_set(0);
    int input;
    while(1) {
        clear();

        // Locking mutex for the whole duration of drawing a map isn't really cool. Would be good to change it later for something more civil
        pthread_mutex_lock(&client_mutex);
        if(quit_server) {
            clear();
            pthread_mutex_unlock(&client_mutex);
            endwin();
            return;
        }
        if(map_received == 1) {
            // mvprintw(0,0,"%s","Well, we should be printing\n");
            for(int y = 0;y < y_border;y++) {
                for(int x = 0;x < x_border;x++) {
                     mvprintw(y,x,"%c",grid[y*x_border+x]);
                }
            }
        }
        pthread_mutex_unlock(&client_mutex);

        refresh();
        input = getch();
        if(input == ERR) continue;
        
        switch(input) {
            case KEY_UP:
                if (alive)
                    send_move_attempt(sockD,DIR_UP);
                break;
            case KEY_DOWN:
                if (alive)
                    send_move_attempt(sockD,DIR_DOWN);
                break;
            case KEY_RIGHT:
                if (alive)
                    send_move_attempt(sockD,DIR_RIGHT);
                break;
            case KEY_LEFT:
                if (alive)
                    send_move_attempt(sockD,DIR_LEFT);
                break;    
            case 27: // Escape
                send_leave(sockD);
                break;
            case ' ':
                if (alive)
                    send_bomb_attempt(sockD);
                break;
            case 'r':
                if(game_status == 0)
                    send_set_ready(sockD);
                break;
        }
        
    }
    endwin();
}

void send_hello(int sock,int sender_id,const char* clied_id,const char* player_name) {
    // message header
    msg_hello_t header;

    // configure message header
    header.msg_type = MSG_HELLO;
    header.sender_id = sender_id;
    header.target_id = 255;

    // Find len

    //send header
    send(sock,&header.msg_type,1,0);
    send(sock,&header.sender_id,1,0);
    send(sock,&header.target_id,1,0);

    //send payload
    send(sock,clied_id,20,0);
    send(sock,player_name,30,0);
}

void send_leave(int sock) {
    msg_leave_t header;
    header.msg_type = MSG_LEAVE;
    pthread_mutex_lock(&client_mutex);
    header.sender_id = player_id;
    pthread_mutex_unlock(&client_mutex);
    header.target_id = 255;

    send(sock,&header.msg_type,1,0);
    send(sock,&header.sender_id,1,0);
    send(sock,&header.target_id,1,0);
}

void send_set_ready(int sock) {
    msg_set_ready_t header;
    header.msg_type = MSG_SET_READY;
    pthread_mutex_lock(&client_mutex);
    header.sender_id = player_id;
    pthread_mutex_unlock(&client_mutex);
    header.target_id = 255;

    send(sock,&header.msg_type,1,0);
    send(sock,&header.sender_id,1,0);
    send(sock,&header.target_id,1,0);
}

void send_move_attempt(int sock,uint8_t dir) {
    msg_move_attempt_t header;
    header.msg_type = MSG_MOVE_ATTEMPT;
    pthread_mutex_lock(&client_mutex);
    header.sender_id = player_id;
    pthread_mutex_unlock(&client_mutex);
    header.target_id = 255;
    header.dir = dir;

    send(sock,&header.msg_type,1,0);
    send(sock,&header.sender_id,1,0);
    send(sock,&header.target_id,1,0);
    send(sock,&header.dir,1,0);
}

void send_bomb_attempt(int sock) {
    msg_bomb_attempt_t msg;
    msg.msg_type = MSG_BOMB_ATTEMPT;
    pthread_mutex_lock(&client_mutex);
    msg.sender_id = player_id;
    pthread_mutex_unlock(&client_mutex);
    msg.target_id = 255;

    send(sock, &msg.msg_type, 1, 0);
    send(sock, &msg.sender_id, 1, 0);
    send(sock, &msg.target_id, 1, 0);
}

void recv_bomb(int sock) {
    uint8_t sender_id, target_id, player_id_bomb;
    recv_all(sock, &sender_id, 1, 0);
    recv_all(sock, &target_id, 1, 0);
    recv_all(sock, &player_id_bomb, 1, 0);

    uint16_t pos;
    recv_all(sock, &pos, sizeof(uint16_t), 0);
    pos = ntohs(pos);

    pthread_mutex_lock(&client_mutex);
    if (grid != NULL) {
        grid[pos] = '@';
    }
    pthread_mutex_unlock(&client_mutex);
}


void* recv_msgs(void *arg) {
            int sockD = (int)(intptr_t)arg;
            uint8_t msg_type;
            while(1) {
                recv(sockD,&msg_type,1,0);
                //printf("Receiving msg type : %d",msg_type);
                switch (msg_type)
                {
                case MSG_WELCOME:
                    recv_welcome(sockD);
                    //usleep(2000000);
                    //send_leave(sockD);
                    break;
                case MSG_DISCONNECT:
                    recv_disconnect(sockD);
                    //usleep(200000);
                    close(sockD);
                    pthread_mutex_lock(&client_mutex);
                    quit_server = 1; 
                    pthread_mutex_unlock(&client_mutex);
                    return NULL;
                case MSG_SET_STATUS:
                    recv_set_status(sockD);
                    //usleep(200000);
                    break;
                case MSG_MAP:
                    recv_map(sockD);
                    break;
                case MSG_MOVED:
                    recv_moved(sockD);
                    break;
                case MSG_EXPLOSION_START:
                    recv_explosion_start(sockD);
                    break;
                case MSG_EXPLOSION_END:
                    recv_explosion_end(sockD);
                    break;
                case MSG_DEATH:
                    recv_death(sockD);
                    break;
                case MSG_BOMB:
                    recv_bomb(sockD);
                    break;
                default:
                    break;
                }
            }
}

void recv_welcome(int sock) {
    uint8_t sender_id;
    recv_all(sock,&sender_id,1,0);

    pthread_mutex_lock(&client_mutex);
    recv_all(sock,&player_id,1,0);
    printf("Your player id is : %d \n",player_id);
    pthread_mutex_unlock(&client_mutex);
    
    char msg[20];
    recv_all(sock,msg,20,0);
    
    pthread_mutex_lock(&client_mutex);
    recv_all(sock,&game_status,1,0);
    pthread_mutex_unlock(&client_mutex);
    printf("Game status is : %d \n",game_status);
    printf("%s\n",msg);
}

void recv_disconnect(int sock) {
    uint8_t sender_id;
    recv_all(sock,&sender_id,sizeof(sender_id),0);

    uint8_t target_id;
    recv_all(sock,&target_id,sizeof(target_id),0);
    printf("Player %d you're being disconnected ! >:) \n",target_id);
}

void recv_set_status(int sock) {
    uint8_t sender_id;
    recv_all(sock,&sender_id,sizeof(sender_id),0);

    uint8_t target_id;
    recv_all(sock,&target_id,sizeof(target_id),0);

    pthread_mutex_lock(&client_mutex);
    recv_all(sock,&game_status,1,0);
    pthread_mutex_unlock(&client_mutex);
    printf("New game status : %d\n",game_status);
}

void recv_map(int sock) {
    printf("Receiving map !\n");
    uint8_t sender_id;
    recv_all(sock,&sender_id,sizeof(sender_id),0);

    uint8_t target_id;
    recv_all(sock,&target_id,sizeof(target_id),0);

    pthread_mutex_lock(&client_mutex);
    recv_all(sock,&x_border,1,0);
    recv_all(sock,&y_border,1,0);


    if(grid != NULL) {
        free(grid);
    }
    grid = malloc(sizeof(char)*x_border*y_border);
    for(int y = 0;y < y_border;y++) {
        for(int x = 0;x < x_border;x++) {
            recv_all(sock,&grid[y*x_border+x],1,0);
            //printf("%c",grid[y*(*y_border)+x]);
        }
        //printf("\n");
    }
    map_received = 1;
    pthread_mutex_unlock(&client_mutex);
}

void recv_moved(int sock) {
    pthread_mutex_lock(&client_mutex);
    uint8_t sender_id;
    recv_all(sock,&sender_id,sizeof(sender_id),0);

    uint8_t target_id;
    recv_all(sock,&target_id,sizeof(target_id),0);

    uint8_t moved_id;
    recv_all(sock,&moved_id,sizeof(moved_id),0);

    uint16_t old_pos;
    recv_all(sock,&old_pos,sizeof(old_pos),0);

    uint16_t new_pos;
    recv_all(sock,&new_pos,sizeof(new_pos),0);
    if(grid[old_pos] != '@') {
        grid[old_pos] = '.';
    }

    grid[new_pos] = moved_id + 48;
    pthread_mutex_unlock(&client_mutex);
}

void recv_explosion_start(int sock) {
    uint8_t sender_id, target_id, radius;
    recv_all(sock, &sender_id, 1, 0);
    recv_all(sock, &target_id, 1, 0);
    recv_all(sock, &radius, 1, 0);

    uint16_t pos;
    recv_all(sock, &pos, sizeof(uint16_t), 0);
    pos = ntohs(pos);

    pthread_mutex_lock(&client_mutex);
    if (grid != NULL) grid[pos] = '*';
    pthread_mutex_unlock(&client_mutex);
}

void recv_explosion_end(int sock) {
    uint8_t sender_id, target_id, radius;
    recv_all(sock, &sender_id, 1, 0);
    recv_all(sock, &target_id, 1, 0);
    recv_all(sock, &radius, 1, 0);

    uint16_t pos;
    recv_all(sock, &pos, sizeof(uint16_t), 0);
    pos = ntohs(pos);

    pthread_mutex_lock(&client_mutex);
    if (grid != NULL) grid[pos] = '.';
    pthread_mutex_unlock(&client_mutex);
}

void recv_death(int sock) {
    uint8_t sender_id, target_id, dead_id;
    recv_all(sock, &sender_id, 1, 0);
    recv_all(sock, &target_id, 1, 0);
    recv_all(sock, &dead_id, 1, 0);

    pthread_mutex_lock(&client_mutex);
    if (dead_id == player_id) {
        alive = 0;
    }
    pthread_mutex_unlock(&client_mutex);
}