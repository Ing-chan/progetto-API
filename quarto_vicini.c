#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#define MAX_AIR_ROUTES 5
#define INF INT_MAX

//per accedere con la riga ribaltata in basso in una coppia riga, colonna
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
void init(unsigned int nuove_colonne, unsigned int nuove_righe);
void change_cost(unsigned int y, unsigned int x, int v, unsigned int raggio);
int floor_float(float val);
int distanza_esagoni(int x1, int y1, int x2, int y2);
bool coordinate_valide(unsigned int x, unsigned int y);
void toggle_air_route(unsigned int y1, unsigned int x1, unsigned int y2, unsigned int x2);
void travel_cost(unsigned int yp, unsigned int xp, unsigned int yd, unsigned int xd);

//funzioni per MinHeap e A*
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
int distanza_esagoni(int y1, int x1, int y2, int x2) {
    int q1 = y1;
    int r1 = x1 - (y1 + (y1 & 1)) / 2;
    int s1 = -q1 - r1;
    
    int q2 = y2;
    int r2 = x2 - (y2 + (y2 & 1)) / 2;
    int s2 = -q2 - r2;
    
    return (abs(q1 - q2) + abs(r1 - r2) + abs(s1 - s2)) / 2;
}

bool coordinate_valide(unsigned int x, unsigned int y) {
    return x < righe && y < colonne;
}

//ho dovuto farla ad hoc perchè quel babbp di floor non viene linkato da gcc
int floor_float(float val) {
    int i = (int)val;
    if (val < 0 && val != i) {
        return i - 1;
    }
    return i;
}

void change_cost(unsigned int y, unsigned int x, int v, unsigned int raggio){

    //esagono non valido o raggio nullo o v non in range
    if(!coordinate_valide(x, y) || raggio==0 || v<-10 || v>10){
        printf("KO\n");
        return;
    }

    /*printf("DEBUG: change_cost centro=(%d,%d), v=%d, raggio=%d\n", x, y, v, raggio);
    int dist_AB = distanza_esagoni(2, 5, 0, 0);  // da centro a (0,0)
    int dist_BA = distanza_esagoni(0, 0, 2, 5);  // da (0,0) a centro
    printf("TEST SIMMETRIA: (2,5)→(0,0)=%d, (0,0)→(2,5)=%d\n", dist_AB, dist_BA);*/
    
    //trova la distanza di tutti gli esagoni dal corrente
    //possibilmente da ottimizzare per restringere la ricerca nel raggio: devo cambiare i costi di tutti quelli in +/-(r-1)!!
    for (int i = 0; i < righe; i++) {
        for (int j = 0; j < colonne; j++) {

            int dist = distanza_esagoni(x, y, i, j);

            //printf("DEBUG: distanza esagono (%d,%d) da (%d,%d): %d\n", i, j, x, y, dist);

            //applica cambiamento a tutti gli esagoni nel raggio
            if (dist < raggio) {

                //calcola nuovo costo via formula, usando approssimazione per difetto
                float delta = (float) (raggio - dist) / raggio;
                if (delta < 0) delta = 0;
                int incremento = floor_float(v * delta); //devo mettere floor ma non va
                //non basta il cast
                //da errore anche se metto -lm alla fine, perché? 

                //printf("DEBUG: change_cost su (%d,%d): vecchio_costo=%d\n", i, j, ACCESSO(i, j).cost);

                int nuovo_costo = ACCESSO(i, j).cost + incremento;

                //limita tra 0 e 100
                if (nuovo_costo > 100) nuovo_costo = 100;
                if (nuovo_costo < 0) nuovo_costo = 0;

                //printf("DEBUG: modifica (%d,%d) dist=%d, %d→%d\n", i, j, dist, ACCESSO(i, j).cost, nuovo_costo);
                
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

    /*debug:printa tutta la matrice
    for (int x = 0; x < righe; x++) {
        for (int y = 0; y < colonne; y++) {
            printf("DEBUG: cella (%d,%d) con costo %d\n", x, y, ACCESSO(x, y).cost);
        }
    }*/

    //cambio con successo
    printf("OK\n");
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
        int nuovo_costo = floor_float((float) somma_costi / (ACCESSO(x1, y1).air_count + 1)); //anche qua appross per difetto con la mia ad hoc
        
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
}

//calcola i vicini terrestri di un esagono
int get_vicini_terrestri(int x, int y, int vicini[][2]) {

    //pattern per righe pari e dispari
    //posizioni: bassosx, sx, altosx, altodx, dx, bassodx
    int direzioni_pari[6][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 0}, {0, -1}};
    int direzioni_dispari[6][2] = {{0, -1}, {-1, 0}, {0, 1}, {1, 1}, {1, 0}, {1, -1}};
    
    //ho cambiato da x a y
    int (*direzioni)[2] = (y % 2 == 0) ? direzioni_pari : direzioni_dispari;
    int count = 0;
    
    for (int i = 0; i < 6; i++) {
        int nx = x + direzioni[i][0];
        int ny = y + direzioni[i][1];
        
        if (coordinate_valide(nx, ny)) {
            vicini[count][0] = nx;
            vicini[count][1] = ny;
            count++;
        }rché pr
    }
    return count;
}

//minheap per A*
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

//algoritmo per ricercare 
int astar(int start_x, int start_y, int end_x, int end_y) {

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
    int h_start = distanza_esagoni(start_x, start_y, end_x, end_y);

    //partenza
    heap_push_astar(heap, start_x, start_y, 0, h_start);
    
    while (!heap_empty(heap)) {

        HeapNode current = heap_pop(heap);
        int x = current.x, y = current.y;
        
        if (visited[righe - 1 - x][y]) continue;
        visited[righe - 1 - x][y] = true;
        
        //preso
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
                    int h = distanza_esagoni(nx, ny, end_x, end_y); //euristica sicuro ammissibile
                    int f = tentative_g + h;
                    heap_push_astar(heap, nx, ny, tentative_g, f);
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
                    int h = distanza_esagoni(nx, ny, end_x, end_y);
                    int f = tentative_g + h;
                    heap_push_astar(heap, nx, ny, tentative_g, f);
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

    int result = astar(xp, yp, xd, yd);
    printf("%d\n", result);
}

/*
PROBEMI:
//sistemato// toggle-air-route NON RITORNA nulla nel caso in cui RIMUOVE una rotta ESISTENTE 

//sistemato// DURANTE UNA INIT() SI INTERROMPE NEL CASO CHE NE ESISTE GIÀ UNA. SI FERMA A free(ACCESSO(x, y).rotte); E DA ABORTION O SEGMENTATION FAULT.
    quelle maledette globali direttamente in scanf...

 //sistemato// le rotte VALIDE ma NON RAGGIUNGIBILI danno 100 invece di -1
    può esserci un problema con le approssimazioni per difetto in change_cost? 
    
//sistemato//perché adesso example funziona example, ma empty da errori grossi?
    io ho colonne e righe invertite... ci ho perso 3 giorni -.-
    !!!tutti i comandi ricevono coordinate (colonna, riga) come init e non (riga, colonna)!!!
        che bal troppa sbatta invertire tutto da coppie (riga,colonna) a coppie (colonna,riga),
        inverto le principali e poi faccio le operazioni tenendo conto di dover cambiare

//sistemato//un diff su empty ha rivelato che sbaglio di poche unità i travel_cost. come mai? 
    ho fato debug su astar e confermato funziona come ho inteso, non è il problema.
    ho provato a modificare vicini_terrestri e ora li prende nell'ordine confermato corretto.
    ho provato a non ribaltare le righe di visited e g_score in astar ed è anche peggio di prima, ho cancellato quella branch.
    edge_cases.txt fa errori come un travel_cost 2 da -1 e un 5 da 3. perché? 
    focus su edge_cases
    potrebbe essere di nuovo errori nel arrotondamento, devo per forza usare floor e non basta il cast
        floor non compila, ho usato una ad hoc.
    per forza sono errori in change_cost perché prima funzionano i travel.
    cosa sto sbagliando in change_cost? il mio floor_float non va bene?
    confermato da init 10 5 e change_cost 5 2 -9 5 che rimane a 1 solo la colonna più a sx della matrice (colonna 0). perché?
        deve essere un errore nel calcolo della distanza perché alcune caselle a distanza 5 vengono modificate. esempio: colonna 9,riga 4
        alte colonne a distanza 5 non vengono toccate (giustamente). esempio: colonna 0 riga 2
        entrambi distano 5 ma solo nel secondo caso non modifica.
        probabile bug nel calcolo della distanza. perché?
        nb: il change_cost fa fallire travel_cost 0 0 2 0 perché A* vede (colonna 1,riga 0) come intransitabile (actually, vede tutta la colonna 1 intransitabile e quindi -1)
    madooo era la distanza_esagoni che prendeva le coordinate invertite -.-.-.-.-
    3 giorni così, ora ho invertito x,y in y,x e dovrebbe andare bene...
*/
