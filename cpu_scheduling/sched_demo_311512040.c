#include <stdio.h>
#include <unistd.h>
#include <string.h> // strtok
#include <stdlib.h>
#define __USE_GNU
#include <sched.h> // for sched_getcpu()
#include <pthread.h>
#include<sys/time.h> //gettimeofday()

typedef struct {
    pthread_t thread_id;
    int thread_num;
    int sched_policy;
    int sched_priority;
    float time_wait;
}thread_info_t;

pthread_barrier_t barrier;



void *thread_func(void *arg)
{
    /* 1. Wait until all threads are ready */
    int id;
    thread_info_t *my_data;   //定義了1個指向這種結構的指標變數my_data
    my_data = (thread_info_t *) arg;
    pthread_barrier_wait(&barrier);

    id = my_data->thread_id;
    /* 2. Do the task */ 
    for (int i = 0; i < 3; i++) {
        printf("Thread %d is running\n", id);
        

        /* Busy for <time_wait> seconds */
        struct timeval start,end;
        double starttime,endtime;
        gettimeofday(&start,NULL);
        starttime = (start.tv_sec*1000000+start.tv_usec)/1000000;
        

        while (1)
        {
            gettimeofday(&end,NULL);
            endtime = (end.tv_sec*1000000+end.tv_usec)/1000000;
            if (endtime> starttime + my_data->time_wait)
                break;
        }
        sched_yield();
    }
    /* 3. Exit the function  */
    pthread_exit(NULL);
}



int main(int argc, char *argv[]) {          //argc指示程序啟動時命令行參數的個數。argv則包含具體的參數字符串。
    /* 1. Parse program arguments */
    int ch;
    char *optstring = "n:t:s:p:";
    float busy_time;
    opterr = 0;  //使getopt不行stderr輸出錯誤資訊
    int NUM_THREADS = atoi(argv[2]);  //str轉int
    char *policies_array[NUM_THREADS];
    int int_policies[NUM_THREADS];
    char priorities_array[NUM_THREADS];
    const char *split = ","; //strtok
    char *s1, *s2;
    char *p;

    while( (ch = getopt(argc, argv, optstring)) != -1 )   // 在命令列選項引數再也檢查不到optstring中包含的選項時，返回－1
    {   
        switch(ch)
        {
        case 'n':
        // printf("option=n,  optarg=%s\n",  optarg);
        break;
        case 't':
        busy_time = atof(optarg);
        // printf("option=t, optarg=%s\n",  optarg);
        break;
        case 's':
        // printf("option=s,  optarg=%s\n",  optarg);
        s1 = optarg;
        p = strtok(s1, split);
        int i = 0;
        while (p != NULL) {
            // printf("%s\n", p);
            policies_array[i]= p;
            p = strtok(NULL, split); 
            i++;
        }
        break;
        case 'p':
        // printf("option=p,  optarg=%s\n",  optarg);
        s2 = optarg;
        p = strtok(s2, split);
        int t = 0;
        while (p != NULL) {
            priorities_array[t]=atoi(p);
            p = strtok(NULL, split);
            t++;
        }
        break;
        }  
    }

    for (int i = 0; i < NUM_THREADS; i++) 
    {  
        if (strcmp("FIFO",policies_array[i])==0){  //將policy轉成整數參數
            int_policies[i]=1;
        }
        if (strcmp("NORMAL",policies_array[i])==0){
            int_policies[i]=0;
        }  
    }  
    
    
    /* 2. Create <num_threads> worker threads */  
    int rc,rr,rp;
    int policy;
    pthread_t threads[NUM_THREADS];
    pthread_attr_t attr[NUM_THREADS];
    thread_info_t thread_data_array[NUM_THREADS];
    struct sched_param params[NUM_THREADS];
    
    /* 3. Set CPU affinity */   //設定 main 主執行緒跑在哪個 CPU
    int cpu_id = 3; // set thread to cpu3
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    /* 4. Set the attributes to each thread */

    // pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL , NUM_THREADS+1); /*需要等待n+1*/
    for (int i = 0; i < NUM_THREADS; i++) 
    {  
        pthread_attr_init(&attr[i]); /*初始化線程屬性變量*/
        pthread_attr_setinheritsched(&attr[i],PTHREAD_EXPLICIT_SCHED); /*設置線程繼承性*/
        pthread_attr_getinheritsched(&attr[i],&policy); /*獲得線程的繼承性*/
        /*pthread_attr_getinheritsched第二個參數放矩陣會報錯,無法create*/

        rp = pthread_attr_setschedpolicy(&attr[i],int_policies[i]);/*設置線程調度策略*/
        if (rp) {
        printf("ERROR; return code from pthread_policy() is %d\n", rp);
        exit(-1);
        }
        pthread_attr_getschedpolicy(&attr[i],&int_policies[i]);/*取得線程的調度策略*/

        thread_data_array[i].thread_id=i; 
        thread_data_array[i].time_wait=busy_time;
        params[i].sched_priority = priorities_array[i];
        // rr = pthread_attr_setschedparam(&attr[i],&params[i]);/*設置線程的調度參數*/
        
        if (priorities_array[i]!=-1)    /*NORMAL不能給-1,sched_priority要在1～127範圍*/
        {   
            rc = pthread_attr_setschedparam(&attr[i], &params[i]); //set priority
           
        }

        // printf("Creating thread %d\n", i);
        rc = pthread_create(&threads[i],&attr[i],thread_func,(void *)&thread_data_array[i]);
        if (rc) {
        printf("ERROR; return code from pthread_create() is %d\n", rc);
        exit(-1);
        }
        // pthread_join(threads[i], NULL);
    }
    /* 5. Start all threads at once */
    pthread_barrier_wait(&barrier);
    
    /* 6. Wait for all threads to finish  */
    pthread_exit(NULL);
    return 0;
}