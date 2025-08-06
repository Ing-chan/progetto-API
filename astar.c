#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#define MAX_AIR_ROUTES 5
#define INF INT_MAX

//per accedere con la riga ribaltata in basso
#define ACCESSO(x, y) mappa[righe - 1 - (x)][y] 

typedef struct {
    int dest_x, dest_y; //coordinate destinazione
    int cost; 
} AirRoute;

typedef struct {
    int cost; //0 senza uscita, 100 max
    AirRoute* rotte;
    int air_count; //max 5 elementi
} Esagono;

typedef struct {
    int x, y;
    int g_score;  // costo reale dalla sorgente
    int f_score;  // g_score + euristica
} HeapNode;

typedef struct {
    HeapNode* nodes;
    int size;
    int capacity;
} MinHeap;

//globali perché devono accederci le altre funzioni
unsigned int colonne, righe; 
Esagono** mappa = NULL;

//dichiarazione funzioni
void init(unsigned int colonne, unsigned int righe);
void change_cost(unsigned int x, unsigned int y, int v, unsigned int raggio);
void offset_to_cube(int x, int y, int* cube_x, int* cube_y, int* cube_z);
int distanza_esagoni(int x1, int y1, int x2, int y2);
bool coordinate_valide(unsigned int x, unsigned int y);
void toggle_air_route(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
void travel_cost(unsigned int xp, unsigned int yp, unsigned int xd, unsigned int yd);

// Funzioni per MinHeap e A*
MinHeap* create_heap(int capacity);
void heap_push_astar(MinHeap* heap, int x, int y, int g_score, int f_score);
HeapNode heap_pop(MinHeap* heap);
bool heap_empty(MinHeap* heap);
void free_heap(MinHeap* heap);
void heapify_up(MinHeap* heap, int idx);
void heapify_down(MinHeap* heap, int idx);
int get_vicini_terrestri(int x, int y, int vicini[][2]);
int astar(int start_x, int start_y, int end_x, int end_y);

int main() {
    char riga[80];       // buffer sufficiente per una riga di comando
    char comando[20];    // spazio per il comando

    unsigned int x, y, raggio;
    int v; // può essere negativo in [-10,10]
    unsigned int x1, y1, x2, y2;
    unsigned int xp, yp, xd, yd;

    while (fgets(riga, sizeof(riga), stdin)) {

        sscanf(riga, "%19s", comando);  // legge il nome del comando

        if (strncmp(comando, "init", 4) == 0) {
            sscanf(riga, "%*s %u %u", &colonne, &righe);
            init(colonne, righe);
        }
        else if (strncmp(comando, "change_cost", 11) == 0) {
            sscanf(riga, "%*s %u %u %d %u", &x, &y, &v, &raggio);
            change_cost(x, y, v, raggio);
        }
        else if (strncmp(comando, "toggle_air_route", 16) == 0) {
            sscanf(riga, "%*s %u %u %u %u", &x1, &y1, &x2, &y2);
            toggle_air_route(x1, y1, x2, y2);
        }
        else if (strncmp(comando, "travel_cost", 11) == 0) {
            sscanf(riga, "%*s %u %u %u %u", &xp, &yp, &xd, &yd);
            travel_cost(xp, yp, xd, yd);
        }
    }
    return 0;
}

void init(unsigned int colonne, unsigned int righe){

    //cancella se già esistente
    if (mappa != NULL) {
        
        for (int x = 0; x < righe; x++) {
            for (int y = 0; y < colonne; y++) {
                if (ACCESSO(x, y).rotte != NULL) {
                    free(ACCESSO(x, y).rotte); //perché mi da segmentation fault?
                }
            }
            free(mappa[x]);
        }

        free(mappa);
        mappa = NULL;
    }

    //alloca spazio per le righe
    mappa = malloc(righe * sizeof(Esagono*));

    //alloca riga per riga le colonne
    for (int x = 0; x < righe; x++) {

        //alloca spazio per le colonne
        mappa[x] = malloc(colonne * sizeof(Esagono));

        for (int y = 0; y < colonne; y++) {
            mappa[x][y].cost = 1;
            mappa[x][y].air_count = 0;
            mappa[x][y].rotte = NULL;
        }
    }

    //messaggio di risposta
    printf("OK\n");
}

// Conversione da coordinate offset a coordinate cubiche
void offset_to_cube(int x, int y, int* cube_x, int* cube_y, int* cube_z) {
    *cube_x = y - (x - (x & 1)) / 2;
    *cube_z = x;
    *cube_y = -(*cube_x) - (*cube_z);
}

// Distanza tra esagoni usando coordinate cubiche
int distanza_esagoni(int x1, int y1, int x2, int y2) {
    int cube_x1, cube_y1, cube_z1;
    int cube_x2, cube_y2, cube_z2;
    
    offset_to_cube(x1, y1, &cube_x1, &cube_y1, &cube_z1);
    offset_to_cube(x2, y2, &cube_x2, &cube_y2, &cube_z2);
    
    return (abs(cube_x1 - cube_x2) + abs(cube_y1 - cube_y2) + abs(cube_z1 - cube_z2)) / 2;
}

bool coordinate_valide(unsigned int x, unsigned int y) {
    return x < righe && y < colonne;
}

void change_cost(unsigned int x, unsigned int y, int v, unsigned int raggio){

    //esagono non valido o raggio nullo o v non in range
    if(!coordinate_valide(x, y) || raggio==0 || v<-10 || v>10){
        printf("KO\n");
        return;
    }
    
    //trova la distanza di tutti gli esagoni dal corrente
    //possibilmente da ottimizzare per restringere la ricerca nel raggio
    //devo cambiare i costi di tutti quelli in +/-(r-1)!!
    for (int i = 0; i < righe; i++) {
        for (int j = 0; j < colonne; j++) {

            int dist = distanza_esagoni(x, y, i, j);

            // Applica cambiamento a tutti gli esagoni nel raggio
            if (dist < raggio) {
                // Calcola nuovo costo secondo la formula
                int incremento = v * (raggio - dist) / raggio;
                int nuovo_costo = ACCESSO(i, j).cost + incremento;
                
                // Limita tra 0 e 100
                if (nuovo_costo < 0) nuovo_costo = 0;
                if (nuovo_costo > 100) nuovo_costo = 100;
                
                ACCESSO(i, j).cost = nuovo_costo;
                
                // Aggiorna anche i costi delle rotte aeree uscenti
                for (int k = 0; k < ACCESSO(i, j).air_count; k++) {
                    // Ricalcola il costo della rotta aerea con la nuova formula
                    int somma_costi = ACCESSO(i, j).cost;
                    for (int m = 0; m < ACCESSO(i, j).air_count; m++) {
                        somma_costi += ACCESSO(i, j).rotte[m].cost;
                    }
                    int nuovo_costo_rotta = somma_costi / (ACCESSO(i, j).air_count + 1);
                    if (nuovo_costo_rotta > 100) nuovo_costo_rotta = 100;
                    ACCESSO(i, j).rotte[k].cost = nuovo_costo_rotta;
                }
            }
        }
    }
    printf("OK\n");
}

void toggle_air_route(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2) {
    // Validazione coordinate
    if (!coordinate_valide(x1, y1) || !coordinate_valide(x2, y2)) {
        printf("KO\n");
        return;
    }
    
    // Cerca se la rotta esiste già
    bool rotta_esistente = false;
    int indice_rotta = -1;
    
    for (int i = 0; i < ACCESSO(x1, y1).air_count; i++) {
        if (ACCESSO(x1, y1).rotte[i].dest_x == x2 && 
            ACCESSO(x1, y1).rotte[i].dest_y == y2) {
            rotta_esistente = true;
            indice_rotta = i;
            break;  // da sostituire con un flag se non piace al prof
        }
    }
    
    if (rotta_esistente) {
        // Rimuovi rotta esistente
        for (int i = indice_rotta; i < ACCESSO(x1, y1).air_count - 1; i++) {
            ACCESSO(x1, y1).rotte[i] = ACCESSO(x1, y1).rotte[i + 1];
        }
        ACCESSO(x1, y1).air_count--;
        
        if (ACCESSO(x1, y1).air_count == 0) {
            free(ACCESSO(x1, y1).rotte);
            ACCESSO(x1, y1).rotte = NULL;
        }else {
            // Rialloca per risparmiare memoria
            ACCESSO(x1, y1).rotte = realloc(ACCESSO(x1, y1).rotte, ACCESSO(x1, y1).air_count * sizeof(AirRoute));
        }
    } else {
        // Aggiungi nuova rotta
        if (ACCESSO(x1, y1).air_count >= MAX_AIR_ROUTES) {
            printf("KO\n");
            return;
        }
        
        // Calcola costo della nuova rotta (media dei costi esistenti + costo esagono)
        int somma_costi = ACCESSO(x1, y1).cost;
        for (int i = 0; i < ACCESSO(x1, y1).air_count; i++) {
            somma_costi += ACCESSO(x1, y1).rotte[i].cost;
        }
        int nuovo_costo = somma_costi / (ACCESSO(x1, y1).air_count + 1);
        
        // Limita costo tra 0 e 100
        if (nuovo_costo > 100) nuovo_costo = 100;
        
        // Rialloca memoria per le rotte
        ACCESSO(x1, y1).rotte = realloc(ACCESSO(x1, y1).rotte,(ACCESSO(x1, y1).air_count + 1) * sizeof(AirRoute));
        
        // Aggiungi nuova rotta
        ACCESSO(x1, y1).rotte[ACCESSO(x1, y1).air_count].dest_x = x2;
        ACCESSO(x1, y1).rotte[ACCESSO(x1, y1).air_count].dest_y = y2;
        ACCESSO(x1, y1).rotte[ACCESSO(x1, y1).air_count].cost = nuovo_costo;
        ACCESSO(x1, y1).air_count++;

        printf("OK\n");
    }
    
    
}

// Calcola i vicini terrestri di un esagono
int get_vicini_terrestri(int x, int y, int vicini[][2]) {
    // Pattern per righe pari e dispari
    int direzioni_pari[6][2] = {{0, -1}, {1, -1}, {1, 0}, {0, 1}, {-1, 0}, {-1, -1}};
    int direzioni_dispari[6][2] = {{0, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}};
    
    int (*direzioni)[2] = (x % 2 == 0) ? direzioni_pari : direzioni_dispari;
    int count = 0;
    
    for (int i = 0; i < 6; i++) {
        int nx = x + direzioni[i][0];
        int ny = y + direzioni[i][1];
        
        if (coordinate_valide(nx, ny)) {
            vicini[count][0] = nx;
            vicini[count][1] = ny;
            count++;
        }
    }
    return count;
}

// Implementazione MinHeap per A*
MinHeap* create_heap(int capacity) {
    MinHeap* heap = malloc(sizeof(MinHeap));
    heap->nodes = malloc(capacity * sizeof(HeapNode));
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

void heap_push_astar(MinHeap* heap, int x, int y, int g_score, int f_score) {
    if (heap->size >= heap->capacity) {
        heap->capacity *= 2;
        heap->nodes = realloc(heap->nodes, heap->capacity * sizeof(HeapNode));
    }
    
    int idx = heap->size;
    heap->nodes[idx] = (HeapNode){x, y, g_score, f_score};
    heap->size++;
    heapify_up(heap, idx);
}

HeapNode heap_pop(MinHeap* heap) {
    HeapNode min = heap->nodes[0];
    heap->size--;
    if (heap->size > 0) {
        heap->nodes[0] = heap->nodes[heap->size];
        heapify_down(heap, 0);
    }
    return min;
}

bool heap_empty(MinHeap* heap) {
    return heap->size == 0;
}

void free_heap(MinHeap* heap) {
    free(heap->nodes);
    free(heap);
}

void heapify_up(MinHeap* heap, int idx) {
    int parent = (idx - 1) / 2;
    if (idx > 0 && heap->nodes[idx].f_score < heap->nodes[parent].f_score) {
        HeapNode temp = heap->nodes[idx];
        heap->nodes[idx] = heap->nodes[parent];
        heap->nodes[parent] = temp;
        heapify_up(heap, parent);
    }
}

void heapify_down(MinHeap* heap, int idx) {
    int left = 2 * idx + 1;
    int right = 2 * idx + 2;
    int smallest = idx;
    
    if (left < heap->size && heap->nodes[left].f_score < heap->nodes[smallest].f_score)
        smallest = left;
    if (right < heap->size && heap->nodes[right].f_score < heap->nodes[smallest].f_score)
        smallest = right;
    
    if (smallest != idx) {
        HeapNode temp = heap->nodes[idx];
        heap->nodes[idx] = heap->nodes[smallest];
        heap->nodes[smallest] = temp;
        heapify_down(heap, smallest);
    }
}

// Algoritmo A*
int astar(int start_x, int start_y, int end_x, int end_y) {
    // Array per g_score e visited
    int** g_score = malloc(righe * sizeof(int*));
    bool** visited = malloc(righe * sizeof(bool*));
    
    for (int i = 0; i < righe; i++) {
        g_score[i] = malloc(colonne * sizeof(int));
        visited[i] = malloc(colonne * sizeof(bool));
        for (int j = 0; j < colonne; j++) {
            g_score[i][j] = INF;
            visited[i][j] = false;
        }
    }
    
    // Inizializza sorgente
    g_score[righe - 1 - start_x][start_y] = 0;
    
    MinHeap* heap = create_heap(righe * colonne);
    int h_start = distanza_esagoni(start_x, start_y, end_x, end_y);
    heap_push_astar(heap, start_x, start_y, 0, h_start);
    
    while (!heap_empty(heap)) {
        HeapNode current = heap_pop(heap);
        int x = current.x, y = current.y;
        
        if (visited[righe - 1 - x][y]) continue;
        visited[righe - 1 - x][y] = true;
        
        // TROVATO IL TARGET!
        if (x == end_x && y == end_y) {
            int result = g_score[righe - 1 - x][y];
            
            // Libera memoria
            for (int i = 0; i < righe; i++) {
                free(g_score[i]);
                free(visited[i]);
            }
            free(g_score);
            free(visited);
            free_heap(heap);
            
            return result;
        }
        
        // Esplora vicini terrestri
        int vicini[6][2];
        int num_vicini = get_vicini_terrestri(x, y, vicini);
        
        for (int i = 0; i < num_vicini; i++) {
            int nx = vicini[i][0], ny = vicini[i][1];
            
            if (ACCESSO(x, y).cost == 0 || visited[righe - 1 - nx][ny]) continue;
            
            int tentative_g = g_score[righe - 1 - x][y] + ACCESSO(x, y).cost;
            
            if (tentative_g < g_score[righe - 1 - nx][ny]) {
                g_score[righe - 1 - nx][ny] = tentative_g;
                int h = distanza_esagoni(nx, ny, end_x, end_y);
                int f = tentative_g + h;
                heap_push_astar(heap, nx, ny, tentative_g, f);
            }
        }
        
        // Esplora rotte aeree
        for (int i = 0; i < ACCESSO(x, y).air_count; i++) {
            int nx = ACCESSO(x, y).rotte[i].dest_x;
            int ny = ACCESSO(x, y).rotte[i].dest_y;
            int air_cost = ACCESSO(x, y).rotte[i].cost;
            
            if (air_cost == 0 || visited[righe - 1 - nx][ny]) continue;
            
            int tentative_g = g_score[righe - 1 - x][y] + air_cost;
            
            if (tentative_g < g_score[righe - 1 - nx][ny]) {
                g_score[righe - 1 - nx][ny] = tentative_g;
                int h = distanza_esagoni(nx, ny, end_x, end_y);
                int f = tentative_g + h;
                heap_push_astar(heap, nx, ny, tentative_g, f);
            }
        }
    }
    
    // Non trovato
    for (int i = 0; i < righe; i++) {
        free(g_score[i]);
        free(visited[i]);
    }
    free(g_score);
    free(visited);
    free_heap(heap);
    
    return -1;
}

void travel_cost(unsigned int xp, unsigned int yp, unsigned int xd, unsigned int yd) {
    // Validazione coordinate
    if (!coordinate_valide(xp, yp) || !coordinate_valide(xd, yd)) {
        printf("-1\n");
        return;
    }
    
    // Caso particolare: stesso esagono
    if (xp == xd && yp == yd) {
        printf("0\n");
        return;
    }

    int result = astar(xp, yp, xd, yd);
    printf("%d\n", result);
}

/*
PROBEMI ATTUALI: il programma non si interrompe ma:
 AL MOMENTO LE ROTTE VALIDE MA NON RAGGIUNGIBILI DANNO 100 INVECE DI -1!!!
toggle-air-route NON RITORNA NULLA nel caso in cui RIMUOVE UNA ROTTA

UNICO CASO IN CUI SI INTERROMPE: DURANTE UNA INIT() NEL CASO CHE NE ESISTE GIÀ UNA. SI FERMA A free(ACCESSO(x, y).rotte); E DA ABORTION O SEGMENTATION FAULT. PERCHÉ??
*/