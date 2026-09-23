#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define MAX_EV 512

// Definire struct per EV, staz e autostrada

struct Station {
    int distanza;
    int num_ev;
    int veicoli[MAX_EV]; // Puntatore per allocazione dinamica
};

struct Highway {
    struct Station* stazioni; // Puntatore per allocazione dinamica
    int num_staz;
};

// F aggiungi nuova staz alla autostrada
void add_staz(struct Highway *highway, int distanza, int num_ev, int *valori_autonomia) {
    // Controlla se esiste già una staz
    for (int i = 0; i < highway->num_staz; i++) {
        if (highway->stazioni[i].distanza == distanza) {
            fprintf(stdout, "non aggiunta\n"); // In caso non aggiunge
            return;
        }
    }

    // Rialloca l'array stazioni
    highway->stazioni = (struct Station*)realloc(highway->stazioni, (highway->num_staz + 1) * sizeof(struct Station));
    struct Station *new_staz = &highway->stazioni[highway->num_staz];
    new_staz->distanza = distanza;
    new_staz->num_ev = num_ev;

    for (int i = 0; i < num_ev; i++) {
        new_staz->veicoli[i] = valori_autonomia[i];
    }
    highway->num_staz++;
    fprintf(stdout, "aggiunta\n"); // aggiunta staz
}

// F per demolire una staz
void distruggi_staz(struct Highway *highway, int distanza) {
    int trovata = -1;
    for (int i = 0; i < highway->num_staz; i++) {
        if (highway->stazioni[i].distanza == distanza) {
            trovata = i;
            break;
        }
    }

    if (trovata != -1) {

        for (int j = trovata; j < highway->num_staz - 1; j++) {
            highway->stazioni[j] = highway->stazioni[j + 1];
        }

        // Rialloca l'array stazioni
        highway->stazioni = (struct Station*) realloc(highway->stazioni, (highway->num_staz - 1) * sizeof(struct Station));
        highway->num_staz--;

        fprintf(stdout, "demolita\n"); // staz demolita
    } else {
        fprintf(stdout, "non demolita\n"); // staz non demolita
    }
}

// F per aggiungere una nuova EV alla staz
void aggiungi_EV(struct Highway *highway, int distanza, int autonomia) {
    // Trova la staz e aggiungi l'EV
    for (int i = 0; i < highway->num_staz; i++) {
        if (highway->stazioni[i].distanza == distanza) {
            struct Station *stazione = &highway->stazioni[i];
            if (stazione->num_ev < MAX_EV) {
                stazione->veicoli[stazione->num_ev] = autonomia;
                stazione->num_ev++;
                fprintf(stdout, "aggiunta\n"); // aggiunto!
                return;
            } else {
                fprintf(stdout, "non aggiunta\n"); // non aggiunto perchè supera il num max di EV
                return;
            }
        }
    }
    fprintf(stdout, "non aggiunta\n"); // non aggiunta perchè non ha trovato la staz
}

// F per rottamare un EV
void rottama_EV(struct Highway *highway, int distanza, int autonomia) {
    // Trova la staz e togli l'EV richesto
    for (int i = 0; i < highway->num_staz; i++) {
        if (highway->stazioni[i].distanza == distanza) {
            struct Station *stazione = &highway->stazioni[i];
            for (int j = 0; j < stazione->num_ev; j++) {
                if (stazione->veicoli[j] == autonomia) {
                    for (int k = j; k < stazione->num_ev - 1; k++) {
                        stazione->veicoli[k] = stazione->veicoli[k + 1]; //da cambiare
                    }
                    stazione->num_ev--;
                    fprintf(stdout, "rottamata\n");
                    return;
                }
            }
        }
    }
    fprintf(stdout, "non rottamata\n");
}

// Trova la staz a distanza minima che ancora non è stata usata
int staz_dist_min(struct Highway *highway, int dist[], bool passata[]) {
    int min = INT_MAX, min_index = 0;

    for (int v = 0; v < highway->num_staz; v++) {
        if (passata[v] == false && dist[v] <= min) {
            min = dist[v];
            min_index = v;
        }
    }
    return min_index;   // Ritorna l'indice più breve
}

// F per ottenere l'indice completo
int indice_completo_staz(struct Highway *highway, int distanza) {
    for (int i = 0; i < highway->num_staz; i++) {
        if (highway->stazioni[i].distanza == distanza) {
            return i;
        }
    }
    return -1; // Non trovato
}

// Dijkstra per il percorso più breve
void dijkstra(struct Highway *highway, int inizio, int fine) {

    if(inizio == -1 || fine == -1) {
        fprintf(stdout, "nessun percorso\n");
        return;
    }

    int *fermata = (int *)malloc(highway->num_staz * sizeof(int));
    bool *passata = (bool *)malloc(highway->num_staz * sizeof(bool));
    int *prec = (int *)malloc(highway->num_staz * sizeof(int));

    if (!fermata || !passata || !prec) {
        free(fermata);
        free(passata);
        free(prec);
        return;
    }

    // Inizializzazione
    for (int i = 0; i < highway->num_staz; i++) {
        fermata[i] = INT_MAX;
        passata[i] = false;
        prec[i] = -1;
    }

    fermata[inizio] = 0;

    for (int count = 0; count < highway->num_staz - 1; count++) {
        int u = staz_dist_min(highway, fermata, passata);
        passata[u] = true;

        // Aggiorna il valore di stop delle stazioni vicine alla selezionata
        for (int v = 0; v < highway->num_staz; v++) {
            if (!passata[v] && fermata[u] != INT_MAX) {
                bool raggiungibile = false;
                for (int veicolo = 0; veicolo < highway->stazioni[u].num_ev; veicolo++) {
                    int autonomia = highway->stazioni[u].veicoli[veicolo];
                    if (abs(highway->stazioni[v].distanza - highway->stazioni[u].distanza) <= autonomia) {
                        raggiungibile = true;
                        break;
                    }
                }
                if (raggiungibile) {
                    if (fermata[u] + 1 < fermata[v] ||
                       (fermata[u] + 1 == fermata[v] && highway->stazioni[u].distanza < highway->stazioni[prec[v]].distanza)) {
                        fermata[v] = fermata[u] + 1;
                        prec[v] = u;
                    }
                }
            }
        }
    }

    // Output del percorso dalla partenza all'arrivo
    if (fermata[fine] != INT_MAX) {
        int *percorso = (int *)malloc(highway->num_staz * sizeof(int));
        if (!percorso) {
            //fprintf(stdout, "Errore allocazione memoria!\n");
            free(fermata);
            free(passata);
            free(prec);
            return;
        }

        int indice_percorso = 0;
        int attuale = fine;
        while (attuale != -1) {
            percorso[indice_percorso++] = attuale;
            attuale = prec[attuale];
        }
        for (int i = indice_percorso - 1; i >= 0; i--) {
            fprintf(stdout, "%d", highway->stazioni[percorso[i]].distanza);
            if (i != 0) {
                fprintf(stdout, " ");
            }
        }
        fprintf(stdout, "\n");
        free(percorso);
    } else {
        fprintf(stdout, "nessun percorso\n");
    }

    free(fermata);
    free(passata);
    free(prec);
}

int main() {
    struct Highway highway = { .stazioni = NULL, .num_staz = 0 };
    char comando[11] = {0};
    int scarto;

    while (fscanf(stdin, "%10s", comando) > 0) {

        char ch;
        do {
            ch = fgetc(stdin);
        } while (ch != ' ' && ch != '\n' && ch != EOF);

        if (strncmp(comando, "aggiungi-st", 10) == 0) {
            int distanza, num_ev;
            int valori_autonomia[MAX_EV];
            scarto = fscanf(stdin, "%d %d", &distanza, &num_ev);
            for(int i = 0; i < num_ev; i++) {
                scarto = fscanf(stdin, "%d", &valori_autonomia[i]);
            }
            add_staz(&highway, distanza, num_ev, valori_autonomia);
        } else if (strncmp(comando, "demolisci-s", 10) == 0) {
            int distanza;
            scarto = fscanf(stdin, "%d", &distanza);
            distruggi_staz(&highway, distanza);
        } else if (strncmp(comando, "aggiungi-au", 10) == 0) {
            int distanza, autonomia;
            scarto = fscanf(stdin, "%d %d", &distanza, &autonomia);
            aggiungi_EV(&highway, distanza, autonomia);
        } else if (strncmp(comando, "rottama-aut", 10) == 0) {
            int distanza, autonomia;
            scarto = fscanf(stdin, "%d %d", &distanza, &autonomia);
            rottama_EV(&highway, distanza, autonomia);
        } else if (strncmp(comando, "pianifica-p", 10) == 0) {
            int partenza, arrivo;
            scarto = fscanf(stdin, "%d %d", &partenza, &arrivo);
            int indice_partenza = indice_completo_staz(&highway, partenza);
            int indice_arrivo = indice_completo_staz(&highway, arrivo);
            dijkstra(&highway, indice_partenza, indice_arrivo);
        }
        scarto++;
    }

    return 0;
}

