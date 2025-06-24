#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>

// Estructura para representar un proceso
typedef struct {
    pid_t pid;              // ID del proceso
    int tiempo_ejecucion;   // Tiempo total de ejecución necesario (ráfaga)
    int tiempo_restante;    // Tiempo restante de ejecución
    bool en_ejecucion;      // Estado del proceso
    int tiempo_llegada;     // Tiempo de llegada (reloj simulado)
    int tiempo_espera;      // Tiempo acumulado de espera
    int tiempo_retorno;     // Tiempo total de retorno
    int ultimo_encolado;    // Último tiempo en que fue encolado
} Proceso;

// Estructura para el nodo de la cola
typedef struct Nodo {
    Proceso proceso;
    struct Nodo* siguiente;
} Nodo;

// Estructura para la cola
typedef struct {
    Nodo* frente;
    Nodo* final;
} Cola;

// Función para inicializar una cola vacía
void inicializarCola(Cola* cola) {
    cola->frente = cola->final = NULL;
}

// Función para verificar si la cola está vacía
bool colaVacia(Cola* cola) {
    return cola->frente == NULL;
}

// Función para encolar un proceso
void encolar(Cola* cola, Proceso proceso) {
    Nodo* nuevoNodo = (Nodo*)malloc(sizeof(Nodo));
    nuevoNodo->proceso = proceso;
    nuevoNodo->siguiente = NULL;
    if (colaVacia(cola)) {
        cola->frente = cola->final = nuevoNodo;
    } else {
        cola->final->siguiente = nuevoNodo;
        cola->final = nuevoNodo;
    }
}

// Función para desencolar un proceso
Proceso desencolar(Cola* cola) {
    if (colaVacia(cola)) {
        fprintf(stderr, "Error: Cola vacía\n");
        exit(EXIT_FAILURE);
    }
    Nodo* temp = cola->frente;
    Proceso proceso = temp->proceso;
    cola->frente = cola->frente->siguiente;
    if (cola->frente == NULL) {
        cola->final = NULL;
    }
    free(temp);
    return proceso;
}

// Función para detener un proceso
void detenerProceso(Proceso* proceso) {
    if (proceso->en_ejecucion) {
        kill(proceso->pid, SIGSTOP);
        proceso->en_ejecucion = false;
    }
}

// Función para continuar un proceso
void continuarProceso(Proceso* proceso) {
    if (!proceso->en_ejecucion) {
        kill(proceso->pid, SIGCONT);
        proceso->en_ejecucion = true;
    }
}

// Función para simular la ejecución de un proceso
void ejecutarProceso(Proceso* proceso, int tiempo_ejecucion) {
    continuarProceso(proceso);
    printf("Proceso %d ejecutándose por %d unidades (Restante: %d)\n",
           proceso->pid, tiempo_ejecucion, proceso->tiempo_restante);
    sleep(tiempo_ejecucion);  // Simulación del tiempo de ejecución
    detenerProceso(proceso);
}

// Función principal del planificador Round Robin
void roundRobin(Cola* cola, int quantum, int* global_clock, Proceso terminados[], int* terminados_count) {
    printf("\nInicio de la planificación Round Robin (Quantum: %d)\n", quantum);
    printf("Reloj | PID  | Evento\n");
    printf("------+------+------------------------\n");
    
    while (!colaVacia(cola)) {
        Proceso procesoActual = desencolar(cola);
        
        // Calcular tiempo de espera desde último encolado
        int espera_actual = *global_clock - procesoActual.ultimo_encolado;
        procesoActual.tiempo_espera += espera_actual;
        printf("%5d | %4d | Encolado (Espera: %d)\n", 
               *global_clock, procesoActual.pid, espera_actual);
        
        // Calcular tiempo de ejecución para este quantum
        int tiempoEjecucion = (procesoActual.tiempo_restante > quantum) ? quantum : procesoActual.tiempo_restante;
        printf("%5d | %4d | Inicio ejecución (%d u.t.)\n", 
               *global_clock, procesoActual.pid, tiempoEjecucion);
        
        // Ejecutar el proceso
        ejecutarProceso(&procesoActual, tiempoEjecucion);
        
        // Actualizar reloj global
        *global_clock += tiempoEjecucion;
        
        // Actualizar tiempo restante
        procesoActual.tiempo_restante -= tiempoEjecucion;
        
        if (procesoActual.tiempo_restante > 0) {
            // Volver a encolar el proceso
            procesoActual.ultimo_encolado = *global_clock;
            encolar(cola, procesoActual);
            printf("%5d | %4d | Reencolado (Restante: %d)\n", 
                   *global_clock, procesoActual.pid, procesoActual.tiempo_restante);
        } else {
            // Calcular tiempo de retorno
            procesoActual.tiempo_retorno = *global_clock - procesoActual.tiempo_llegada;
            
            // Guardar proceso en lista de terminados
            terminados[*terminados_count] = procesoActual;
            (*terminados_count)++;
            
            printf("%5d | %4d | Terminado (Retorno: %d)\n", 
                   *global_clock, procesoActual.pid, procesoActual.tiempo_retorno);
            kill(procesoActual.pid, SIGTERM);
            waitpid(procesoActual.pid, NULL, 0);
        }
    }
}

// Función que ejecuta cada proceso hijo
void trabajoProcesoHijo() {
    signal(SIGCONT, SIG_DFL);
    signal(SIGSTOP, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    while(1) {
        // Proceso hijo simplemente espera
        pause();
    }
}

int main() {
    Cola cola;
    inicializarCola(&cola);
    
    int num_procesos;
    int quantum;
    int global_clock = 0;  // Reloj global simulado
    int terminados_count = 0;
    
    srand(time(NULL));
    printf("Ingrese el número de procesos: ");
    scanf("%d", &num_procesos);
    printf("Ingrese el quantum de tiempo: ");
    scanf("%d", &quantum);
    
    // Crear arreglo para procesos terminados
    Proceso terminados[num_procesos];

    // Crear los procesos hijos
    for (int i = 0; i < num_procesos; i++) {
        pid_t pid = fork();
        if (pid == 0) { // Código del proceso hijo
            trabajoProcesoHijo();
            exit(0);
        } else if (pid > 0) { // Código del proceso padre
            Proceso p;
            p.pid = pid;
            p.tiempo_ejecucion = 3 + (rand() % 8); // Tiempo de ejecución aleatorio entre 3-10
            p.tiempo_restante = p.tiempo_ejecucion;
            p.en_ejecucion = false;
            p.tiempo_llegada = 0;
            p.tiempo_espera = 0;
            p.ultimo_encolado = 0;
            p.tiempo_retorno = 0;
            
            encolar(&cola, p);
            kill(pid, SIGSTOP); // Detener proceso inmediatamente
            printf("Proceso %d creado (Ráfaga: %d)\n", pid, p.tiempo_ejecucion);
        } else {
            perror("Error al crear proceso");
            exit(EXIT_FAILURE);
        }
    }

    // Ejecutar el planificador Round Robin
    roundRobin(&cola, quantum, &global_clock, terminados, &terminados_count);
    
    // Calcular tiempos promedios
    float espera_promedio = 0, retorno_promedio = 0;
    for (int i = 0; i < terminados_count; i++) {
        espera_promedio += terminados[i].tiempo_espera;
        retorno_promedio += terminados[i].tiempo_retorno;
    }
    espera_promedio /= terminados_count;
    retorno_promedio /= terminados_count;
    
    // Imprimir tabla de procesos
    printf("\n\nTabla de procesos:");
    printf("\nPID\tRáfaga\tEspera\tRetorno");
    printf("\n----\t------\t------\t-------");
    for (int i = 0; i < terminados_count; i++) {
        printf("\n%d\t%d\t%d\t%d", 
               terminados[i].pid, 
               terminados[i].tiempo_ejecucion, 
               terminados[i].tiempo_espera, 
               terminados[i].tiempo_retorno);
    }
    
    // Imprimir resumen
    printf("\n\nResumen:");
    printf("\nTiempo total del sistema: %d u.t.", global_clock);
    printf("\nTiempo de espera promedio: %.2f u.t.", espera_promedio);
    printf("\nTiempo de retorno promedio: %.2f u.t.", retorno_promedio);
    
    printf("\n\nTodos los procesos han terminado\n");
    return 0;
}
