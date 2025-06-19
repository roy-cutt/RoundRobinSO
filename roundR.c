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
   int tiempo_ejecucion;   // Tiempo total de ejecución necesario
   int tiempo_restante;    // Tiempo restante de ejecución
   bool en_ejecucion;      // Estado del proceso
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
       printf("Proceso %d detenido\n", proceso->pid);
   }
}
// Función para continuar un proceso
void continuarProceso(Proceso* proceso) {
   if (!proceso->en_ejecucion) {
       kill(proceso->pid, SIGCONT);
       proceso->en_ejecucion = true;
       printf("Proceso %d continuado\n", proceso->pid);
   }
}
// Función para simular la ejecución de un proceso
void ejecutarProceso(Proceso* proceso, int quantum) {
   continuarProceso(proceso);
   printf("Proceso %d ejecutándose por %d unidades de tiempo (Restante: %d)\n",
          proceso->pid, quantum, proceso->tiempo_restante);
   // Simulamos el tiempo de ejecución
   sleep(quantum);
   // Actualizamos el tiempo restante
   proceso->tiempo_restante -= quantum;
   if (proceso->tiempo_restante < 0) {
       proceso->tiempo_restante = 0;
   }
   detenerProceso(proceso);
}
// Función principal del planificador Round Robin
void roundRobin(Cola* cola, int quantum) {
   while (!colaVacia(cola)) {
       Proceso procesoActual = desencolar(cola);
       if (procesoActual.tiempo_restante > 0) {
           // Ejecutar el proceso por el quantum o hasta que termine
           int tiempoEjecucion = (procesoActual.tiempo_restante > quantum) ? quantum : procesoActual.tiempo_restante;
           ejecutarProceso(&procesoActual, tiempoEjecucion);
           // Si todavía le queda tiempo, volver a encolarlo
           if (procesoActual.tiempo_restante > 0) {
               encolar(cola, procesoActual);
           } else {
               printf("Proceso %d ha terminado\n", procesoActual.pid);
               kill(procesoActual.pid, SIGTERM); // Terminar el proceso
               waitpid(procesoActual.pid, NULL, 0); // Esperar a que el proceso hijo termine
           }
       }
   }
}
// Función que ejecuta cada proceso hijo
void trabajoProcesoHijo() {
   // Configurar manejo de señales
   signal(SIGCONT, SIG_DFL);
   signal(SIGSTOP, SIG_DFL);
   signal(SIGTERM, SIG_DFL);
   while(1) {
       printf("Proceso %d trabajando...\n", getpid());
       sleep(1); // Trabajo simulado
   }
}
int main() {
   Cola cola;
   inicializarCola(&cola);
   int num_procesos;
   int quantum;
   srand(time(NULL)); // Semilla para números aleatorios
   printf("Ingrese el número de procesos: ");
   scanf("%d", &num_procesos);
   printf("Ingrese el quantum de tiempo: ");
   scanf("%d", &quantum);
   // Crear los procesos hijos
   for (int i = 0; i < num_procesos; i++) {
       pid_t pid = fork();
       if (pid == 0) { // Código del proceso hijo
           trabajoProcesoHijo();
           exit(0);
       } else if (pid > 0) { // Código del proceso padre
           // Crear un proceso para la cola
           Proceso p;
           p.pid = pid;
           p.tiempo_ejecucion = 3 + (rand() % 8); // Tiempo de ejecución aleatorio entre 3 y 10
           p.tiempo_restante = p.tiempo_ejecucion;
           p.en_ejecucion = false;
           encolar(&cola, p);
           // Inicialmente detenemos el proceso
           kill(pid, SIGSTOP);
       } else {
           perror("Error al crear proceso");
           exit(EXIT_FAILURE);
       }
   }
   // Ejecutar el planificador Round Robin
   roundRobin(&cola, quantum);
   printf("Todos los procesos han terminado\n");
   return 0;
}