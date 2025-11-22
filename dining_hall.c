/*
 * dining_hall.c (v2.0 - Deadlock Fix)
 * Solução Robusta para o Extended Dining Hall Problem.
 * * Correção: Adicionada lógica para abortar threads "órfãs" quando 
 * não há mais parceiros possíveis (evita Deadlock no final).
 * Authors: Miguel Badany Cerne & Pedro Videira Rubinstein
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

/* Constantes */
const int NUM_ITERATIONS = 20; // Aumentei para testar mais a fundo
const int MIN_SLEEP_MS = 10;   // Reduzi tempos para acelerar teste
const int MAX_SLEEP_MS = 50;

/* Estrutura para o Monitor do Refeitório */
typedef struct {
    int eating_count;          
    int waiting_to_eat;        
    int waiting_to_leave;      
    
    /* NOVOS CAMPOS PARA CONTROLE DE FIM DE JOGO */
    int total_students;        // Total de threads iniciadas
    int finished_students;     // Quantas threads já encerraram o loop principal
    
    pthread_mutex_t lock;
    pthread_cond_t ok_to_sit;
    pthread_cond_t ok_to_leave;
} DiningMonitor;

DiningMonitor monitor;

/* Auxiliares */
void random_sleep(void);
void get_food(int id);
void dine(int id);

/* Core Logic */
bool enter_hall(int id);  // Agora retorna bool (true=sentou, false=abortou)
void leave_hall(int id);
void student_done(void);  // Nova função: avisa que terminou tudo

/* Inicialização */
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

/* * Tenta entrar no refeitório.
 * Retorna: true se conseguiu sentar.
 * Retorna: false se deve abortar (não há mais parceiros).
 */
bool enter_hall(int id) {
    pthread_mutex_lock(&monitor.lock);

    monitor.waiting_to_eat++;

    while (true) {
        // Condição 1: Posso sentar? (Alguém comendo OU tenho par na fila)
        bool can_sit = (monitor.eating_count > 0) || (monitor.waiting_to_eat >= 2);
        
        if (can_sit) {
            break; // Sai do loop de espera e vai comer
        }

        // Condição 2: Devo desistir? (Deadlock prevention)
        // Se (Total - Finalizados) < 2, significa que sou o último (ou somos < 2), 
        // e ninguém está comendo (eating=0). Nunca formarei par.
        int active_students = monitor.total_students - monitor.finished_students;
        if (monitor.eating_count == 0 && active_students < 2) {
            monitor.waiting_to_eat--; // Sai da fila
            pthread_mutex_unlock(&monitor.lock);
            // printf("⚠️  [Estudante %d] Abortando: sem parceiros possíveis.\n", id);
            return false; 
        }

        // Se não posso sentar nem preciso desistir, espero.
        pthread_cond_wait(&monitor.ok_to_sit, &monitor.lock);
    }

    monitor.waiting_to_eat--;
    monitor.eating_count++;

    // printf("🟢 [Estudante %d] SENTOU. (Comendo: %d)\n", id, monitor.eating_count);

    // Acorda o próximo (meu par ou alguém extra)
    pthread_cond_signal(&monitor.ok_to_sit);

    pthread_mutex_unlock(&monitor.lock);
    return true;
}

void leave_hall(int id) {
    pthread_mutex_lock(&monitor.lock);

    if (monitor.eating_count == 2) {
        monitor.waiting_to_leave++;
        while (monitor.waiting_to_leave < 2 && monitor.eating_count == 2) {
            pthread_cond_wait(&monitor.ok_to_leave, &monitor.lock);
        }
        monitor.waiting_to_leave--;
    }

    monitor.eating_count--;
    // printf("🔴 [Estudante %d] SAIU.\n", id);

    pthread_cond_broadcast(&monitor.ok_to_leave);
    pthread_cond_signal(&monitor.ok_to_sit);

    pthread_mutex_unlock(&monitor.lock);
}

/* * Função chamada quando o estudante termina TODAS as iterações.
 * Importante para avisar os que sobraram que "não vem mais ninguém".
 */
void student_done() {
    pthread_mutex_lock(&monitor.lock);
    monitor.finished_students++;
    
    // ACORDA TODOS: Quem estiver esperando em enter_hall precisa acordar
    // para checar a condição de aborto (active_students < 2).
    pthread_cond_broadcast(&monitor.ok_to_sit);
    
    pthread_mutex_unlock(&monitor.lock);
}

void random_sleep() {
    int ms = MIN_SLEEP_MS + rand() % (MAX_SLEEP_MS - MIN_SLEEP_MS + 1);
    usleep(ms * 1000);
}

void get_food(int id) { random_sleep(); }
void dine(int id) { random_sleep(); }

void* student_routine(void* arg) {
    int id = *(int*)arg;
    free(arg);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        get_food(id);
        
        // Tenta entrar. Se retornar false, aborta o loop inteiro.
        if (!enter_hall(id)) {
            break; 
        }
        
        dine(id);
        leave_hall(id);
    }

    // Marca presença como finalizado antes de morrer
    student_done();
    return NULL;
}

int main(int argc, char* argv[]) {
    srand(time(NULL));

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <numero_estudantes>\n", argv[0]);
        return 1;
    }

    const int num_students = atoi(argv[1]);
    if (num_students < 2) {
        fprintf(stderr, "Erro: Minimo 2 estudantes.\n");
        return 1;
    }

    pthread_t* students = malloc(sizeof(pthread_t) * num_students);
    init_monitor(num_students); // Passamos o total para o monitor

    // printf("--- Iniciando com %d estudantes ---\n", num_students);

    for (int i = 0; i < num_students; i++) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&students[i], NULL, student_routine, id);
    }

    for (int i = 0; i < num_students; i++) {
        pthread_join(students[i], NULL);
    }

    // printf("--- Fim da Simulação ---\n");
    
    destroy_monitor();
    free(students);
    return 0;
}
