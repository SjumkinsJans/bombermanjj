#include "config.h"

char **grid = NULL;
int y_border;
int x_border;
uint16_t player_base_speed; // blocks/ticks
uint16_t exp_dur; //explosion duration (ticks)
uint8_t exp_dist; //explosion distantce (ticks) 
uint16_t exp_timer; // (ticks)
int get_grid();

void get_shared_memory();
void start_network();
void gameloop();
void process_client();


int main(void) {
    int pid = 0;
    printf("SERVER started !\n");
    if(get_grid() == -1) {
        return -1;
    }
    get_shared_memory();
    /*
    fork to have two processes -
    networking (child here) and
    game loop (parent here)
    */
    pid = fork();
    if(pid == 0) {

    } else {
        gameloop();
    }
    return 0;
}

//ielasam *izvēlēto* spēles režģi
int get_grid() {
    FILE *fin =  fopen("grid1","r");
    if(fin == NULL) {
        printf("File doesn't exist ! \n");
        return -1;
    }

    fscanf(fin,"%d %d %hd %hd %hhd %hd",
            &y_border,
            &x_border,
            &player_base_speed,
            &exp_dur,
            &exp_dist,
            &exp_timer);

    // for debug
    // printf("%d %d %hd %hd %hhd %hd\n",            
    //         y_border,
    //         x_border,
    //         player_base_speed,
    //         exp_dur,
    //         exp_dist,
    //         exp_timer);

    grid = malloc(y_border*sizeof(char*));
    fgetc(fin);
    int row = 0;
    int buffer[x_border];
    for(row;row < y_border;row++) {
        grid[row] = malloc(x_border*sizeof(char));
        fread(grid[row],1,x_border,fin);
        fgetc(fin);

        //for debug
        // for(int i = 0;i < x_border;i++) {
        //     printf("%c",grid[row][i]);
        // }
        // printf("\n");
    }

    fclose(fin);
    printf("Grid loaded successfully !\n");
    return 0;
}

void get_shared_memory() {

}

void gameloop() {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);
    
    //render loop
    int input;
    int xpos = 1;
    int ypos = 1;
    int play = 1;
    while(play) {
        clear();
        for(int x = 0;x < x_border;x++) {
            for(int y = 0; y < y_border;y++) {
                mvprintw(y,x,"%c",grid[y][x]);
            }
        }
        mvprintw(ypos,xpos,"1");


        //inputs
        int dx = 0;
        int dy = 0;
        input = getch();
        switch(input) {
            case KEY_UP:
                dy = -1; break;
            case KEY_DOWN:
                dy = 1; break;
            case KEY_RIGHT:
                dx = 1; break;
            case KEY_LEFT:
                dx = -1; break;    
            case 27: // Escape
                play = 0; break;
            case ' ':
                grid[ypos][xpos] = '@'; break; // @ - bumba
        }
        
        if(grid[ypos][xpos+dx] == '.') {
            xpos+=dx;
        }
        if(grid[ypos+dy][xpos] == '.') {
            ypos+=dy;
        }
        
        refresh();
        usleep(50000); 
        // specifikācijā rakstīts, ka spēles stāvoklis tiek atjaunināts 20 reizes sekundē(ticks).
        // usleep prasa laiku mikrosekundēs(1*10^-6);
        // sanāk, ka 50000 mikrosekundes = 0.05 sekundes = 20 reizes;
        // godīgi, gnjau šo vajadzēs pamainīt, bet pagaidām derēs  
    }
    endwin();
}

void start_network() {

}

void process_client() {

}