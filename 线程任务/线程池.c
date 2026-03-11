#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#define MAX_NUM 10
int num=0;
typedef struct {
    void (*func)(void*);
    void* arg;
} Task;
typedef struct {
    int front;
    int last;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    Task tasks[MAX_NUM];
} Queue;
typedef struct{
    pthread_t Thread[MAX_NUM];
    int is_running;
    Queue queue;
} Pool;
Pool* pool;
void queue_init(Queue*queue){
    queue->front = queue->last = queue->count=0;
    pthread_mutex_init(&queue->mutex,NULL);
    pthread_cond_init(&queue->cond,NULL);
}
void* pthread_func(void*arg) {
    while (pool->is_running) {
        pthread_mutex_lock(&pool->queue.mutex);
        while (pool->queue.count == 0&&pool->is_running) {
            pthread_cond_wait(&pool->queue.cond, &pool->queue.mutex);
        }
        if(!pool->is_running&&pool->queue.count==0){
            pthread_mutex_unlock(&pool->queue.mutex);
            return NULL;
        }
        Task task;
        task = pool->queue.tasks[pool->queue.front];
        pool->queue.front = (pool->queue.front + 1) % MAX_NUM;
        pool->queue.count--;
        task.func(task.arg);
        pthread_mutex_unlock(&pool->queue.mutex);
    }
    return NULL;
}
void pool_init() {
    pool = (Pool*)malloc(sizeof(Pool));
    if(pool==NULL){
        perror("malloc failed");
        exit(1);
    }
    pool->is_running = 1;
    queue_init(&pool->queue);
    for (int i = 0; i < MAX_NUM; i++) {
        pthread_create(&pool->Thread[i], NULL, pthread_func, NULL);
    }
}
void task_add(Task task){
    if(pool==NULL){
        return;
    }
    pthread_mutex_lock(&pool->queue.mutex);
    if(pool->queue.count>=MAX_NUM){
        pthread_mutex_unlock(&pool->queue.mutex);
        return;
    }
    pool->queue.tasks[pool->queue.last] = task;
    pool->queue.last = (pool->queue.last + 1) % MAX_NUM;
    pool->queue.count++;
    pthread_cond_signal(&pool->queue.cond);
    pthread_mutex_unlock(&pool->queue.mutex);
}
void *juzhen(void *arg){
     num = *(int*)arg;
    long long ret;
    ret = num * num;
    printf("线程%d计算%d的平方等于%lld\n", pthread_self(), num, ret);
    free(arg);
}
void add_juzhen(){
    for (int i = 0; i < 10;i++){
        Task task;
        task.func = juzhen;
        task.arg = malloc(sizeof(int));
        *(int*)task.arg = i;
        task_add(task);
    }
}
void Close(){
        pthread_mutex_lock(&pool->queue.mutex);
        pool->is_running = 0;
        pthread_mutex_unlock(&pool->queue.mutex);
    pthread_cond_broadcast(&pool->queue.cond);
    for (int i = 0; i < MAX_NUM;i++){
        pthread_join(pool->Thread[i],NULL);
    }
    pthread_mutex_destroy(&pool->queue.mutex);
    pthread_cond_destroy(&pool->queue.cond);
    free(pool);
    pool = NULL;
    printf("线程已经安全关闭\n");
}
int main(){
    pool_init();
    add_juzhen();
    usleep(1000);
    Close();
    fflush(stdout);
}