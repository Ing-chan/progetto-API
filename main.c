#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define MAX_AIR_ROUTES 5
#define MAX_NEIGHBORS 6 

//per ribaltare la y
#define TILT(y, x) mappa[righe - 1 - (y)][x] 

typedef struct {
    int dest_index;
    int cost;
} AirRoute;

typedef struct {
    int land_cost; //0 senza uscita, 100 max
    int neighbors[MAX_NEIGHBORS]; // max 6, non ha senso usare vla

    AirRoute* rotte; //ha senso vla perché poche, creo struct a parte
    int air_count; //max 5 elementi
} Esagono;

//globali perché devono accederci le altre funzioni
unsigned int colonne, righe; 
Esagono* mappa = NULL;

int main() {
    char riga[80];       // buffer sufficiente per una riga di comando
    char comando[20];    // spazio per il comando

    unsigned int x, y, r;
    int v; // può essere negativo
    unsigned int x1, y1, x2, y2;
    unsigned int xp, yp, xd, yd;

    while (fgets(riga, sizeof(riga), stdin)) {

        sscanf(riga, "%19s", comando);  // legge il nome del comando

        if (strncmp(comando, "init", 4) == 0) {
            sscanf(riga, "%*s <%u> <%u>", &colonne, &righe);
            init(colonne, righe);
        }
        else if (strncmp(comando, "change_cost", 11) == 0) {
            sscanf(riga, "%*s <%u> <%u> <%d> <%u>", &x, &y, &v, &r);
            change_cost(x, y, v, r);
        }
        else if (strncmp(comando, "toggle_air_route", 16) == 0) {
            sscanf(riga, "%*s <%u> <%u> <%u> <%u>", &x1, &y1, &x2, &y2);
            toggle_air_route(x1, y1, x2, y2);
        }
        else if (strncmp(comando, "travel_cost", 11) == 0) {
            sscanf(riga, "%*s <%u> <%u> <%u> <%u>", &xp, &yp, &xd, &yd);
            travel_cost(xp, yp, xd, yd);
        }
    }
}

void init(colonne, righe){

    if (mappa != NULL) {
        for (int y = 0; y < righe; y++) {
            free(mappa[y]);
        }

        free(mappa);
        mappa = NULL;
    }

    mappa = malloc (righe * sizeof(Esagono));

    for (int y = 0; y < righe; y++) {

        mappa[y] = malloc(colonne * sizeof(HexTile));

        for (int x = 0; x < colonne; x++) {
            mappa[y][x].cost = 1;
        }
    }
}

