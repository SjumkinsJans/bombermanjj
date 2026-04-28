#include <netinet/in.h> //structure for storing address information
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> //for socket APIs
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <sys/prctl.h>   // for prctl
#include <signal.h>      // for SIGTERM
#include <pthread.h>


#define MAX_PLAYERS 8
#define TICKS_PER_SECOND 20
#define MAX_NAME_LEN 15

typedef enum {
    GAME_LOBBY = 0,
    GAME_RUNNING = 1,
    GAME_END = 2
} game_status_t;

typedef enum {
    DIR_UP = 0,
    DIR_DOWN = 1,
    DIR_LEFT = 2,
    DIR_RIGHT = 3
} direction_t;

typedef enum {
    BONUS_NONE = 0,
    BONUS_SPEED = 1,
    BONUS_RADIUS = 2,
    BONUS_TIMER = 3
} bonus_type_t;

typedef enum {
    MSG_HELLO = 0,
    MSG_WELCOME = 1,
    MSG_DISCONNECT = 2,
    MSG_PING = 3,
    MSG_PONG = 4,
    MSG_LEAVE = 5,
    MSG_ERROR = 6,
    MSG_MAP = 7,
    MSG_SET_READY = 10,
    MSG_SET_STATUS = 20,
    MSG_WINNER = 23,
    MSG_MOVE_ATTEMPT = 30,
    MSG_BOMB_ATTEMPT = 31,
    MSG_MOVED = 40,
    MSG_BOMB = 41,
    MSG_EXPLOSION_START = 42,
    MSG_EXPLOSION_END = 43,
    MSG_DEATH = 44,
    MSG_BONUS_AVAILABLE = 45,
    MSG_BONUS_RETRIEVED = 46,
    MSG_BLOCK_DESTROYED = 47
} msg_type_t;

typedef struct {
    uint8_t id;
    uint8_t lives; // 0 nozīme beigts
    char name[MAX_NAME_LEN + 1];
    uint16_t pos;
    uint8_t alive;
    uint8_t ready;

    uint8_t bomb_count;
    uint8_t bomb_radius;
    uint16_t bomb_timer_ticks;
    uint16_t speed;
} player_t;

typedef struct {
    uint8_t owner_id;
    uint16_t row;
    uint16_t col;
    uint8_t radius;
    uint16_t timer_ticks;
} bomb_t;

static inline uint16_t make_cell_index(uint16_t row, uint16_t col, uint16_t cols) {
    return (uint16_t)(row * cols + col);
}

static inline void split_cell_index(uint16_t index, uint16_t cols, uint16_t *row, uint16_t *col) {
    *row = (uint16_t)(index / cols);
    *col = (uint16_t)(index % cols);
}





typedef struct {
    uint8_t msg_type;  // 1 byte
    uint8_t sender_id; // 1 byte
    uint8_t target_id; // 1 byte
    char client_id[20]; // 20 byte
    char client_name[30]; // 30 byte
} msg_hello_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    char server_id[20];
    uint8_t game_status;
} msg_welcome_t;

typedef struct {
    uint8_t msg_type; 
    uint8_t sender_id;
    uint8_t target_id;
} msg_leave_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
} msg_disconnect_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
} msg_set_ready_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t game_status;
} msg_set_status_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t w;
    uint8_t h;
} msg_map_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t dir;
} msg_move_attempt_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id; // 254 because broadcast
    uint8_t target_id;
    
    uint8_t client_id;
    uint16_t old_pos;
    uint16_t new_pos;
} msg_moved_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
} msg_bomb_attempt_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t player_id;
    uint16_t pos;
} msg_bomb_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t radius;
    uint16_t pos;
} msg_explosion_start_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t radius;
    uint16_t pos;
} msg_explosion_end_t;

typedef struct {
    uint8_t msg_type;
    uint8_t sender_id;
    uint8_t target_id;
    uint8_t player_id;
} msg_death_t;

typedef struct {
    uint16_t pos;
    uint16_t timer;
} explosion_t;

void recv_all(int socket,void* buffer,ssize_t expected,int flags) {
    //printf("Receiving bytes !\n");
    ssize_t bytes_total = 0;
    ssize_t bytes_read;
    while(bytes_total < expected) {
        bytes_read = recv(socket,(char*)buffer+bytes_total,expected-bytes_total,0);
        bytes_total += bytes_read;
        //printf("Receiving bytes ! %ld %ld %ld\n",bytes_read,bytes_total,expected);
    }
    //printf("Received all bytes ! %ld %ld\n",bytes_total,expected);
}