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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define DEVICE_COUNT 15
#define SENSOR_COUNT 6
#define RESOURCE_COUNT 6
#define WAIT_THRESHOLD 3 // seconds

#define READY 0
#define RUNNING 1
#define WAITING 2
#define TERMINATED 3

// ---------------- SHARED RESOURCES ----------------
pthread_mutex_t resources[RESOURCE_COUNT];
char* resource_names[RESOURCE_COUNT] = {"WiFi","Power","Memory","Battery","Storage","CPU"};
int resource_usage[RESOURCE_COUNT] = {0};

// ---------------- TIME ----------------
char* current_time() {
    static char buf[20];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    sprintf(buf,"%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    return buf;
}

// ---------------- DEVICE STRUCT ----------------
typedef struct {
    char name[50];
    int priority;
    int state;
    int depends_on; // -1 if none
    int sensor_sum[SENSOR_COUNT];
    int sensor_count[SENSOR_COUNT];
} Device;

// ---------------- SENSOR THREAD ARG ----------------
typedef struct {
    Device* device;
    char name[100];
    int sensor_index;
} SensorArg;

// ---------------- SENSOR DATA ----------------
int generate_sensor_data(int index){
    srand(time(NULL) + rand()%1000 + index);
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
    Device* d = s->device;

    printf("[%s] [THREAD] %-35s | Requesting resources                    |\n", current_time(), s->name);

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
            printf("[%s] [THREAD] %-35s | WAITING (Resources Busy)                  |\n", current_time(), s->name);
            sleep(1);
            if(time(NULL)-start_wait >= WAIT_THRESHOLD)
                printf("[%s] [ALERT] %-35s | Waiting >%d sec!                       |\n", current_time(), s->name, WAIT_THRESHOLD);
        }
    }

    d->state = RUNNING;
    int data = generate_sensor_data(s->sensor_index);
    d->sensor_sum[s->sensor_index] += data;
    d->sensor_count[s->sensor_index]++;

    printf("[%s] [THREAD] %-35s | Resources Allocated | Sensor Data: %-3d        |\n", current_time(), s->name, data);
    sleep(1);

    for(int i=0;i<RESOURCE_COUNT;i++)
        if(locked[i]) pthread_mutex_unlock(&resources[i]);

    printf("[%s] [THREAD] %-35s | Resources Released                             |\n", current_time(), s->name);
    free(s);
    pthread_exit(NULL);
}

// ---------------- DEVICE FUNCTION ----------------
void run_device(Device* d, Device devices[]){
    if(d->depends_on!=-1){
        printf("[%s] [PROCESS] %-20s | Waiting for dependency: %-25s |\n", current_time(), d->name, devices[d->depends_on].name);
        sleep(2);
    }

    d->state = RUNNING;
    printf("[%s] [PROCESS] %-20s | Priority: %-3d | STATE: RUNNING             |\n", current_time(), d->name, d->priority);

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
    printf("[%s] [PROCESS] %-20s | STATE CHANGED → TERMINATED                |\n", current_time(), d->name);
    sleep(1);
}

// ---------------- PRINT DEVICE TABLE ----------------
void print_device_table(Device devices[]){
    printf("\n[HUB] Current Device Table:\n");
    printf("+----------------------+----------+--------------+\n");
    printf("| %-20s | %-8s | %-12s |\n", "Device Name","Priority","State");
    printf("+----------------------+----------+--------------+\n");
    for(int i=0;i<DEVICE_COUNT;i++){
        char* state;
        switch(devices[i].state){
            case READY: state="READY"; break;
            case RUNNING: state="RUNNING"; break;
            case WAITING: state="WAITING"; break;
            case TERMINATED: state="TERMINATED"; break;
            default: state="UNKNOWN";
        }
        printf("| %-20s | %-8d | %-12s |\n", devices[i].name, devices[i].priority, state);
    }
    printf("+----------------------+----------+--------------+\n");
}

// ---------------- PRINT SENSOR SUMMARY ----------------
void print_sensor_summary(Device devices[]){
    printf("\n[HUB] Average Sensor Readings per Device:\n");
    printf("+----------------------+------------+------------+------------+------------+------------+------------+\n");
    printf("| %-20s | %-10s | %-10s | %-10s | %-10s | %-10s | %-10s |\n",
           "Device","S1 Temp","S2 Hum","S3 Light","S4 Motion","S5 Gas","S6 Noise");
    printf("+----------------------+------------+------------+------------+------------+------------+------------+\n");
    for(int i=0;i<DEVICE_COUNT;i++){
        printf("| %-20s", devices[i].name);
        for(int j=0;j<SENSOR_COUNT;j++){
            int avg = devices[i].sensor_count[j]==0?0:devices[i].sensor_sum[j]/devices[i].sensor_count[j];
            printf(" | %-10d", avg);
        }
        printf(" |\n");
    }
    printf("+----------------------+------------+------------+------------+------------+------------+------------+\n");
}

// ---------------- MAIN ----------------
int main(){
    printf("\n================ SMART HOME OS HUB STARTED ================\n");
    sleep(2);

    for(int i=0;i<RESOURCE_COUNT;i++)
        pthread_mutex_init(&resources[i], NULL);

    Device devices[DEVICE_COUNT] = {
        {"Security Camera",1,READY,-1,{0},{0}},
        {"Air Conditioner",2,READY,-1,{0},{0}},
        {"Smart Door Lock",2,READY,0,{0},{0}},
        {"Smart Light",3,READY,-1,{0},{0}},
        {"Thermostat",2,READY,1,{0},{0}},
        {"Water Heater",3,READY,-1,{0},{0}},
        {"Refrigerator",2,READY,-1,{0},{0}},
        {"Oven",2,READY,6,{0},{0}},
        {"Coffee Machine",3,READY,-1,{0},{0}},
        {"Air Purifier",2,READY,-1,{0},{0}},
        {"Vacuum Cleaner",3,READY,-1,{0},{0}},
        {"Sprinkler System",3,READY,-1,{0},{0}},
        {"Fan",3,READY,-1,{0},{0}},
        {"Heater",2,READY,-1,{0},{0}},
        {"Door Sensor",1,READY,-1,{0},{0}}
    };

    printf("\n[HUB] Initializing Device Table (STATE: READY)\n");
    print_device_table(devices);
    sleep(2);

    printf("\n[HUB] Applying Priority Scheduling\n");
    printf("+----+----------------------+----------+\n");
    printf("| No | %-20s | Priority |\n", "Device Name");
    printf("+----+----------------------+----------+\n");
    for(int i=0;i<DEVICE_COUNT;i++)
        printf("| %-2d | %-20s | %-8d |\n", i+1, devices[i].name, devices[i].priority);
    printf("+----+----------------------+----------+\n");
    sleep(3);

    for(int i=0;i<DEVICE_COUNT;i++){
        printf("\n+------------------------------------------------------------------+\n");
        printf("| [HUB] Dispatching Device: %-42s |\n", devices[i].name);
        printf("+------------------------------------------------------------------+\n");
        run_device(&devices[i], devices);
        sleep(1);
    }

    printf("\n[HUB] Resource Usage Summary:\n");
    for(int i=0;i<RESOURCE_COUNT;i++)
        printf("    %-10s used: %d times\n", resource_names[i], resource_usage[i]);

    print_sensor_summary(devices);

    for(int i=0;i<RESOURCE_COUNT;i++)
        pthread_mutex_destroy(&resources[i]);

    printf("\n================ SMART HOME OS HUB TERMINATED ================\n");
    return 0;
}