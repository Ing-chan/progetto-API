#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#define MAX_AIR_ROUTES 5
#define INF INT_MAX

//per accedere con la riga ribaltata in basso in una coppia riga, colonna perché la mappa ha 0,0 in basso a sx
#define ACCESSO(x, y) mappa[righe - 1 - (x)][y] 

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
//int cache_version = 0;  per debug incrementato ad ogni change_cost/toggle_air_route/init


//dichiarazione funzioni
void init(unsigned int nuove_colonne, unsigned int nuove_righe);
void change_cost(unsigned int y, unsigned int x, int v, unsigned int raggio);
int distanza_esagoni(int x1, int y1, int x2, int y2);
bool coordinate_valide(unsigned int x, unsigned int y);
void toggle_air_route(unsigned int y1, unsigned int x1, unsigned int y2, unsigned int x2);
void travel_cost(unsigned int yp, unsigned int xp, unsigned int yd, unsigned int xd);

//funzioni per MinHeap e A*
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
            sscanf(riga, "%*s %u %u %d %u", &y, &x, &v, &raggio);
            change_cost(y, x, v, raggio);
        }
        else if (strncmp(comando, "toggle_air_route", 16) == 0) {
            sscanf(riga, "%*s %u %u %u %u", &y1, &x1, &y2, &x2);
            toggle_air_route(y1, x1, y2, x2);
        }
        else if (strncmp(comando, "travel_cost", 11) == 0) {
            sscanf(riga, "%*s %u %u %u %u", &yp, &xp, &yd, &xd);
            travel_cost(yp, xp, yd, xd);
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

    //messaggio di risposta
    printf("OK\n");
}

//distanza tra esagoni usando coordinate cubiche
int distanza_esagoni(int r1, int c1, int r2, int c2) {

    int x1 = c1 - (r1 / 2);
    int z1 = r1;
    int y1 = -x1 - z1;

    int x2 = c2 - (r2 / 2);
    int z2 = r2;
    int y2 = -x2 - z2;

    // Calcolo differenze
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    int dz = abs(z1 - z2);

    // Distanza esagonale = max delle tre differenze
    int max1 = dx > dy ? dx : dy;
    int max2 = dz > max1 ? dz : max1;

    return max2;
}

bool coordinate_valide(unsigned int x, unsigned int y) {
    return x < righe && y < colonne;
}

void change_cost(unsigned int y, unsigned int x, int v, unsigned int raggio){

    //esagono non valido o raggio nullo o v non in range
    if(!coordinate_valide(x, y) || raggio==0 || v<-10 || v>10){
        printf("KO\n");
        return;
    }

    //trova la distanza di tutti gli esagoni dal corrente
    //possibilmente da ottimizzare per restringere la ricerca nel raggio: devo cambiare i costi di tutti quelli in +/-(r-1)!!
    for (int i = 0; i < righe; i++) {
        for (int j = 0; j < colonne; j++) {

            int dist = distanza_esagoni(righe - 1 - (x), y, righe - 1 - (i), j); 

            //printf("DEBUG: distanza esagono (%d,%d) da (%d,%d): %d\n", i, j, x, y, dist);

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
                
                int nuovo_costo = ACCESSO(i, j).cost + incremento;

                //limita tra 0 e 100
                if (nuovo_costo > 100) nuovo_costo = 100;
                if (nuovo_costo < 0) nuovo_costo = 0;

                //printf("DEBUG: modifica (%d,%d) perché dist=%d da (%d,%d), costo:%d→%d\n", j, i, dist, y, x, ACCESSO(i, j).cost, nuovo_costo);
                
                ACCESSO(i, j).cost = nuovo_costo;
                
                //aggiorna se ci sono i costi delle rotte aeree uscenti
                if(ACCESSO(i,j).air_count > 0){
                for (int k = 0; k < ACCESSO(i, j).air_count; k++) {

                    //aggiunge anche alle aeree il delta
                    int nuovo_costo_rotta = ACCESSO(i, j).rotte[k].cost + incremento;
                    
                    //check per il max
                    if (nuovo_costo_rotta > 100) nuovo_costo_rotta = 100;
                    if (nuovo_costo_rotta < 0) nuovo_costo_rotta = 0;

                    ACCESSO(i, j).rotte[k].cost = nuovo_costo_rotta;
                }
                }
            }
        }
    }

    
    /*debug per vedere se funziona il change_cost
    for (int x = 0; x < righe; x++) {
        for (int y = 0; y < colonne; y++) {
            printf("DEBUG: cella (%d,%d) con costo %d\n", y, x, ACCESSO(x, y).cost);
        }
    }*/

    //cambio con successo
    printf("OK\n");

    //cancella la cache perché cambiata
    cancella_cache();
}

void toggle_air_route(unsigned int y1, unsigned int x1, unsigned int y2, unsigned int x2) {
    
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
    int direzioni_pari[6][2] = {//nuovo ordine
        {-1, -1},  // nord-ovest
        {-1, 0},   // nord-est  
        {0, -1},   // ovest
        {0, 1},    // est
        {1, -1},   // sud-ovest
        {1, 0}     // sud-est
    };
    //vecchio ordine: int direzioni_dispari[6][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 1}, {1, 0}, {1, -1}};
    int direzioni_dispari[6][2] = {//nuovo ordine
        {-1, 0},   // nord-ovest
        {-1, 1},   // nord-est
        {0, -1},   // ovest  
        {0, 1},    // est
        {1, 0},    // sud-ovest
        {1, 1}     // sud-est
    };

    //sceglie in base a riga, quindi x e non righe-1-x
    int (*direzioni)[2] = ((x) % 2 == 0) ? direzioni_pari : direzioni_dispari;
    int count = 0;

    //printf("DEBUG VICINI di (%d,%d) usando pattern: %s\n", y, x, (x%2==0) ? "pari" : "dispari");
    
    for (int i = 0; i < 6; i++) {
        int nx = x + direzioni[i][0];
        int ny = y + direzioni[i][1];

        if (nx >= 0 && nx < righe && ny >= 0 && ny < colonne) {
            vicini[count][0] = nx;
            vicini[count][1] = ny;
            count++;
            //printf("DEBUG VICINI: vicino %d: (%d,%d) -> costo=%d\n", i, ny, nx, ACCESSO(nx, ny).cost);
        }
    }
    return count;
}

//minheap e strutture tipiche per A*
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

    return a->y < b->y; //altrimenti vado di colonna
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
        cache = crea_cache(10000);  // Cache di 10k entry
    }
    
    unsigned int index = hash_coordinate(src_x, src_y, dst_x, dst_y, cache->capacity);
    
    // Linear probing per trovare uno slot
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
        
        //aggiorna entry esistente
        if (entry->src_x == src_x && entry->src_y == src_y && entry->dst_x == dst_x && entry->dst_y == dst_y) {
            entry->cost = cost;
            return;
        }
    }
    
    // Cache piena quindi sostituisci una entry random (strategia semplice)
    int pos = rand() % cache->capacity;
    cache->entries[pos] = (CacheEntry){src_x, src_y, dst_x, dst_y, cost, true};
}

// Invalida tutta la cache causa change_cost/toggle_air_route
void cancella_cache() {
    if (cache == NULL) return;
    
    for (int i = 0; i < cache->capacity; i++) {
        cache->entries[i].valid = false;
    }
    cache->size = 0;
    //cache_version++; per debug
}

//algoritmo per ricercare 
int dijkstra(int start_x, int start_y, int end_x, int end_y) {

    //array per g_score e visited
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

    //inizializza 
    g_score[righe - 1 - start_x][start_y] = 0;
    MinHeap* heap = create_heap(righe * colonne);

    //partenza
    heap_push(heap, start_x, start_y, 0);

    //printf("DEbug: partenza da: %d,%d\n", start_y, start_x);
    
    while (!heap_empty(heap)) {

        HeapNode current = heap_pop(heap);
        int x = current.x, y = current.y;

        if (visited[righe - 1 - x][y]) continue;
        visited[righe - 1 - x][y] = true;

        //printf("DEGUB: percorso fino a %d,%d con g= %d\n", y, x, g_score[righe - 1 - x][y]);
        
        //fine trovata
        if (x == end_x && y == end_y) {

            int result = g_score[righe - 1 - x][y];

            //libera tutto
            for (int i = 0; i < righe; i++) {
                free(g_score[i]);
                free(visited[i]);
            }
            free(g_score);
            free(visited);
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
                
                if (visited[righe - 1 - nx][ny]) continue;
                
                int tentative_g = g_score[righe - 1 - x][y] + ACCESSO(x, y).cost;
                
                if (tentative_g < g_score[righe - 1 - nx][ny]) {
                    g_score[righe - 1 - nx][ny] = tentative_g;
                    heap_push(heap, nx, ny, tentative_g);
                }
            }
            
            //esplora rotte se ci sono
            for (int i = 0; i < ACCESSO(x, y).air_count; i++) {
                int nx = ACCESSO(x, y).rotte[i].dest_x;
                int ny = ACCESSO(x, y).rotte[i].dest_y;
                int air_cost = ACCESSO(x, y).rotte[i].cost;
                
                if (air_cost == 0 || visited[righe - 1 - nx][ny]) continue;
                
                int tentative_g = g_score[righe - 1 - x][y] + air_cost;
                
                if (tentative_g < g_score[righe - 1 - nx][ny]) {
                    g_score[righe - 1 - nx][ny] = tentative_g;
                    heap_push(heap, nx, ny, tentative_g);
                }
            }
        }
    }
    
    //non rovato aka destinazione non ragiungibile
    for (int i = 0; i < righe; i++) {
        free(g_score[i]);
        free(visited[i]);
    }
    free(g_score);
    free(visited);
    free_heap(heap);
    
    return -1;
}

void travel_cost(unsigned int yp, unsigned int xp, unsigned int yd, unsigned int xd) {
    
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

    // Inserisci in cache solo se il risultato è valido
    if (result >= 0) {
        metti_cache(xp, yp, xd, yd, result);
    }
}

/*DA FARE:
    implementare una cache per evitare di dover fare sempre la ricerca e velocizzare i travel_cost
        mi son rotto di aspettare vada tutto bene e l'ho messa comunque.


PROBEMI:
//sistemato// toggle-air-route NON RITORNA nulla nel caso in cui RIMUOVE una rotta ESISTENTE 

//sistemato// DURANTE UNA INIT() SI INTERROMPE NEL CASO CHE NE ESISTE GIÀ UNA. SI FERMA A free(ACCESSO(x, y).rotte); E DA ABORTION O SEGMENTATION FAULT.
    quelle maledette globali direttamente in scanf...

 //sistemato// le rotte VALIDE ma NON RAGGIUNGIBILI danno 100 invece di -1
    può esserci un problema con le approssimazioni per difetto in change_cost?
    EXAMPLE.TXT SUPERATO
    
//sistemato//perché adesso example funziona example, ma empty da errori grossi?
    io ho colonne e righe invertite... ci ho perso 2 giorni -.-
    !!!tutti i comandi ricevono coordinate (colonna, riga) come init e non (riga, colonna)!!!
        che bal troppa sbatta invertire tutto da coppie (riga,colonna) a coppie (colonna,riga)
        inverto le principali e poi faccio le operazioni tenendo conto di dover cambiare

//sistemato//edge_cases.txt fa errori come un travel_cost 2 da -1 e un 5 da 3. perché? 
    focus su edge_cases
    potrebbe essere di nuovo errori nel arrotondamento, devo per forza usare floor e non basta il cast
        floor non compila, ho usato una ad hoc.
    per forza sono errori in change_cost perché prima funzionano i travel.
    cosa sto sbagliando in change_cost? il mio floor del float non va bene?
    confermato da init 10 5 e change_cost 5 2 -9 5 che rimane a 1 solo la colonna più a sx della matrice (colonna 0). perché?
        deve essere un errore nel calcolo della distanza perché alcune caselle a distanza 5 vengono modificate. esempio: colonna 9,riga 4
        alte colonne a distanza 5 non vengono toccate (giustamente). esempio: colonna 0 riga 2
        entrambi distano 5 ma solo nel secondo caso non modifica.
        probabile bug nel calcolo della distanza. perché?
        nb: il change_cost fa fallire travel_cost 0 0 2 0 perché A* vede (colonna 1,riga 0) come intransitabile (actually, vede tutta la colonna 1 intransitabile e quindi -1)
    madooo era la distanza_esagoni che prendeva le coordinate invertite -.-.-.-.-
    3 giorni così, ora ho invertito x,y in y,x e dovrebbe andare bene...
    EDGE_CASES.TXT SUPERATO

//DA SISTEMARE//un diff su empty ha rivelato che sbaglio di poche unità i travel_cost. come mai? 
    ho fato debug su a* e confermato funziona come ho inteso, non è il problema.
    ho provato a modificare vicini_terrestri e ora li prende nell'ordine confermato corretto.
    ho provato a non ribaltare le righe di visited e g_score in a* ed è anche peggio di prima, ho cancellato quella branch.
    empty continua a darmi problemi... come mai? adesso mi sbaglia più spesso e di molto rispetto a prima.
    è un problema di A*, probabilmente come prende i vicini terrestri
        ho corretto x%2 e sceglie in base alla riga, le prime 25 righe sono corrette!
    ho di nuovo errore alla riga 25 ma dovrei averla corretta perché ora non uso più il casting per troncare, cosa non va ora?
            ho notato che spesso sono differenze di travel_cost di +/- 1 (pochi casi +/- 2). 
            a volta capita siano differenze più grandi di un paio di centinaia di valori (magari per un toggle_air non contato?)
            nel file diff sono segnati 4 errori nelle prime 50 righe in cui un mio 475 in realtà sarebbe un 474
            gli altri sono vari a circa 6k, 18k, 24k, 30k, 36k, 42k, 48k, 54k. è un caso? non credo
        devo sistemare di nuovo quel cabbo di floor ad hoc, ho provato a usar double invece di float ma non cambia...
    miracolo, floor ora funziona perché ho riscritto da zero tasks e launch, evidentemente avevo sbagliato qualcosa prima
        il problema però rimane, a riga 25 mi da 475 e non 474...
    appurato il problema non era ne calcolo. o sbaglio di nuovo a prendere dei vicini o sbaglio ad estrarre i nodi e a volte conta due volte quello iniziale/finale?
        ho provato a rendere deterministica la scelta del nodo in caso f score siano uguali ma peggiora solo: passa a 492 e l'errore succesivoriamen a 475.
    può essere che in alcuni casi sbaglio ancora la distanza?

    ho realizzato che se uso h =distanza_esagoni per le rotte aeree non è ammissibile. ritorno ad usare dijkstra ponendo h=0.
    ora riprovo ad usare una funzione per rendere deteministisca la scelta di smallest dentro heapify
    ok con questa nuova h=0 (e avendo usato di nuovo better_node, ma non centra) ho risolto il problema dei valori discostanti di tanti numeri
        ora non ho più il problema delle rotte aeree
    ho sempre il problema del travel_cost di 1 o 2.

    mi son rotto di cercare di capire sto problema e nel mentre ho implmentato la cache. rimane comunque il problema che ogni tot sbaglia di 1 o i 2

    sul telegram ci sono varie persone che usano floor e hanno un errore. provo a ri implmementare la mia ad hoc
        ho fatto una ad hoc diversa da prima ma comuqnue non cambia nulla. rollback alla floor di math.h

    ho fatto girare a* sul verificatore e occupa troppa memoria, trasformo in dijkstra puro.

    ho letto di uno che usava double come me e aveva problemi. non è cambiato nulla ad usare float.
    ho pensato che magari aumenta di 1 perché uno dei change_cost pone a 0 un esagono lungo il percorso
        perché evita quell'esagono, un passaggio che dovrebbe costare 1 costa 2 quindi aumenta a 475. ha senso?
    
    urge chiarimento per get_vicini_terrestri e distanza_esagoni: a entrambe passo da x senza ribaltarla!!
    da considerare anche in heap push non ribalto (però li ha senso perché ribalto tutto il resto dopo?)
        per distanza_esagoni va di culo perché va a culo che passo invertite le coordinate x e y e funziona
        in get_vicini sto proprio dando la coordinata della mappa sbagliata quindi non va a vedere i reali costi dei vicini ma di quelli della x non ribaltata!!
        ho ribaltato la x di vicini_terrestri, alcune cose vegono errate ma credo sia perché devo invertire anche le coordinate della distanza
        sto ribaltando le righe coordinate della distanza e mettenod coppie x,y
        ora ho 453 invece di 474 in input.txt mentre in edge_cases travel_cost 0 0 9 3 da -1 invece di 2, tutto il resto è corretto. come mai?
    ora distanza_esagoni calcola la distanza esatta e il cahnge_cost influenza solo gli esagoni nel raggio.
    altro problema: dijkstra in travel_cost 0 0 9 3 arriva a esplorare 9,4 ma non va diretto a 9,3.
        probabile svista in vicini_terrestri?
        alla fine non devo passare coordinate fisiche!
        risolto, passo x,y a vicini. dovrei forse fare tutto così?
    ora sono tornato al punto di prima: edge_cases viene passato ma riga25 di empty ecc danno ancora problemi di +/-1
    usare ovunque in dijkstra le coordinate logiche non ha funzionato.
    sono davveroa corto di idee...
    il problema può essere che al mio dijkstra e nei vicini terrestri passo la x senza ribaltarla!

    DAJEEEEE HO RISOLTO ora viene 474 anche lì. il problema era in get vicini terrestr!!!!!
        davo x%2 che però non era ribaltata!! con righe-1-x%2 risolvo il problema!!

    fuck ora però ho altri problemi in empty, alcuni che venivano corretti ora hanno lo stesso problema di +-1 o 2.
    può essere che devo considerare in modo diverso le righe pari e quelle dispari? non basta un semplice x%2 o righe-1-x%2.
    sembra ci sia un problema per come prende le righe pari o dispari, boh
        per quelle pari posso fare righe-1-x%2 e per quelle dispari potrei provare x%2 però boh mi sa di cabbata
        devo vedere per quali fa -1 e per quali +1
        magari l'ordine dei pari e dispari non va bene? io ho fatto bassosx, sx, altosx, altodx, dx, bassodx.
        provo con ordine nord-ovest, nord-est, ovest, est, sud-ovest, sud-est
    ora che ho spostato l'ordine di esplorazione, funzionano sia edge_cases, che su empty i due travel_cost:
        travel_cost 272 114 324 163 da 77 e i problematici come riga 25 travel_cost 38 61 457 170 danno 474 e sono corretti.

    nuovo problema: diff ha evidenziato comunque 150+ errori
        alcuni valori sono sistematicamente più alti di 1: 941 e 940, 286 e 285, 843 e 842...
        alcuni-1 dovrebbero essere valori positivi, tipo 117, 364..
        alcuni miei 443 dovrebbero essere 327(aka differenza di 116)
        è sempre un problema nella indicizzazzione? :/
        provo a ribaltare di nuovo l'ordine in cui considero le direzioni
        non ha funzionato, travel_cost 272 114 324 163 torna a dare 76
        provo ad  aggiungere dei debug dentro vicnin terrestri e vediamo che fa
        è servito a nulla, prende correttamente righe pari e dispari usando x%2 e non capisco dove arriva sto nuyovo problema....

        */
