#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#define MAX_AIR_ROUTES 5
#define INF INT_MAX

//scambo colonna riga
#define ACCESSO(x, y) mappa[y][x]

//struct per la mappa
typedef struct {
    int dest_x, dest_y; //coordinate destinazione
    int cost; 
} AirRoute;

typedef struct {
    int cost; //0 senza uscita, 100 max
    AirRoute* rotte;
    int air_count; //max 5 elementi
} Esagono;

//struct per dijkstra
typedef struct {
    int x, y;
    int g_score;
} HeapNode;

typedef struct {
    HeapNode* nodes;
    int size;
    int capacity;
} MinHeap;

//struct per la cache
typedef struct {
    unsigned int src_x, src_y, dst_x, dst_y;
    int cost;
    bool valid;
} CacheEntry;

typedef struct {
    CacheEntry* entries;
    int capacity;
    int size;
} CachePercorsi;

//globali perché devono accederci le altre funzioni
unsigned int colonne, righe; 
Esagono** mappa = NULL;

//globali per la cache
CachePercorsi* cache = NULL;

//globali per ottimizzare
int** g_score = NULL;
int direzioni_pari[6][2] = {
    {+1,  0}, { 0, -1}, {-1, -1},
    {-1,  0}, {-1, +1}, { 0, +1}
};
int direzioni_dispari[6][2] = {
    {+1,  0}, {+1, -1}, { 0, -1},
    {-1,  0}, { 0, +1}, {+1, +1}
};


//dichiarazione funzioni
void init(unsigned int nuove_colonne, unsigned int nuove_righe);
void change_cost(unsigned int x1, unsigned int y1, int v, unsigned int raggio);
int distanza_esagoni(int x1, int y1, int x2, int y2);
bool coordinate_valide(unsigned int x, unsigned int y);
void toggle_air_route(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
void travel_cost(unsigned int xp, unsigned int yp, unsigned int xd, unsigned int yd);

//funzioni per MinHeap e dijkstra
MinHeap* create_heap(int capacity);
void heap_push(MinHeap* heap, int x, int y, int g_score);
HeapNode heap_pop(MinHeap* heap);
bool heap_empty(MinHeap* heap);
void free_heap(MinHeap* heap);
void heapify_up(MinHeap* heap, int idx);
void heapify_down(MinHeap* heap, int idx);
int get_vicini_terrestri(int x, int y, int vicini[][2]);
int dijkstra(int start_x, int start_y, int end_x, int end_y);

//funzioni per la cache per velocizzare i travel_cost
CachePercorsi* crea_cache(int capacity);
void metti_cache(unsigned int src_x, unsigned int src_y, unsigned int dst_x, unsigned int dst_y, int cost);
int cache_lookup(unsigned int src_x, unsigned int src_y, unsigned int dst_x, unsigned int dst_y);
unsigned int hash_coordinate(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, int capacity);
void cancella_cache();

//inizio main
int main() {
    char riga[80];       // buffer sufficiente per una riga di comando
    char comando[20];    // spazio per il comando

    unsigned int x, y, raggio;
    int v; // può essere negativo in [-10,10]
    unsigned int x1, y1, x2, y2;
    unsigned int xp, yp, xd, yd;

    while (fgets(riga, sizeof(riga), stdin)) {

        sscanf(riga, "%19s", comando);  //legge il nome del comando

        if (strncmp(comando, "init", 4) == 0) {
            unsigned int nuove_colonne, nuove_righe;
            sscanf(riga, "%*s %u %u", &nuove_colonne, &nuove_righe);
            init(nuove_colonne, nuove_righe);  //le globali hanno ancora i vecchi valori quindi no segmentation fault
            //aggiorna i globali dopo la init
            righe = nuove_righe;
            colonne = nuove_colonne;
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

    //libera tutto alla fine
    for (int x = 0; x < righe; x++) {
        for (int y = 0; y < colonne; y++) {
            if (mappa[x][y].rotte != NULL) {
                free(mappa[x][y].rotte);
            }
        }
        free(mappa[x]);
    }
    free(mappa);
    mappa = NULL;

    if (cache != NULL) {
        free(cache->entries);
        free(cache);
        cache = NULL;
    }

    if (g_score != NULL) {
        for (int i = 0; i < righe; i++) {
            free(g_score[i]);
        }
        free(g_score);
        g_score = NULL;
    }

    return 0;
}

void init(unsigned int nuove_colonne, unsigned int nuove_righe){

    //cancella se già esistente
    if (mappa != NULL) {
        
        for (int x = 0; x < righe; x++) {
            for (int y = 0; y < colonne; y++) {
                if (mappa[x][y].rotte != NULL) {
                    free(mappa[x][y].rotte);
                }
            }
            free(mappa[x]);
        }

        free(mappa);
        mappa = NULL;
    }

    //se esiste va cancellato perché nuova mappa
    if (g_score != NULL) {
        for (int i = 0; i < righe; i++) {
            free(g_score[i]);
        }
        free(g_score);
        g_score = NULL;
    }


    //alloca spazio per le righe
    mappa = malloc(nuove_righe * sizeof(Esagono*));

    //alloca riga per riga le colonne
    for (int x = 0; x < nuove_righe; x++) {

        //alloca spazio per le colonne
        mappa[x] = malloc(nuove_colonne * sizeof(Esagono));

        for (int y = 0; y < nuove_colonne; y++) {
            mappa[x][y].cost = 1;
            mappa[x][y].air_count = 0;
            mappa[x][y].rotte = NULL;
        }
    }

    //qua g_score per ottimizzare
    g_score = malloc(nuove_righe * sizeof(int*));
    for (int i = 0; i < nuove_righe; i++) {
        g_score[i] = malloc(nuove_colonne * sizeof(int));
    }
    
    //messaggio di risposta
    printf("OK\n");
}

//distanza tra esagoni usando coordinate cubiche
int distanza_esagoni(int x1, int y1, int x2, int y2) {

    int s1 = x1 - ( (y1 - (y1 % 2 == 1 ? 1 : 0)) / 2 );
    int z1 = y1;
    int t1 = -s1 - z1;

    int s2 = x2 - ( (y2 - (y2 % 2 == 1 ? 1 : 0)) / 2 );
    int z2 = y2;
    int t2 = -s2 - z2;

    // Calcolo differenze
    int dx = abs(s1 - s2);
    int dy = abs(t1 - t2);
    int dz = abs(z1 - z2);

    return (dx + dy + dz) / 2;
}

bool coordinate_valide(unsigned int x, unsigned int y) {
    return x < colonne && y < righe;
}

void change_cost(unsigned int x, unsigned int y, int v, unsigned int raggio){
    
    //esagono non valido o raggio nullo o v non in range
    if(!coordinate_valide(x, y) || raggio==0 || v<-10 || v>10){
        printf("KO\n");
        return;
    }

    //bounding box per evitare di iterare su tutta la mappa
    int min_i = (int)y - raggio;
    int max_i = (int)y + raggio;
    int min_j = (int)x - raggio;
    int max_j = (int)x + raggio;

    //taglio ai confini
    if (min_i < 0) min_i = 0;
    if (max_i >= righe) max_i = righe;
    if (min_j < 0) min_j = 0;
    if (max_j >= colonne) max_j = colonne;

    //trova la distanza dal corrente
    for (int i = min_i; i < max_i; i++) {
        for (int j = min_j; j < max_j; j++) {

            int dist = distanza_esagoni(x,y, j, i); 

            //applica cambiamento a tutti gli esagoni nel raggio
            if (dist < raggio) {

                //calcola nuovo costo via formula, usando approssimazione per difetto
                float delta = (float) (raggio - dist) / raggio;
                int incremento;
                if (delta < 0){
                    delta = 0;
                    incremento = 0;
                }else {
                    incremento = (int) floorf((float) v * delta);
                }
                
                int nuovo_costo = ACCESSO(j,i).cost + incremento;

                //limita tra 0 e 100
                if (nuovo_costo > 100) nuovo_costo = 100;
                if (nuovo_costo < 0) nuovo_costo = 0;
                
                ACCESSO(j,i).cost = nuovo_costo;
                
                //aggiorna se ci sono i costi delle rotte aeree uscenti
                if(ACCESSO(j,i).air_count > 0){
                for (int k = 0; k < ACCESSO(j,i).air_count; k++) {

                    //aggiunge anche alle aeree il delta
                    int nuovo_costo_rotta = ACCESSO(j,i).rotte[k].cost + incremento;
                    
                    //check per il max
                    if (nuovo_costo_rotta > 100) nuovo_costo_rotta = 100;
                    if (nuovo_costo_rotta < 0) nuovo_costo_rotta = 0;

                    ACCESSO(j,i).rotte[k].cost = nuovo_costo_rotta;
                }
                }
            }
        }
    }

    //cambio con successo
    printf("OK\n");

    //cancella la cache perché cambiata
    cancella_cache();
}

void toggle_air_route(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2) {

    //validazione coordinate
    if (!coordinate_valide(x1, y1) || !coordinate_valide(x2, y2)) {
        printf("KO\n");
        return;
    }

    //check per vedere il numero
    if (ACCESSO(x1, y1).air_count >= MAX_AIR_ROUTES) {
        printf("KO\n");
        return;
    }
    
    //cerca se la rotta esiste già
    bool rotta_esistente = false;
    int indice_rotta = -1;
    
    for (int i = 0; i < ACCESSO(x1, y1).air_count && !rotta_esistente; i++) {
        if (ACCESSO(x1, y1).rotte[i].dest_x == x2 && 
            ACCESSO(x1, y1).rotte[i].dest_y == y2) {
            indice_rotta = i;
            rotta_esistente = true;
        }
    }
    
    if (rotta_esistente) {//rimuovi rotta esistente

        for (int i = indice_rotta; i < ACCESSO(x1, y1).air_count - 1; i++) {
            ACCESSO(x1, y1).rotte[i] = ACCESSO(x1, y1).rotte[i + 1];
        }
        ACCESSO(x1, y1).air_count--;
        
        if (ACCESSO(x1, y1).air_count == 0) {
            free(ACCESSO(x1, y1).rotte);
            ACCESSO(x1, y1).rotte = NULL;
        }else {
            //rialloca per risparmiare memoria
            ACCESSO(x1, y1).rotte = realloc(ACCESSO(x1, y1).rotte, ACCESSO(x1, y1).air_count * sizeof(AirRoute));
        }
    } else {//aggiungi nuova rotta

        //calcola costo della nuova rotta (media dei costi esistenti + costo esagono)
        int somma_costi = ACCESSO(x1, y1).cost;
        for (int i = 0; i < ACCESSO(x1, y1).air_count; i++) {
            somma_costi += ACCESSO(x1, y1).rotte[i].cost;
        }
        int nuovo_costo = (int) floorf((float) somma_costi / (ACCESSO(x1, y1).air_count + 1)); //anche qua appross per difetto con la mia ad hoc
        
        //limita costo tra 0 e 100
        if (nuovo_costo > 100) nuovo_costo = 100;
        if (nuovo_costo < 0) nuovo_costo = 0;
        
        //rialloca memoria per le rotte
        ACCESSO(x1, y1).rotte = realloc(ACCESSO(x1, y1).rotte,(ACCESSO(x1, y1).air_count + 1) * sizeof(AirRoute));
        
        //aggiungi nuova rotta
        ACCESSO(x1, y1).rotte[ACCESSO(x1, y1).air_count].dest_x = x2;
        ACCESSO(x1, y1).rotte[ACCESSO(x1, y1).air_count].dest_y = y2;
        ACCESSO(x1, y1).rotte[ACCESSO(x1, y1).air_count].cost = nuovo_costo;
        ACCESSO(x1, y1).air_count++;
        
    }
    //rotta trovata e rimossa oppure rotta aggiunta
    printf("OK\n");

    //cancella la cache perché modificata
    cancella_cache();
}

//calcola i vicini terrestri di un esagono
int get_vicini_terrestri(int x, int y, int vicini[][2]) { 

    //pattern per righe pari e dispari
    //posizioni: bassosx, sx, altosx, altodx, dx, bassodx
    //veccio ordine: int direzioni_pari[6][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 0}, {0, -1}};

    //sceglie in base a riga
    int (*direzioni)[2] = ((y) % 2 == 0) ? direzioni_pari : direzioni_dispari;
    int count = 0;

    for (int i = 0; i < 6; i++) {
        int nx = x + direzioni[i][0];
        int ny = y + direzioni[i][1];

        if (nx >= 0 && nx < colonne && ny >= 0 && ny < righe) {
            vicini[count][0] = nx;
            vicini[count][1] = ny;
            count++;
        }
    }
    return count;
}

//minheap e strutture per dijkstra
MinHeap* create_heap(int capacity) {
    MinHeap* heap = malloc(sizeof(MinHeap));
    heap->nodes = malloc(capacity * sizeof(HeapNode));
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

void heap_push(MinHeap* heap, int x, int y, int g_score) {
    if (heap->size >= heap->capacity) {
        heap->capacity *= 2;
        heap->nodes = realloc(heap->nodes, heap->capacity * sizeof(HeapNode));
    }
    
    int idx = heap->size;
    heap->nodes[idx] = (HeapNode){x, y, g_score};
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

//funzione per tie-breaking deterministico
bool better_node(HeapNode* a, HeapNode* b) {

    //normale via g
    if (a->g_score != b->g_score) {
        return a->g_score < b->g_score;
    }

    return a->y < b->y; //altrimenti vado di y
}

void heapify_up(MinHeap* heap, int idx) {
    int parent = (idx - 1) / 2;
    if (idx > 0 && better_node(&heap->nodes[idx], &heap->nodes[parent])) {
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
    
    if (left < heap->size && better_node(&heap->nodes[left], &heap->nodes[smallest]))
        smallest = left;
    if (right < heap->size && better_node(&heap->nodes[right], &heap->nodes[smallest]))
        smallest = right;
    
    if (smallest != idx) {
        HeapNode temp = heap->nodes[idx];
        heap->nodes[idx] = heap->nodes[smallest];
        heap->nodes[smallest] = temp;
        heapify_down(heap, smallest);
    }
}

//funzioni della cache
CachePercorsi* crea_cache(int capacity) {
    CachePercorsi* c = malloc(sizeof(CachePercorsi));
    c->entries = malloc(capacity * sizeof(CacheEntry));
    c->capacity = capacity;
    c->size = 0;
    
    //inizializza tutte le entry come non valide
    for (int i = 0; i < capacity; i++) {
        c->entries[i].valid = false;
    }
    
    return c;
}

//combina le 4 coordinate in un hash
unsigned int hash_coordinate(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2, int capacity) {
    unsigned int hash = x1 * 73856093 + y1 * 19349663 + x2 * 83492791 + y2 * 50331653;
    return hash % capacity;
}

//ricerca nella cache
int cache_lookup(unsigned int src_x, unsigned int src_y, unsigned int dst_x, unsigned int dst_y) {
    if (cache == NULL) return -2;  //cache non inizializzata
    
    unsigned int index = hash_coordinate(src_x, src_y, dst_x, dst_y, cache->capacity);
    
    //linear probing per gestire collisioni
    for (int i = 0; i < cache->capacity; i++) {
        int pos = (index + i) % cache->capacity;
        CacheEntry* entry = &cache->entries[pos];
        
        if (!entry->valid) {
            return -2;  //entry vuota, non trovato
        }
        
        if (entry->src_x == src_x && entry->src_y == src_y && 
            entry->dst_x == dst_x && entry->dst_y == dst_y) {
            return entry->cost;  //trovato
        }
    }
    
    return -2;  //cache piena + non trovato
}

//inserisce nella cache
void metti_cache(unsigned int src_x, unsigned int src_y, unsigned int dst_x, unsigned int dst_y, int cost) {
    if (cache == NULL) {
        cache = crea_cache(5000);  //cache di 5k entry
    }
    
    unsigned int index = hash_coordinate(src_x, src_y, dst_x, dst_y, cache->capacity);
    
    //linear probing per trovare uno slot
    for (int i = 0; i < cache->capacity; i++) {
        int pos = (index + i) % cache->capacity;
        CacheEntry* entry = &cache->entries[pos];
        
        if (!entry->valid) {//slot vuoto, inserisci qui
            entry->src_x = src_x;
            entry->src_y = src_y;
            entry->dst_x = dst_x;
            entry->dst_y = dst_y;
            entry->cost = cost;
            entry->valid = true;
            cache->size++;
            return;
        }
        
        //aggiorna entry esistente -- non lo uso più
        if (entry->src_x == src_x && entry->src_y == src_y && entry->dst_x == dst_x && entry->dst_y == dst_y) {
            entry->cost = cost;
            return;
        }
    }
    
    //cache piena quindi sostituisci una entry random
    int pos = rand() % cache->capacity;
    cache->entries[pos] = (CacheEntry){src_x, src_y, dst_x, dst_y, cost, true};
}

//invalida tutta la cache causa change_cost/toggle_air_route
void cancella_cache() {
    if (cache == NULL) return;
    
    for (int i = 0; i < cache->capacity; i++) {
        cache->entries[i].valid = false;
    }
    cache->size = 0;
}

//algoritmo per ricercare 
int dijkstra(int start_x, int start_y, int end_x, int end_y) {

    //reinizializzo tutto a inf
    for (int i = 0; i < righe; i++) {
        for (int j = 0; j < colonne; j++) {
            g_score[i][j] = INF;
        }
    }

    //inizializza 
    g_score[start_y][start_x] = 0;
    MinHeap* heap = create_heap(righe * colonne);

    //partenza
    heap_push(heap, start_x, start_y, 0);

    while (!heap_empty(heap)) {

        HeapNode current = heap_pop(heap);
        int x = current.x, y = current.y;

        //fine trovata
        if (x == end_x && y == end_y) {

            int result = g_score[y][x];

            //azzera heap
            free_heap(heap);
            
            return result;
        }
        
        /*IMPORTANTE: 
        se l'esagono corrente ha costo 0 non può uscire via terra anche le rotte aeree hanno costo 0
        aka un esagono con costo 0 è valido solo come destinazione*/
        
        if (ACCESSO(x, y).cost > 0) {

            //esplora vicini terrestri
            int vicini[6][2];
            int num_vicini = get_vicini_terrestri(x, y, vicini);
            
            for (int i = 0; i < num_vicini; i++) {
                int nx = vicini[i][0], ny = vicini[i][1];
                
                int tentative_g = g_score[y][x] + ACCESSO(x, y).cost;
                
                if (tentative_g < g_score[ny][nx]) {
                    g_score[ny][nx] = tentative_g;
                    heap_push(heap, nx, ny, tentative_g);
                }
            }
            
            //esplora rotte se ci sono
            for (int i = 0; i < ACCESSO(x, y).air_count; i++) {
                int nx = ACCESSO(x, y).rotte[i].dest_x;
                int ny = ACCESSO(x, y).rotte[i].dest_y;
                int air_cost = ACCESSO(x, y).rotte[i].cost;
                
                if (air_cost == 0) continue;

                int tentative_g = g_score[y][x] + air_cost;

                if (tentative_g < g_score[ny][nx]) {
                    g_score[ny][nx] = tentative_g;
                    heap_push(heap, nx, ny, tentative_g);
                }
            }
        }
    }
    
    //non trovato aka destinazione non ragiungibile
    free_heap(heap);
    
    return -1;
}

void travel_cost(unsigned int xp, unsigned int yp, unsigned int xd, unsigned int yd) {

    //validazione coordinate
    if (!coordinate_valide(xp, yp) || !coordinate_valide(xd, yd)) {
        printf("-1\n");
        return;
    }
    
    //caso particolare di stesso esagono
    if (xp == xd && yp == yd) {
        printf("0\n");
        return;
    }

    //0 in partenza aka non uscibile
    if (ACCESSO(xp, yp).cost == 0) {
        printf("-1\n");
        return;  
    }

    // Cerca nella cache
    int cached_result = cache_lookup(xp, yp, xd, yd);
    if (cached_result != -2) {
        printf("%d\n", cached_result);
        return;
    }

    //non trovato in cache quindi calcola
    int result = dijkstra(xp, yp, xd, yd);
    printf("%d\n", result);

    // Inserisci in cache se il risultato è valido
    if (result >= 0) {
        metti_cache(xp, yp, xd, yd, result);
    }
}

