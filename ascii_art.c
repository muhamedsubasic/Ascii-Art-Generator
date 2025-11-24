/******
*   ASCII ART RANDOM GENERATOR
*   Shared memory usage and multi process
********/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* Adjustable canvas size with max amount of workers */
#define ART_WIDTH 50
#define ART_HEIGHT 15
#define MAX_WORKERS 2

typedef struct {
    char canvas[ART_HEIGHT][ART_WIDTH]; // Creates an array for canvas
} shared_data;
shared_data *shared_memory; // Pointer that points into a shared memory where main canvas is located

/*********
* Canvas Functions
**********/

/* Canvas filled with empty spaces that will be replaced by workers */
void clear_canvas() {
    for (int y = 0; y < ART_HEIGHT; y++) {
        for (int x = 0; x < ART_WIDTH; x++) {
            shared_memory->canvas[y][x] = ' '; 
        }
    }
}

/* Prints each time canvas is changed (main functionality) */
void print_canvas() {
    printf("\n====== ASCII ART ======\n");
    for (int y = 0; y < ART_HEIGHT; y++) {
        for (int x = 0; x < ART_WIDTH; x++) {
            putchar(shared_memory->canvas[y][x]); // Prints character
        }
        putchar('\n');
    }
    printf("======================\n\n");
}

void draw_random_shape(int worker_id) {
    char colors[] = {'@', ')', '*', '+', '.', '$', '(', '0', '&', '%'};
    /* Random number generator unique to each worker, properly following time between executions to avoid duplicate colors */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand((time(NULL) ^ (worker_id << 16)) + ts.tv_nsec);
    
    int center_x = rand() % ART_WIDTH;  // Random X position
    int center_y = rand() % ART_HEIGHT; // Random Y position
    int radius = 2 + rand() % 4;        // Random radius
    char color = colors[rand() % (sizeof(colors)/sizeof(char))];
    
    for (int y = 0; y < ART_HEIGHT; y++) {
        for (int x = 0; x < ART_WIDTH; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            if (dx*dx + dy*dy <= radius*radius) { // Checks if we are within lines of radius
                shared_memory->canvas[y][x] = color; //Sets color on the shared memory aka canvas
            }
        }
    }
    
    printf("Painter %d drew %c at (%d,%d)\n", worker_id, color, center_x, center_y);
}

/*******
* Worker proces
*******/
void worker_process(int worker_id, int timeout) {
    time_t start = time(NULL); // Start time
    
    // Creates a child process using fork() for a sub-task
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child process starts drawing
        char *argv[] = {"/bin/echo", "Painter subprocess drawing...", NULL};
        execv("/bin/echo", argv);
        perror("execv failed");
        exit(1);
    } else if (child_pid > 0) {
        // Parent process continues with drawing
        while (time(NULL) - start < timeout) {
            draw_random_shape(worker_id); 
            
            // Randomly decide whether to kill the child process (approx 20%)
            if (rand() % 5 == 0 && child_pid > 0) {
                printf("Painter %d killing its subprocess %d\n", worker_id, child_pid);
                kill(child_pid, SIGTERM);  // Kills child and sends termination signal
                child_pid = 0;             // Marks as killed
            }
            
            sleep(1 + rand() % 2);
        }
        
        // Wait for child process if it's still alive
        if (child_pid > 0) {
            wait(NULL);
        }
    } else {
        perror("fork failed");
    }
    exit(0);
}

int main() {
    srand(time(NULL)); // Random seed number generator
    
    /* Creates shared memory */
    // IPC_PRIVATE = new segment, 0666 = read/write permissions
    int shmid = shmget(IPC_PRIVATE, sizeof(shared_data), IPC_CREAT | 0666);
    shared_memory = (shared_data *)shmat(shmid, NULL, 0); // Shared memory assigned
    
    // Initialization of canvas (blank canvas)
    clear_canvas();
    print_canvas();
    
    printf("====== ASCII Art Generator ======\n");
    
    pid_t workers[MAX_WORKERS]; // Stores workers PIDs
    
    /* Launches workers using fork() */
    for (int i = 0; i < MAX_WORKERS; i++) {
        pid_t pid = fork();                               // New process created
        if (pid == 0) {                                   // Child becomes worker
            worker_process(i, 8);                         // 8 seconds run of process
        } else if (pid > 0) {     
            workers[i] = pid;                             // Parent worker PID is stored
            printf("Launched painter %d (PID: %d)\n", i, pid);
            usleep(100000);                               // 100ms delay between forks
        } else {
            perror("fork failed");
            exit(1);
        }
    }
    
    // Update display 5 times
    for (int i = 0; i < 5; i++) {
        sleep(3);
        print_canvas();
        
        // Randomly kill a worker process on 3rd update
        if (i == 2) {
            int worker_to_kill = rand() % MAX_WORKERS;
            printf("Main process killing painter %d (PID: %d)\n", 
                   worker_to_kill, workers[worker_to_kill]);
            kill(workers[worker_to_kill], SIGTERM);
        }
    }
    
    // Wait for workers using wait() function implementation
    for (int i = 0; i < MAX_WORKERS; i++) {
        int status;
        pid_t terminated_pid = wait(&status);
        printf("Painter PID %d terminated with status %d\n", terminated_pid, status);
    }
    
    printf("\n== Final Artwork ==\n");
    print_canvas();
    
    // Clean up memory
    shmdt(shared_memory);             // Detach from shared memory
    shmctl(shmid, IPC_RMID, NULL);    // Mark segment for destruction by system
    
    return 0;
}
