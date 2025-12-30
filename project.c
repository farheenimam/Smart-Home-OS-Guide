/*
=========================================================================
Smart Home OS Hub Simulation
Semester Project – Operating Systems (40 Marks)

Features:
1. Devices = simulated processes
2. Threads manage sensors (6 sensors per device)
3. Inter-thread communication and shared resource handling
4. Priority Scheduling with dependencies
5. Deadlock avoidance using trylock
6. Waiting queues + alerts
7. Average sensor readings per device
8. Resource usage summary
9. Timestamps for all actions
10. Gradual execution for live demo
=========================================================================
*/

#include <stdio.h> // printf, sprintf
#include <stdlib.h> // malloc, free, rand, srand
#include <pthread.h> // pthreads, mutexes
#include <unistd.h> // sleep
#include <time.h> // time, localtime, struct tm

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
    static char buf[20]; // memory remains valid after function returns
    time_t now = time(NULL);
    struct tm *t = localtime(&now); // readable format
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
        case 0: return 18 + rand()%13;   // Temp
        case 1: return 20 + rand()%61;   // Humidity
        case 2: return rand()%101;        // Light
        case 3: return rand()%2;          // Motion
        case 4: return rand()%201;        // Gas
        case 5: return rand()%100;        // Noise
        default: return rand()%100;
    }
}

// ---------------- SENSOR THREAD ----------------
void* sensor_thread(void* arg){
    SensorArg* s = (SensorArg*)arg;
    Device* d = s->device;  // jo device sensor use krrhi hn

    printf(CYAN "[%s] " WHITE "[THREAD] " BRIGHT_CYAN "%-35s " RESET "| " YELLOW "Requesting resources" RESET "                    \n", current_time(), s->name);

    int locked[RESOURCE_COUNT] = {0};
    int acquired = 0;
    time_t start_wait = time(NULL);

    // Helps in deadlock avoidance, checks which resource is available
    while(!acquired){ // Jab tk true hn
        acquired = 1;
        for(int i=0;i<RESOURCE_COUNT;i++){
            if(pthread_mutex_trylock(&resources[i])==0){ // checks if the resource is unlocked
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

    printf(CYAN "[%s] " WHITE "[THREAD] " BRIGHT_CYAN "%-35s " RESET "| " BRIGHT_GREEN "Resources Allocated" RESET " | " YELLOW "Sensor Data: " BRIGHT_YELLOW "%-3d" RESET "   \n", current_time(), s->name, data);
    sleep(1);
    // releasing resources after they are completed
    for(int i=0;i<RESOURCE_COUNT;i++)
        if(locked[i]) pthread_mutex_unlock(&resources[i]);

    printf(CYAN "[%s] " WHITE "[THREAD] " BRIGHT_CYAN "%-35s " RESET "| " GREEN "Resources Released" RESET "                             \n", current_time(), s->name);
    free(s);
    pthread_exit(NULL);
}

// ---------------- SORT DEVICES BY PRIORITY (PRIORITY QUEUE) ----------------
// Bubble sort algorithm: Sort devices by priority (lower number = higher priority)
void sort_devices_by_priority(Device devices[]){
    // Priority queue implementation using bubble sort
    for(int i = 0; i < DEVICE_COUNT; i++){
        for(int j = i + 1; j < DEVICE_COUNT; j++){
            // If current device has higher priority number (lower priority), swap
            if(devices[i].priority > devices[j].priority){
                // Swap devices - put lower priority number first
                Device temp = devices[i];
                devices[i] = devices[j];
                devices[j] = temp;
            }
        }
    }
}

// ---------------- DEVICE FUNCTION ----------------
// executes the devices, creates sensor threads, waits for them to finish
void run_device(Device* d, Device devices[]){
    //checks if the device is waiting for another device, -1 no dependency
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
        // Name, Priority, State, Depends_on, Sensor_data[]
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

    printf("\n" BRIGHT_CYAN BOLD "[HUB] " RESET YELLOW "Initializing Device Table " RESET "(" BRIGHT_GREEN "STATE: READY" RESET ")\n");
    print_device_table(devices);
    sleep(2);

    printf("\n" BRIGHT_CYAN BOLD "[HUB] " RESET BRIGHT_YELLOW BOLD "Sorting Devices by Priority (Priority Queue Algorithm)..." RESET "\n");
    sort_devices_by_priority(devices);
    sleep(1);

    printf("\n" BRIGHT_CYAN BOLD "[HUB] " RESET BRIGHT_YELLOW BOLD "Applying Priority Scheduling" RESET "\n");
    printf(BRIGHT_WHITE "+----+----------------------+----------+\n" RESET);
    printf(BRIGHT_WHITE "| " RESET BOLD "No " RESET BRIGHT_WHITE "| " RESET BOLD "%-20s " RESET BRIGHT_WHITE "| " RESET BOLD "Priority " RESET BRIGHT_WHITE "|\n" RESET, "Device Name");
    printf(BRIGHT_WHITE "+----+----------------------+----------+\n" RESET);
    for(int i=0;i<DEVICE_COUNT;i++)
        printf(BRIGHT_WHITE "| " RESET YELLOW "%-2d " RESET BRIGHT_WHITE "| " RESET CYAN "%-20s " RESET BRIGHT_WHITE "| " RESET MAGENTA "%-8d " RESET BRIGHT_WHITE "|\n" RESET, i+1, devices[i].name, devices[i].priority);
    printf(BRIGHT_WHITE "+----+----------------------+----------+\n" RESET);
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
    printf(BRIGHT_WHITE BOLD "\t                SMART HOME OS HUB TERMINATED" RESET);
    printf(BRIGHT_CYAN BOLD "                  ║\n" RESET);
    printf(BRIGHT_CYAN BOLD "\t\t╚═══════════════════════════════════════════════════════════════╝\n" RESET);
    printf("\n");
    return 0;
}
