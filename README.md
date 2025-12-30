# Smart Home OS Hub Simulation - Project Documentation

## Project Overview (Project Ka Overview)

Yeh project Operating Systems ke concepts ko demonstrate karta hai ek Smart Home simulation ke through. Is mein devices ko processes ki tarah simulate kiya gaya hai, aur har device ke sensors ko threads se manage kiya jata hai. Yeh project priority scheduling, deadlock avoidance, resource sharing, aur inter-thread communication ko practically explain karta hai.

---

## Libraries Aur Unki Zarurat (Libraries and Their Purpose)

### 1. `<stdlib.h>`
**Kyun Import Kiya:**
- `malloc()` function ke liye - dynamic memory allocation ke liye
- `free()` function ke liye - allocated memory ko release karne ke liye
- `srand()` aur `rand()` functions ke liye - random sensor data generate karne ke liye
- `NULL` macro ke liye - null pointer define karne ke liye

**Line 160:** `SensorArg* s = malloc(sizeof(SensorArg));`
- Yeh line dynamically memory allocate karti hai SensorArg structure ke size ke barabar
- `malloc()` heap memory se memory allocate karta hai
- Return value void pointer hoti hai jo hum typecast karte hain `SensorArg*` mein
- Agar memory available nahi hai to NULL return hota hai

**Line 144:** `free(s);`
- Yeh allocated memory ko release karta hai
- Memory leak prevent karne ke liye zaroori hai
- Thread complete hone ke baad sensor argument ki memory free kar di jaati hai

### 2. `<pthread.h>`
**Kyun Import Kiya:**
- POSIX threads (pthreads) library hai jo multi-threading support karti hai
- Threads create karne, join karne, aur synchronize karne ke liye
- Mutex locks ke liye deadlock prevention implement karne ke liye

**Important Functions:**

**Line 235:** `pthread_mutex_init(&resources[i], NULL);`
- Mutex (Mutual Exclusion) locks initialize karta hai
- Har resource (WiFi, Power, Memory, etc.) ke liye ek mutex create hota hai
- Second parameter NULL hai jo default mutex attributes use karta hai
- Mutex synchronization provide karta hai - ek time par sirf ek thread resource access kar sakta hai

**Line 114:** `pthread_mutex_trylock(&resources[i])==0`
- **Deadlock Avoidance Algorithm:** Yeh trylock mechanism use karta hai
- Regular `pthread_mutex_lock()` blocking hota hai (wait karta hai jab lock nahi milta)
- `pthread_mutex_trylock()` non-blocking hai - immediately return hota hai
- Agar lock mil gaya (return 0), to thread resource access kar sakta hai
- Agar lock nahi mila (return non-zero), to thread wait nahi karta, balke apni current locks release kar deta hai
- Yeh **deadlock prevention** guarantee karta hai kyunki threads circular wait mein nahi fase

**Line 123:** `pthread_mutex_unlock(&resources[i]);`
- Lock release karta hai jisse other threads resource access kar saken
- Mutex ko properly unlock karna zaroori hai warna deadlock ho sakta hai

**Line 164:** `pthread_create(&threads[i], NULL, sensor_thread, s);`
- Naya thread create karta hai
- First parameter: thread ID store karne ke liye pointer
- Second parameter: thread attributes (NULL = default)
- Third parameter: function jo thread mein execute hogi (sensor_thread)
- Fourth parameter: function ko pass hone wala argument (SensorArg structure)
- Yeh har sensor ke liye alag thread create karta hai

**Line 169:** `pthread_join(threads[i], NULL);`
- Parent thread child thread ka wait karta hai completion tak
- Second parameter NULL hai jo thread return value collect nahi karta
- Yeh ensure karta hai ke saare sensor threads complete hone ke baad hi device terminated ho
- Without join, program terminate ho sakta hai before threads complete

**Line 145:** `pthread_exit(NULL);`
- Thread explicitly terminate karta hai
- NULL return value indicate karta hai
- Thread resources automatically clean ho jati hain

**Line 287:** `pthread_mutex_destroy(&resources[i]);`
- Mutex locks ko destroy karta hai
- Program end hone se pehle cleanup ke liye
- Destroyed mutex ko reuse nahi kar sakte

### 3. `<string.h>`
**Kyun Import Kiya:**
- String manipulation functions ke liye

**Line 161:** `snprintf(s->name, sizeof(s->name), "%s - Sensor %d", d->name, i+1);`
- Safe string formatting karta hai buffer overflow prevent karne ke liye
- `snprintf()` maximum buffer size specify karta hai (`sizeof(s->name)`)
- Regular `sprintf()` unsafe hai kyunki buffer overflow ho sakta hai
- Yeh sensor ka name format karta hai: "Device Name - Sensor 1", "Device Name - Sensor 2", etc.

### 4. `<unistd.h>`
**Kyun Import Kiya:**
- Unix standard library - sleep function ke liye

**Multiple Lines:** `sleep(1)`, `sleep(2)`, `sleep(3)`
- Program execution ko delay deta hai specified seconds ke liye
- Live demo ke liye gradual execution enable karta hai
- Thread scheduling aur timing simulation ke liye use hota hai
- Seconds mein parameter pass hota hai

### 5. `<time.h>`
**Kyun Import Kiya:**
- Time-related functions ke liye - timestamps generate karne ke liye

**Line 63:** `time_t now = time(NULL);`
- Current system time ko seconds mein return karta hai (epoch time)
- NULL parameter current time fetch karta hai
- `time_t` type time ko store karta hai

**Line 64:** `struct tm *t = localtime(&now);`
- `time_t` ko local time structure mein convert karta hai
- `struct tm` contains: tm_hour, tm_min, tm_sec, etc.
- System ke timezone ke according local time return karta hai

**Line 65:** `sprintf(buf,"%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);`
- Time ko formatted string mein convert karta hai
- `%02d` ensures 2-digit formatting with leading zero
- Format: "HH:MM:SS"

**Line 88:** `srand(time(NULL) + rand()%1000 + index);`
- Random number generator seed set karta hai
- `time(NULL)` current time use karta hai taake har run par different random numbers aayein
- `rand()%1000` aur `index` add karke har sensor ke liye unique seed ensure karta hai

**Line 109:** `time_t start_wait = time(NULL);`
- Waiting start time record karta hai
- Baad mein use hota hai wait duration calculate karne ke liye

**Line 127:** `if(time(NULL)-start_wait >= WAIT_THRESHOLD)`
- Current time minus start time calculates wait duration
- Agar wait time 3 seconds se zyada ho jaye to alert display hota hai
- Time-based threshold checking

---

## Data Structures (Data Structures)

### Device Structure (Line 70-77)
```c
typedef struct {
    char name[50];
    int priority;
    int state;
    int depends_on;
    int sensor_sum[SENSOR_COUNT];
    int sensor_count[SENSOR_COUNT];
} Device;
```

**Explanation:**
- `name[50]`: Device ka name store karta hai (e.g., "Security Camera")
- `priority`: Process priority (1 = highest, 2 = medium, 3 = lowest)
- `state`: Device ka current state (READY, RUNNING, WAITING, TERMINATED)
- `depends_on`: Agar device kisi aur device ka wait kar raha hai to uska index, warna -1
- `sensor_sum[]`: Har sensor ke readings ka sum (average calculate karne ke liye)
- `sensor_count[]`: Har sensor ke kitne readings hain (average = sum/count)

### SensorArg Structure (Line 80-84)
```c
typedef struct {
    Device* device;
    char name[100];
    int sensor_index;
} SensorArg;
```

**Explanation:**
- `device*`: Pointer to parent device (shared data access ke liye)
- `name[100]`: Sensor ka formatted name
- `sensor_index`: Sensor ka index (0-5) jo determine karta hai kaunsa sensor hai

---

## Algorithms Implementation (Algorithms Ka Implementation)

### 1. Priority Scheduling Algorithm (Line 259-265)

**How It Works:**
- Devices already priority order mein sorted hain (1, 2, 3)
- Har device ko sequentially execute kiya jata hai according to priority
- Lower priority number = higher priority (1 sabse zyada priority)

**Implementation:**
```c
for(int i=0;i<DEVICE_COUNT;i++){
    run_device(&devices[i], devices);
}
```
- Simple FCFS (First Come First Served) priority order mein
- Priority 1 devices pehle execute hote hain, phir 2, phir 3

### 2. Dependency Handling Algorithm (Line 150-153)

**How It Works:**
- Agar device ka `depends_on` -1 nahi hai, matlab wo kisi aur device ka wait karega
- Dependency graph implementation hai

**Example:**
- "Smart Door Lock" depends on "Security Camera" (index 0)
- "Thermostat" depends on "Air Conditioner" (index 1)
- "Oven" depends on "Refrigerator" (index 6)

**Implementation:**
```c
if(d->depends_on!=-1){
    // Wait for dependency device to complete
    sleep(2);
}
```
- 2 seconds wait simulate karta hai dependency completion

### 3. Deadlock Avoidance Algorithm (Line 111-130)

**Algorithm Name:** Trylock-based Deadlock Prevention

**How It Works:**
1. Thread saare resources ko trylock se acquire karne ki koshish karta hai
2. Agar koi bhi resource unavailable hai, thread apni saari acquired locks release kar deta hai
3. Thread wait karta hai (sleep), phir dobara try karta hai
4. Yeh **All-or-Nothing** approach hai - ya saare resources mil jayein ya kuch bhi nahi

**Implementation Steps:**
```c
while(!acquired){
    acquired = 1;  // Assume success initially
    // Try to lock all resources
    for(int i=0;i<RESOURCE_COUNT;i++){
        if(pthread_mutex_trylock(&resources[i])==0){
            locked[i]=1;  // Mark as locked
        } else {
            acquired = 0;  // Failed - need to retry
        }
    }
    if(!acquired){
        // Release all locks if couldn't acquire all
        for(int i=0;i<RESOURCE_COUNT;i++){
            if(locked[i]) pthread_mutex_unlock(&resources[i]);
        }
        sleep(1);  // Wait before retry
    }
}
```

**Why This Prevents Deadlock:**
- Circular wait impossible hai kyunki threads ek saath saare resources chahiye
- Agar circular wait banne wali ho, to koi bhi thread resources acquire nahi kar payega
- Threads wait queue mein rehte hain jab resources busy hain
- No hold and wait - thread either sab kuch hold karta hai ya kuch bhi nahi

### 4. Resource Allocation Algorithm (Line 111-130)

**All-or-Nothing Resource Allocation:**
- Thread ko saare 6 resources ek saath chahiye (WiFi, Power, Memory, Battery, Storage, CPU)
- Agar koi bhi resource unavailable hai, thread ko kuch bhi nahi milta
- Yeh atomic operation hai - transaction-like behavior

### 5. Wait Queue and Alert System (Line 106-129)

**Wait Queue Simulation:**
- `WAITING` state device ko assign hoti hai jab resources busy hain
- Thread wait karta hai jab `acquired = 0`

**Alert System:**
```c
if(time(NULL)-start_wait >= WAIT_THRESHOLD)
    printf("[ALERT] ... Waiting >3 sec!");
```
- Agar thread 3 seconds se zyada wait kar raha hai, alert display hota hai
- Time-based monitoring system hai

### 6. Sensor Data Aggregation Algorithm (Line 133-135, 207)

**How It Works:**
- Har sensor reading ko `sensor_sum[index]` mein add kiya jata hai
- `sensor_count[index]` increment hota hai
- Average = `sensor_sum / sensor_count`

**Line 207:** `int avg = devices[i].sensor_count[j]==0?0:devices[i].sensor_sum[j]/devices[i].sensor_count[j];`
- Ternary operator use karta hai - agar count 0 hai to average 0, warna division
- Division by zero prevent karta hai

---

## Process Simulation (Process Simulation)

### How Devices Simulate Processes

1. **Process States:**
   - `READY` (0): Device initialized, execute karne ke liye ready
   - `RUNNING` (1): Currently executing, sensors active
   - `WAITING` (2): Resources unavailable, waiting
   - `TERMINATED` (3): Execution complete

2. **Process Lifecycle (run_device function):**
   - **Initialization:** Device READY state se start
   - **Dependency Check:** Agar dependency hai to wait
   - **State Change to RUNNING:** Execution start
   - **Thread Creation:** 6 sensor threads create
   - **Thread Execution:** Parallel sensor processing
   - **Thread Synchronization:** `pthread_join` se wait
   - **Termination:** State TERMINATED mein change

3. **Process Control Block (PCB) Simulation:**
   - Device structure PCB ki tarah kaam karta hai
   - Contains: name, priority, state, dependencies
   - Process metadata store karta hai

---

## Threading Model (Threading Model)

### Multi-threading Architecture

1. **Thread Hierarchy:**
   - **Main Thread:** Device processes manage karta hai
   - **Sensor Threads:** Har device ke 6 threads (parallel execution)

2. **Thread Synchronization:**
   - **Mutex Locks:** Resource sharing protect karte hain
   - **pthread_join:** Parent thread child completion ka wait karta hai
   - **Shared Memory:** Device structure shared hai threads ke beech

3. **Thread Communication:**
   - **Shared Variables:** `Device* d` se threads device data access karte hain
   - **Mutex-protected Resources:** Global resources array mutex se protected
   - **State Updates:** Threads device state modify karte hain (WAITING, RUNNING)

---

## Inter-Thread Communication Aur Resource Sharing (Inter-Thread Communication and Resource Sharing)

### Haan, Threads Resources Share Kar Rahe Hain!

**Important:** Yeh project mein multiple levels par inter-thread communication aur resource sharing ho rahi hai. Detail mein:

### 1. Shared Resources - Global Resource Pool

**Line 56-58:** 
```c
pthread_mutex_t resources[RESOURCE_COUNT];  // 6 mutex locks
char* resource_names[RESOURCE_COUNT] = {"WiFi","Power","Memory","Battery","Storage","CPU"};
int resource_usage[RESOURCE_COUNT] = {0};
```

**Explanation:**
- **Global Resources Array:** Yeh saare system ka shared resource pool hai
- **Sabhi threads share karte hain:** Har thread (har device ka har sensor) isi global resource pool ko access karta hai
- **6 Resources:** WiFi, Power, Memory, Battery, Storage, CPU
- **Mutex Protection:** Har resource ka apna mutex lock hai jo synchronization ensure karta hai

**Kaise Share Ho Raha Hai:**
```
Thread 1 (Security Camera - Sensor 1) ──┐
Thread 2 (Security Camera - Sensor 2) ──┤
Thread 3 (Air Conditioner - Sensor 1) ──┼──> Global Resources Array [WiFi, Power, Memory, ...]
Thread 4 (Air Conditioner - Sensor 2) ──┤    (Sab threads isko share karte hain)
Thread 5 (Smart Door Lock - Sensor 1) ──┤
... (90 total threads) ──────────────────┘
```

**Example:**
- Jab "Security Camera - Sensor 1" WiFi resource acquire karta hai, to woh mutex lock karta hai
- Same time par agar "Air Conditioner - Sensor 2" bhi WiFi chahiye, to wo wait karega
- Kyunki global resource pool shared hai, ek time par sirf ek thread resource use kar sakta hai

### 2. Shared Device Structure - Intra-Device Communication

**Line 81:** `Device* device;` (SensorArg structure mein)
**Line 103:** `Device* d = s->device;`

**Explanation:**
- **Same Device Ke Threads Share Karte Hain:** Ek device ke saare 6 sensor threads apne parent device ki structure share karte hain
- **Shared Memory:** Pointer ke through same Device structure access hoti hai

**Example Scenario:**
```
Device: "Security Camera"
├── Thread 1: Security Camera - Sensor 1 ──┐
├── Thread 2: Security Camera - Sensor 2 ──┤
├── Thread 3: Security Camera - Sensor 3 ──┤──> Same Device Structure (Shared Memory)
├── Thread 4: Security Camera - Sensor 4 ──┤    - sensor_sum[]
├── Thread 5: Security Camera - Sensor 5 ──┤    - sensor_count[]
└── Thread 6: Security Camera - Sensor 6 ──┘    - state
```

**What They Share:**
- **Line 134-135:** `d->sensor_sum[s->sensor_index] += data;` aur `d->sensor_count[s->sensor_index]++;`
  - Har sensor thread apna data device structure mein add karta hai
  - Sab threads same structure ko modify karte hain parallel mein
  - Yeh data aggregation hai - sab threads apna contribution add karte hain

- **Line 124:** `d->state = WAITING;`
  - Thread device ka state update karta hai
  - Agar ek thread resource nahi mil paya, wo device state WAITING set kar deta hai
  - Other threads yeh state dekh sakte hain

### 3. Resource Sharing Mechanism (Resource Sharing Kaise Ho Raha Hai)

#### A. Global Resource Pool Access

**Line 114-116:** Resource Acquisition
```c
if(pthread_mutex_trylock(&resources[i])==0){
    locked[i]=1;
    resource_usage[i]++;
}
```

**Explanation:**
- **Mutex Lock:** `pthread_mutex_trylock(&resources[i])` thread ko resource access dene ki koshish karta hai
- **Global Array Access:** `resources[i]` global array hai jo sabhi threads share karte hain
- **Usage Counter:** `resource_usage[i]++` global counter update karta hai
- **Shared Variable:** `resource_usage[]` bhi shared hai - har thread isko modify karta hai jab resource use hota hai

**Race Condition Prevention:**
- Mutex lock ensures karta hai ke ek time par sirf ek thread `resource_usage[i]` ko modify kar sake
- Bina mutex ke, multiple threads simultaneously counter increment karte to data corruption ho sakti thi

#### B. All-or-Nothing Resource Acquisition

**Line 111-130:** Complete Resource Acquisition Algorithm

**Kaise Kaam Karta Hai:**
1. Thread saare 6 resources ko ek saath acquire karne ki koshish karta hai
2. Agar koi bhi resource unavailable hai, thread apni saari acquired locks release kar deta hai
3. Yeh ensure karta hai ke multiple threads circular wait mein nahi faste

**Resource Sharing Pattern:**
```
Time T1: Thread A tries to lock [WiFi ✓, Power ✓, Memory ✓, Battery ✓, Storage ✓, CPU ✗]
         → Fails, releases all resources

Time T2: Thread B tries to lock [WiFi ✓, Power ✓, Memory ✓, Battery ✓, Storage ✓, CPU ✓]
         → Success, uses all resources

Time T3: Thread B releases all resources
         → Thread A ab dobara try kar sakta hai
```

### 4. Inter-Thread Communication Methods

#### Method 1: Shared Memory Communication

**Device Structure as Shared Memory:**
- Same device ke threads `Device* d` pointer ke through communicate karte hain
- Threads sensor data ko shared structure mein add karte hain
- Yeh producer-consumer pattern hai - threads data produce karte hain aur shared location par store

**Line 133-135:**
```c
int data = generate_sensor_data(s->sensor_index);  // Thread generates data
d->sensor_sum[s->sensor_index] += data;           // Writes to shared memory
d->sensor_count[s->sensor_index]++;               // Updates shared counter
```

**Communication Flow:**
```
Sensor Thread 1 → Generates data → Updates d->sensor_sum[0] (Shared)
Sensor Thread 2 → Generates data → Updates d->sensor_sum[1] (Shared)
Sensor Thread 3 → Generates data → Updates d->sensor_sum[2] (Shared)
...
Main Thread → Reads d->sensor_sum[] → Calculates averages
```

#### Method 2: Mutex-Based Synchronization

**Mutex as Communication Mechanism:**
- Mutex lock/unlock threads ko signal deta hai ke resource available hai ya nahi
- Thread jo lock acquire karta hai, wo other threads ko indirectly signal deta hai ke resource busy hai

**Line 114:** `pthread_mutex_trylock(&resources[i])`
- Success (return 0) → Thread ko signal: "Resource available, tum use kar sakte ho"
- Failure (non-zero) → Thread ko signal: "Resource busy, wait karo"

**Line 141:** `pthread_mutex_unlock(&resources[i])`
- Yeh other waiting threads ko signal deta hai: "Resource ab available hai"

#### Method 3: State-Based Communication

**Line 124:** `d->state = WAITING;`
**Line 132:** `d->state = RUNNING;`

**Explanation:**
- Threads device state ko modify karke communicate karte hain
- Agar ek thread resources nahi acquire kar paya, wo device state WAITING set kar deta hai
- Main thread ya other threads yeh state check kar sakte hain device ki current condition dekhne ke liye

### 5. Resource Sharing Patterns in This Project

#### Pattern 1: Competing for Shared Resources
```
90 threads (15 devices × 6 sensors) → Compete for → 6 shared resources
```
- Har thread same 6 resources ko access karne ki koshish karta hai
- Mutex locks ensure fairness aur prevent conflicts

#### Pattern 2: Cooperative Data Sharing
```
6 threads (same device) → Cooperate → Update shared Device structure
```
- Same device ke threads ek saath kaam karke device data aggregate karte hain
- Producer pattern - har thread apna data contribute karta hai

### 6. Why Mutex is Critical for Shared Resources?

**Without Mutex (Problem):**
```
Thread A: Reads resource_usage[0] = 5
Thread B: Reads resource_usage[0] = 5  (Same time par)
Thread A: Increments to 6
Thread B: Increments to 6  (Overwrites A's update)
Result: Counter = 6 (Should be 7) → Data Corruption!
```

**With Mutex (Solution):**
```
Thread A: Locks → Reads 5 → Increments to 6 → Unlocks
Thread B: Waits → Locks → Reads 6 → Increments to 7 → Unlocks
Result: Counter = 7 → Correct!
```

### 7. Real Example of Resource Sharing

**Scenario:** Security Camera aur Air Conditioner dono simultaneously run ho rahe hain

```
Time 0:00
Security Camera - Sensor 1: Tries to lock WiFi → Success
Security Camera - Sensor 1: Tries to lock Power → Success
Security Camera - Sensor 1: Tries to lock Memory → Success
...
Security Camera - Sensor 1: Tries to lock CPU → Success
Security Camera - Sensor 1: All resources acquired, using them

Time 0:01
Air Conditioner - Sensor 1: Tries to lock WiFi → Fails (Busy)
Air Conditioner - Sensor 1: Releases any acquired locks
Air Conditioner - Sensor 1: Sets device state to WAITING
Air Conditioner - Sensor 1: Sleeps 1 second

Time 0:02
Security Camera - Sensor 1: Releases all resources
Air Conditioner - Sensor 1: Wakes up, tries again
Air Conditioner - Sensor 1: Now acquires all resources successfully
```

**Key Points:**
- Dono threads same global resources ko share kar rahe the
- Mutex ne ensure kiya ke conflict nahi hua
- Threads sequentially access karte hain, parallel nahi (kyunki resources limited hain)

### 8. Communication Flow Diagram

```
┌─────────────────────────────────────────────────────────┐
│                  Global Resources Array                  │
│     [WiFi] [Power] [Memory] [Battery] [Storage] [CPU]   │
│          (Protected by Mutex Locks)                      │
└───────────────┬─────────────────────────────────────────┘
                │
    ┌───────────┼───────────┐
    │           │           │
┌───▼───┐  ┌───▼───┐  ┌───▼───┐
│Thread │  │Thread │  │Thread │  ... (90 threads)
│   A   │  │   B   │  │   C   │  Compete for resources
└───┬───┘  └───┬───┘  └───┬───┘
    │          │          │
    └──────────┼──────────┘
               │
    ┌──────────▼──────────┐
    │  Device Structures  │
    │  (Shared per device)│
    │  - sensor_sum[]     │
    │  - sensor_count[]   │
    │  - state            │
    └─────────────────────┘
```

### Summary: Resource Sharing in This Project

1. ✅ **Global Resources:** 90 threads 6 shared resources ko compete karte hain
2. ✅ **Device Structures:** Same device ke 6 threads apna data shared structure mein store karte hain
3. ✅ **Mutex Protection:** Sab shared resources mutex se protected hain
4. ✅ **Communication:** Threads shared memory, mutex locks, aur state updates ke through communicate karte hain
5. ✅ **Synchronization:** pthread_join ensures proper coordination between parent aur child threads

**Yeh pure project mein inter-thread communication aur resource sharing ka comprehensive example hai!**

---

## Memory Management (Memory Management)

### Dynamic Memory Allocation

**Line 160:** `SensorArg* s = malloc(sizeof(SensorArg));`
- Heap memory se allocation
- Size: sizeof(SensorArg) bytes
- Returns pointer to allocated memory

**Line 144:** `free(s);`
- Allocated memory release
- Memory leak prevention
- Thread exit se pehle cleanup

### Static Memory

**Line 62:** `static char buf[20];`
- Static variable - function calls ke beech persist rehta hai
- Stack par nahi, data segment mein store
- `current_time()` function multiple calls mein same buffer reuse karta hai

---

## Key Concepts Explained (Key Concepts)

### 1. Mutex (Mutual Exclusion)
- Lock mechanism jo ensures karta hai ke ek time par sirf ek thread resource access kare
- `pthread_mutex_t` mutex type hai
- Lock acquire karne ke baad unlock zaroori hai

### 2. Trylock vs Lock
- `pthread_mutex_lock()`: Blocking - wait karta hai jab lock unavailable
- `pthread_mutex_trylock()`: Non-blocking - immediately return
- Trylock deadlock prevention mein use hota hai

### 3. Thread Join vs Detach
- `pthread_join()`: Parent waits for child completion
- Join nahi kiya to thread "zombie" ban sakta hai
- Saare threads join hone chahiye proper cleanup ke liye

### 4. Shared Resources
- Global `resources[]` array sabhi threads ke liye shared
- Mutex protection zaroori hai race conditions prevent karne ke liye
- Resource usage counter track karta hai har resource ka usage

---

## Code Flow (Code Ka Flow)

1. **Initialization:**
   - Title display → 2 seconds wait
   - Mutex locks initialize (6 resources)
   - Devices array initialize (15 devices, all READY)

2. **Device Table Display:**
   - All devices ka current state display

3. **Priority Scheduling:**
   - Devices priority order mein list
   - Execution order show

4. **Device Execution Loop:**
   - Har device ke liye:
     - Dependency check
     - State → RUNNING
     - 6 sensor threads create
     - Threads execute (parallel)
     - Threads join (wait for completion)
     - State → TERMINATED

5. **Sensor Thread Execution:**
   - Resource acquisition (trylock all)
   - Sensor data generate
   - Data update (sum, count)
   - Resource release
   - Memory free

6. **Final Summary:**
   - Resource usage statistics
   - Average sensor readings per device

---

## Compilation Instructions (Compilation)

```bash
gcc -o project project.c -lpthread
```

- `-lpthread`: pthread library link karta hai
- `-o project`: Output executable name

## Execution (Execution)

```bash
./project
```

---

## Project Features Summary

1. ✅ **Process Simulation:** 15 devices as processes
2. ✅ **Multi-threading:** 6 threads per device (90 total threads)
3. ✅ **Priority Scheduling:** Priority-based execution order
4. ✅ **Dependency Handling:** Device dependency graph
5. ✅ **Deadlock Avoidance:** Trylock-based prevention algorithm
6. ✅ **Resource Management:** 6 shared resources with mutex protection
7. ✅ **Wait Queue:** Waiting state with time-based alerts
8. ✅ **Data Aggregation:** Sensor readings average calculation
9. ✅ **Real-time Monitoring:** Timestamps for all operations
10. ✅ **Visual Output:** Color-coded professional display

---

## Conclusion

Yeh project Operating Systems ke core concepts ko practically demonstrate karta hai:
- Process management aur state transitions
- Multi-threading aur synchronization
- Deadlock prevention algorithms
- Resource allocation strategies
- Inter-process/thread communication

Har line carefully designed hai to show real-world OS concepts in action.

