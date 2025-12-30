/* 
=========================================================================
Smart Home OS Hub Simulation
Semester Project – Operating Systems (40 Marks)
========================================================================= 
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// Macros
#define DEVICE_COUNT 15
#define SENSOR_COUNT 6
#define RESOURCE_COUNT 6
#define WAIT_THRESHOLD 3 // seconds

// ---------------- ANSI COLOR CODES ----------------
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BRIGHT_RED     "\033[91m"
#define BRIGHT_GREEN   "\033[92m"
#define BRIGHT_YELLOW  "\033[93m"
#define BRIGHT_BLUE    "\033[94m"
#define BRIGHT_MAGENTA "\033[95m"
#define BRIGHT_CYAN    "\033[96m"
#define BRIGHT_WHITE   "\033[97m"
#define BOLD    "\033[1m"

#define READY 0
#define RUNNING 1
#define WAITING 2
#define TERMINATED 3

// ---------------- SHARED RESOURCES ----------------
pthread_mutex_t resources[RESOURCE_COUNT]; // global array of mutex lock
char* resource_names[RESOURCE_COUNT] = {"WiFi","Power","Memory","Battery","Storage","CPU"};
int resource_usage[RESOURCE_COUNT] = {0};

// ---------------- TIME ----------------
char* current_time() {
    static char buf[20];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    return buf;
}

// ---------------- DEVICE STRUCT ----------------
typedef struct {
    char name[50];
    int priority;
    int state;
    int depends_on; // -1 if none
    int sensor_data[SENSOR_COUNT]; 
    int pipe_fd[2]; // pipe: [0]=read, [1]=write
} Device;

// ---------------- SENSOR THREAD ARG ----------------
typedef struct {
    Device* device;
    char name[100];
    int sensor_index;
} SensorArg;

// ---------------- SENSOR DATA ----------------
int generate_sensor_data(int index){
    switch(index){
        case 0: return 18 + rand()%13;
        case 1: return 20 + rand()%61;
        case 2: return rand()%101;
        case 3: return rand()%2;
        case 4: return rand()%201;
        case 5: return rand()%100;
        default: return rand()%100;
    }
}

// ---------------- SENSOR THREAD ----------------
void* sensor_thread(void* arg){
    SensorArg* s = (SensorArg*)arg;
    Device* d = s->device;

    printf(CYAN "[%s] " WHITE "[THREAD] " BRIGHT_CYAN "%-35s " RESET "| " YELLOW "Requesting resources" RESET "                    \n", current_time(), s->name);

    int locked[RESOURCE_COUNT] = {0};
    int acquired = 0;
    time_t start_wait = time(NULL);

    while(!acquired){
        acquired = 1;
        for(int i=0;i<RESOURCE_COUNT;i++){
            if(pthread_mutex_trylock(&resources[i])==0){
                locked[i]=1;
                resource_usage[i]++;
            } else {
                acquired = 0;
            }
        }
        if(!acquired){
            for(int i=0;i<RESOURCE_COUNT;i++)
                if(locked[i]) { pthread_mutex_unlock(&resources[i]); locked[i]=0; }
            d->state = WAITING;
            printf(CYAN "[%s] " WHITE "[THREAD] " BRIGHT_CYAN "%-35s " RESET "| " BRIGHT_RED "WAITING (Resources Busy)" RESET "                  \n", current_time(), s->name);
            sleep(0.5);
            if(time(NULL)-start_wait >= WAIT_THRESHOLD)
                printf(BRIGHT_RED "[%s] [ALERT] " BOLD "%-35s " RESET BRIGHT_RED "| " BOLD "Waiting >%d sec!" RESET BRIGHT_RED "                       " RESET "\n", current_time(), s->name, WAIT_THRESHOLD);
        }
    }

    d->state = RUNNING;
    int data = generate_sensor_data(s->sensor_index);
    d->sensor_data[s->sensor_index] = data;

    // ---------------- SEND DATA TO HUB ----------------
    write(d->pipe_fd[1], &data, sizeof(int));
    printf(CYAN "[%s] " WHITE "[THREAD] " BRIGHT_CYAN "%-35s " RESET "| " BRIGHT_GREEN "Resources Allocated" RESET " | " YELLOW "Sensor Data: " BRIGHT_YELLOW "%-3d" RESET "   \n", current_time(), s->name, data);
    sleep(1);

    for(int i=0;i<RESOURCE_COUNT;i++)
        if(locked[i]) pthread_mutex_unlock(&resources[i]);

    printf(CYAN "[%s] " WHITE "[THREAD] " BRIGHT_CYAN "%-35s " RESET "| " GREEN "Resources Released" RESET "                             \n", current_time(), s->name);
    free(s);
    pthread_exit(NULL);
}

// ---------------- SORT DEVICES BY PRIORITY ----------------
void sort_devices_by_priority(Device devices[]){
    for(int i = 0; i < DEVICE_COUNT; i++){
        for(int j = i + 1; j < DEVICE_COUNT; j++){
            if(devices[i].priority > devices[j].priority){
                Device temp = devices[i];
                devices[i] = devices[j];
                devices[j] = temp;
            }
        }
    }
}

// ---------------- DEVICE FUNCTION ----------------
void run_device(Device* d, Device devices[]){
    if(d->depends_on!=-1){
        printf(CYAN "[%s] " WHITE "[PROCESS] " BRIGHT_BLUE "%-20s " RESET "| " YELLOW "Waiting for dependency: " BRIGHT_YELLOW "%-25s " RESET "\n", current_time(), d->name, devices[d->depends_on].name);
        sleep(2);
    }

    d->state = RUNNING;
    printf(CYAN "[%s] " WHITE "[PROCESS] " BRIGHT_BLUE "%-20s " RESET "| " MAGENTA "Priority: " BRIGHT_MAGENTA "%-3d" RESET " | " BRIGHT_GREEN "STATE: RUNNING" RESET "             \n", current_time(), d->name, d->priority);

    pthread_t threads[SENSOR_COUNT];
    for(int i=0;i<SENSOR_COUNT;i++){
        SensorArg* s = malloc(sizeof(SensorArg));
        snprintf(s->name, sizeof(s->name), "%s - Sensor %d", d->name, i+1);
        s->device = d;
        s->sensor_index = i;
        pthread_create(&threads[i], NULL, sensor_thread, s);
        sleep(1);
    }

    for(int i=0;i<SENSOR_COUNT;i++)
        pthread_join(threads[i], NULL);

    // ---------------- READ DATA FROM PIPE ----------------
    for(int i=0;i<SENSOR_COUNT;i++){
        int sensor_value;
        read(d->pipe_fd[0], &sensor_value, sizeof(int));
          printf(BRIGHT_CYAN "[%s] " WHITE "[HUB]    " BRIGHT_CYAN "%s received Sensor %d: "YELLOW "%d\n" RESET, current_time(), d->name, i+1, sensor_value);
    }

    d->state = TERMINATED;
    printf(CYAN "[%s] " WHITE "[PROCESS] " BRIGHT_BLUE "%-20s " RESET "              | " YELLOW "STATE CHANGED → " RED "TERMINATED" RESET "                \n", current_time(), d->name);
    sleep(1);
}

// ---------------- PRINT DEVICE TABLE ----------------
void print_device_table(Device devices[]){
    printf("\n" BRIGHT_CYAN BOLD "[HUB] Current Device Table:" RESET "\n");
    printf(BRIGHT_WHITE "+----------------------+----------+--------------+\n" RESET);
    printf(BRIGHT_WHITE "| " RESET BOLD "%-20s " RESET BRIGHT_WHITE "| " RESET BOLD "%-8s " RESET BRIGHT_WHITE "| " RESET BOLD "%-12s " RESET BRIGHT_WHITE "|\n" RESET, "Device Name","Priority","State");
    printf(BRIGHT_WHITE "+----------------------+----------+--------------+\n" RESET);
    for(int i=0;i<DEVICE_COUNT;i++){
        char* state;
        char* state_color;
        switch(devices[i].state){
            case READY: state="READY"; state_color=BRIGHT_GREEN; break;
            case RUNNING: state="RUNNING"; state_color=BRIGHT_YELLOW; break;
            case WAITING: state="WAITING"; state_color=BRIGHT_RED; break;
            case TERMINATED: state="TERMINATED"; state_color=RED; break;
            default: state="UNKNOWN"; state_color=WHITE;
        }
        printf(BRIGHT_WHITE "| " RESET CYAN "%-20s " RESET BRIGHT_WHITE "| " RESET MAGENTA "%-8d " RESET BRIGHT_WHITE "| " RESET "%s%-12s" RESET BRIGHT_WHITE " |\n" RESET, devices[i].name, devices[i].priority, state_color, state);
    }
    printf(BRIGHT_WHITE "+----------------------+----------+--------------+\n" RESET);
}

// ---------------- PRINT SENSOR SUMMARY ----------------
void print_sensor_summary(Device devices[]){
    printf("\n" BRIGHT_CYAN BOLD "[HUB] Average Sensor Readings per Device:" RESET "\n");
    printf(BRIGHT_WHITE "+----------------------+------------+------------+------------+------------+------------+------------+\n" RESET);
    printf(BRIGHT_WHITE "| " RESET BOLD "%-20s " RESET BRIGHT_WHITE "| " RESET BOLD "%-10s " RESET BRIGHT_WHITE "| " RESET BOLD "%-10s " RESET BRIGHT_WHITE "| " RESET BOLD "%-10s " RESET BRIGHT_WHITE "| " RESET BOLD "%-10s " RESET BRIGHT_WHITE "| " RESET BOLD "%-10s " RESET BRIGHT_WHITE "| " RESET BOLD "%-10s " RESET BRIGHT_WHITE "|\n" RESET,
           "Device","S1 Temp","S2 Hum","S3 Light","S4 Motion","S5 Gas","S6 Noise");
    printf(BRIGHT_WHITE "+----------------------+------------+------------+------------+------------+------------+------------+\n" RESET);
    for(int i=0;i<DEVICE_COUNT;i++){
        printf(BRIGHT_WHITE "| " RESET CYAN "%-20s" RESET, devices[i].name);
        for(int j=0;j<SENSOR_COUNT;j++){
            printf(BRIGHT_WHITE " | " RESET YELLOW "%-10d" RESET, devices[i].sensor_data[j]);
        }
        printf(BRIGHT_WHITE " |\n" RESET);
    }
    printf(BRIGHT_WHITE "+----------------------+------------+------------+------------+------------+------------+------------+\n" RESET);
}

// ---------------- PRINT PROJECT TITLE ----------------
void print_project_title(){
    printf("\n");
    printf(BRIGHT_CYAN BOLD "\t\t╔═══════════════════════════════════════════════════════════════╗\n" RESET);
    printf(BRIGHT_CYAN BOLD "\t\t║" RESET);
    printf(BRIGHT_WHITE BOLD "\t         SMART HOME OS HUB SIMULATION SYSTEM" RESET);
    printf(BRIGHT_CYAN BOLD "\t        ║\n" RESET);
    printf(BRIGHT_CYAN BOLD "\t\t║" RESET);
    printf(WHITE "\t          Operating Systems Semester Project" RESET);
    printf(BRIGHT_CYAN BOLD "            ║\n" RESET);
    printf(BRIGHT_CYAN BOLD "\t\t╚═══════════════════════════════════════════════════════════════╝\n" RESET);
    printf("\n");
}

// ---------------- MAIN ----------------
int main(){
    print_project_title();
    sleep(2);

    for(int i=0;i<RESOURCE_COUNT;i++)
        pthread_mutex_init(&resources[i], NULL);

    Device devices[DEVICE_COUNT] = {
        {"Security Camera",1,READY,-1,{0}},
        {"Air Conditioner",2,READY,-1,{0}},
        {"Smart Door Lock",2,READY,0,{0},},
        {"Smart Light",3,READY,-1,{0}},
        {"Thermostat",2,READY,1,{0}},
        {"Water Heater",3,READY,-1,{0}},
        {"Refrigerator",2,READY,-1,{0}},
        {"Oven",2,READY,6,{0}},
        {"Coffee Machine",3,READY,-1,{0}},
        {"Air Purifier",2,READY,-1,{0}},
        {"Vacuum Cleaner",3,READY,-1,{0}},
        {"Sprinkler System",3,READY,-1,{0}},
        {"Fan",3,READY,-1,{0}},
        {"Heater",2,READY,-1,{0}},
        {"Door Sensor",1,READY,-1,{0}}
    };

    printf("\n" BRIGHT_CYAN BOLD "  [HUB] " RESET YELLOW "Initializing Device Table " RESET "(" BRIGHT_GREEN "STATE: READY" RESET ")\n");
    print_device_table(devices);
    sleep(2);

    // ---------------- CREATE PIPES FOR DEVICES ----------------
    for(int i=0;i<DEVICE_COUNT;i++)
        pipe(devices[i].pipe_fd);

    printf("\n" BRIGHT_CYAN BOLD "[HUB] " RESET BRIGHT_YELLOW BOLD "Sorting Devices by Priority (Priority Queue Algorithm)..." RESET "\n");
    sort_devices_by_priority(devices);
    sleep(1);

    printf("\n" BRIGHT_CYAN BOLD "[HUB] " RESET BRIGHT_YELLOW BOLD "Applying Priority Scheduling" RESET "\n");
    print_device_table(devices);
    sleep(3);

    for(int i=0;i<DEVICE_COUNT;i++){
        printf("\n" BRIGHT_CYAN "+--------------------------------------------------------------------------------------------+\n" RESET);
        printf(BRIGHT_CYAN "| " RESET BRIGHT_CYAN BOLD "                 [HUB] " RESET BRIGHT_YELLOW "Dispatching Device: " RESET BRIGHT_WHITE BOLD "%-42s " RESET BRIGHT_CYAN "      |\n" RESET, devices[i].name);
        printf(BRIGHT_CYAN "+--------------------------------------------------------------------------------------------+\n" RESET);
        run_device(&devices[i], devices);
        sleep(1);
    }

    printf("\n" BRIGHT_CYAN BOLD "[HUB] " RESET BRIGHT_YELLOW BOLD "Resource Usage Summary:" RESET "\n");
    printf(BRIGHT_WHITE "+----------------------+------------------+\n" RESET);
    printf(BRIGHT_WHITE "| " RESET BOLD "%-20s " RESET BRIGHT_WHITE "| " RESET BOLD "%-16s " RESET BRIGHT_WHITE "|\n" RESET, "Resource", "Usage Count");
    printf(BRIGHT_WHITE "+----------------------+------------------+\n" RESET);
    for(int i=0;i<RESOURCE_COUNT;i++)
        printf(BRIGHT_WHITE "| " RESET CYAN "%-20s " RESET BRIGHT_WHITE "| " RESET YELLOW "%-16d " RESET BRIGHT_WHITE "|\n" RESET, resource_names[i], resource_usage[i]);
    printf(BRIGHT_WHITE "+----------------------+------------------+\n" RESET);

    print_sensor_summary(devices);

    for(int i=0;i<RESOURCE_COUNT;i++)
        pthread_mutex_destroy(&resources[i]);

    printf("\n");
    printf(BRIGHT_CYAN BOLD "\t\t╔═══════════════════════════════════════════════════════════════╗\n" RESET);
    printf(BRIGHT_CYAN BOLD "\t\t║" RESET);
    printf(BRIGHT_WHITE BOLD "\t               SMART HOME OS HUB TERMINATED" RESET);
    printf(BRIGHT_CYAN BOLD "             ║\n" RESET);
    printf(BRIGHT_CYAN BOLD "\t\t╚═══════════════════════════════════════════════════════════════╝\n" RESET);
    printf("\n");
    return 0;
}