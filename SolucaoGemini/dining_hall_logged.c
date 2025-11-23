/*
 * dining_hall_logged.c
 * Versão Instrumentada para Geração de Logs de Rastreio.
 * Compilação: gcc -Wall -pthread -O2 -o dining_hall_logged dining_hall_logged.c
 * Authors: Miguel Badany Cerne & Pedro Videira Rubinstein
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h> // Para gettimeofday (microsegundos)

/* Constantes */
const int NUM_ITERATIONS = 5; // Reduzi para 5 para o log não ficar gigante
const int MIN_SLEEP_MS = 10;
const int MAX_SLEEP_MS = 50;

/* Estrutura do Monitor */
typedef struct {
    int eating_count;          
    int waiting_to_eat;        
    int waiting_to_leave;      
    int total_students;        
    int finished_students;     
    
    pthread_mutex_t lock;
    pthread_cond_t ok_to_sit;
    pthread_cond_t ok_to_leave;
} DiningMonitor;

/* Globais */
DiningMonitor monitor;
FILE* log_file = NULL;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER; // Mutex exclusivo para o arquivo

/* Protótipos */
void log_event(int id, const char* action, const char* reason);
void random_sleep(void);
void get_food(int id);
void dine(int id);
bool enter_hall(int id); 
void leave_hall(int id);
void student_done(int id);

/* --- Implementação do Logger --- */
void log_event(int id, const char* action, const char* reason) {
    if (log_file == NULL) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    pthread_mutex_lock(&log_lock);
    // Formato: [TIMESTAMP] [STUDENT_ID] ACTION | State | Reason
    fprintf(log_file, "[%ld.%06ld] [Estudante %02d] %-15s | Eat:%d Wait:%d | %s\n",
            tv.tv_sec, (long)tv.tv_usec, 
            id, action, 
            monitor.eating_count, monitor.waiting_to_eat, 
            reason ? reason : "");
    fflush(log_file); // Garante escrita imediata no disco
    pthread_mutex_unlock(&log_lock);
}

/* --- Monitor --- */
void init_monitor(int num_students) {
    monitor.eating_count = 0;
    monitor.waiting_to_eat = 0;
    monitor.waiting_to_leave = 0;
    monitor.total_students = num_students;
    monitor.finished_students = 0;
    pthread_mutex_init(&monitor.lock, NULL);
    pthread_cond_init(&monitor.ok_to_sit, NULL);
    pthread_cond_init(&monitor.ok_to_leave, NULL);
}

void destroy_monitor() {
    pthread_mutex_destroy(&monitor.lock);
    pthread_cond_destroy(&monitor.ok_to_sit);
    pthread_cond_destroy(&monitor.ok_to_leave);
}

bool enter_hall(int id) {
    pthread_mutex_lock(&monitor.lock);
    
    log_event(id, "REQ_ENTRY", "Tentando sentar");
    monitor.waiting_to_eat++;

    while (true) {
        bool can_sit = (monitor.eating_count > 0) || (monitor.waiting_to_eat >= 2);
        
        if (can_sit) break;

        int active_students = monitor.total_students - monitor.finished_students;
        if (monitor.eating_count == 0 && active_students < 2) {
            monitor.waiting_to_eat--;
            log_event(id, "ABORT_ENTRY", "Último sobrevivente detectado");
            pthread_mutex_unlock(&monitor.lock);
            return false; 
        }

        log_event(id, "WAIT_ENTRY", "Aguardando par");
        pthread_cond_wait(&monitor.ok_to_sit, &monitor.lock);
    }

    monitor.waiting_to_eat--;
    monitor.eating_count++;
    
    log_event(id, "ENTERED", "Conseguiu mesa");

    pthread_cond_signal(&monitor.ok_to_sit);
    pthread_mutex_unlock(&monitor.lock);
    return true;
}

void leave_hall(int id) {
    pthread_mutex_lock(&monitor.lock);

    log_event(id, "REQ_LEAVE", "Tentando sair");

    if (monitor.eating_count == 2) {
        monitor.waiting_to_leave++;
        log_event(id, "WAIT_LEAVE", "Esperando par para sair (Barreira)");
        
        while (monitor.waiting_to_leave < 2 && monitor.eating_count == 2) {
            pthread_cond_wait(&monitor.ok_to_leave, &monitor.lock);
        }
        monitor.waiting_to_leave--;
    }

    monitor.eating_count--;
    log_event(id, "LEFT", "Saiu do refeitório");

    pthread_cond_broadcast(&monitor.ok_to_leave);
    pthread_cond_signal(&monitor.ok_to_sit);
    pthread_mutex_unlock(&monitor.lock);
}

void student_done(int id) {
    pthread_mutex_lock(&monitor.lock);
    monitor.finished_students++;
    log_event(id, "FINISHED", "Terminou todas iterações");
    pthread_cond_broadcast(&monitor.ok_to_sit);
    pthread_mutex_unlock(&monitor.lock);
}

/* --- Rotinas --- */
void random_sleep() {
    int ms = MIN_SLEEP_MS + rand() % (MAX_SLEEP_MS - MIN_SLEEP_MS + 1);
    usleep(ms * 1000);
}

void get_food(int id) { 
    log_event(id, "GET_FOOD", "Pegando comida");
    random_sleep(); 
}

void dine(int id) { 
    log_event(id, "EATING", "Comendo");
    random_sleep(); 
}

void* student_routine(void* arg) {
    int id = *(int*)arg;
    free(arg);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        get_food(id);
        if (!enter_hall(id)) break;
        dine(id);
        leave_hall(id);
    }
    student_done(id);
    return NULL;
}

int main(int argc, char* argv[]) {
    srand(time(NULL));

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <num_students>\n", argv[0]);
        return 1;
    }

    // Configuração do Logger via Variável de Ambiente
    char* env_log = getenv("DINING_LOG_FILE");
    if (env_log) {
        log_file = fopen(env_log, "w");
        if (!log_file) {
            perror("Erro ao criar arquivo de log");
            return 1;
        }
        fprintf(log_file, "--- Trace Log Iniciado ---\n");
    }

    const int num_students = atoi(argv[1]);
    pthread_t* students = malloc(sizeof(pthread_t) * num_students);
    init_monitor(num_students);

    for (int i = 0; i < num_students; i++) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&students[i], NULL, student_routine, id);
    }

    for (int i = 0; i < num_students; i++) {
        pthread_join(students[i], NULL);
    }

    if (log_file) {
        fprintf(log_file, "--- Trace Log Finalizado ---\n");
        fclose(log_file);
    }
    
    destroy_monitor();
    free(students);
    pthread_mutex_destroy(&log_lock);
    return 0;
}
