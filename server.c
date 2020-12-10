#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

//************************************************************************************************************************
//			SEZIONE STRUTTURE DATI E COSTANTI

// Struttura dati che mantiene le info del client
struct clientInfo{
    char sessionId[11];
    char username[50];
    int tentativi;
    uint32_t ip;
};

struct des_schedina{
    // ogni posizione rappresenta una ruota
    // le posizioni corrispondenti a uno sono le ruote su cui punto
    // 0 altrimenti
    int ruotaSchedina[12];
    
    // numeri giocati, 0 se non ho giocato
    int numeriGiocati[10];
    
    // importi puntati, 0 se non ho puntato
    double imp[5];
};

const int SIZE_GIOCATA_COMPLETA = 210;      // Una giocata completa
                                            // (tutte le ruote tutti in numeri tutti gli importi) è 207
const int SIZE_VINCITA_COMPLETA = 1435;     // Una record di vincita completo
                                            // (vittorie su tutti i numeri per tutti i tipi di giocata)

const int SIZE_RECAP_WIN = 159;             // Riassunto vincite

const int MARGINE_ERRORE = 512;

// Il massimo numero di estrazioni visibili (con tutte le ruote selezionate)
// sarebbe (con il buffer di default di 4096) 4096/350 = 11 -> 10. 
// 350 (grandezza estr completa) è ottenuto sperimentalmente:
// mostrare una tabella intera di estrazione occupa 341 byte, arrotondato a 350 byte
// Se invece si mostra un sola riga 4096/70 = 58 -> 55
// 70 è ottenuto sperimentalmente: stampare una sola riga occupa 67 byte, arrotondato a 70
// Per evitare queste limitazioni si usa un buffer allocato dinamicamente.
const int SIZE_ESTR_COMPLETA = 350;

//********************************************************************************************************************************
//		SEZIONE FUNZIONI SPECIFICHE DEL GIOCO DEL LOTTO

int scriviGiocata(struct des_schedina* sc, char* username, int isVittoria, time_t attuale)
{
    FILE* fptr;
    char new_name[50];
    int i = 0;
    int fd;				// file descriptor per il lock
 	int ri_flock;
    
    printf(" *** Scrivo la giocata sul file di log dell'utente\n");
    strcpy(new_name,username);
    
    if(!isVittoria)
        strcat(new_name,".txt");
    
   
    
    printf(" *** Apro il file utente %s\n",new_name);
    fptr = fopen(new_name,"a");
    
    if(fptr==NULL)
        return 0;
    
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

    // Stampo 0 se il record corrisponde a una giocata
    // 1 se corrisponde a una vittoria
    fprintf(fptr,"%d ",isVittoria);
    
    // Stampo il timestamp della giocata/vittoria
    fprintf(fptr,"%li ",attuale);
    // 12 ruote 10 numeri 5 importi = 27 posizioni
    for(i=0; i<12; i++)
        fprintf(fptr,"%d ",sc->ruotaSchedina[i]);
    
    for(i=0; i<10; i++)
        fprintf(fptr,"%d ",sc->numeriGiocati[i]);
    
    for(i=0; i<5; i++)
        fprintf(fptr,"%f ",sc->imp[i]);
    
    fprintf(fptr,"\n");
    
    flock(fd,LOCK_UN);
    fclose(fptr);
    
    // Va tutto bene
    return 1;
}

// Inizializza un struttura dati di tipo des_schedina
void iniDesSc(struct des_schedina* sc)
{
    int i;
    for(i=0; i<12; i++)
    {
        sc->ruotaSchedina[i] = 0;
        if(i<10)
            sc->numeriGiocati[i] = 0;
        if(i<5)
            sc->imp[i] = 0;
    }
}

// Effettua una giocata
int giocata(char** cmdString, char* risp, char* username)
{
    struct des_schedina str_sch;
    struct des_schedina* sc = &str_sch;
    
    char* ruote[12]; 
    ruote[0] = "bari";
    ruote[1] = "cagliari";
    ruote[2] = "firenze";
    ruote[3] = "genova";
    ruote[4] = "milano";
    ruote[5] = "napoli";
    ruote[6] = "palermo";
    ruote[7] = "roma";
    ruote[8] = "torino";
    ruote[9] = "venezia";
    ruote[10] = "nazionale";
    ruote[11] = "tutte";
    
    // iteratori for
    int i;
    int k = 0;
    int j = 0;
    int h = 0;
    
    int indexToken; 			// indice di scorrimento dei parametri
    int parOk = 0; 				// vale 1 dopo il controllo se i parametri sono corretti
    int almenoUnaRuota = 0; 	// ad 1 se almeno una delle ruote passate è tra quelle corrette
    int rit; 					// ritorno scriviGiocata
    
    // Gestione Timestamp
    time_t attuale;
    time(&attuale);
    
    
    
    // inizializzazione struttura dati
    iniDesSc(sc);
            
    printf(" $$$ Controllo parametri giocata\n");

    // Ci devono essere almeno 8 parametri
    if(cmdString[7]==NULL)
    {
        printf(" Parametri non validi\n");
        strcpy(risp," Parametri non validi\n");
        return -1;
    }
    
    // CONTROLLO PARAMETRI
    if(strcmp(cmdString[2],"-r")==0)
    {
        do
        {
            k++;
            // se non trovo -n tra i token num 3 e 14 o arrivo prima a null
            // allora -n manca
            if(k>11 || cmdString[2+k]==NULL)
            {
                break;
            }
        } while(strcmp(cmdString[2+k],"-n")!=0);
        // se -n manca o l'ho trovato arrivo comunque qui
        // se manca allora il token corrispondente a 2+k vale null o un valore 
        // non significativo
        // k deve essere diverso da 1 perchè tr -r e -n ci deve essere almeno un altro parametro
        if(cmdString[2+k]!=NULL && strcmp(cmdString[2+k],"-n")==0 && k!=1)
        {
            do
            {
                j++;
                // se non trovo -i tra i token [2+k+1 e 2+k+10] o arrivo prima a null
                // allora -i manca o ci sono troppi numeri giocati
                if(j>10 || cmdString[2+k+j]==NULL)
                {
                    break;
                }
            }while(strcmp(cmdString[2+k+j],"-i")!=0);
            // se -i manca o l'ho trovato arrivo comunque qui
            // se manca allora il token corrispondente a 2+k+j vale null o un valore 
            // non significativo
            // j deve essere diverso da uno perchè tra -n e -i ci deve almeno un altro parametro
            if(cmdString[2+k+j]!=NULL && strcmp(cmdString[2+k+j],"-i")==0 && j!=1)
            {
                // devo controllare il numero di giocate
                while(cmdString[2+k+j+h]!=NULL)
                {
                    h++;
                }
                
                // j-1 è il numero di numeri giocati, mentre h il numero di tipologie di giocata
                // non posso scommettere che escano più numeri di quanti ne ho giocati:
                // ad esempio non posso scomettere su una cinquina se ho giocato due numeri
                if((h-1)<=(j-1) && h<=6) // non ci possono essere inoltre più di 5 tipologie di giocata
                    parOk = 1;
            }
        }   
    }
                
    if(parOk==0)
    {
        printf(" Parametri non validi\n");
        strcpy(risp," Parametri non validi\n");
        return -1;
    }
    
    printf(" *** I parametri sono validi\n");
    
    // Perchè nella posizione 0 c'è il sessionID, in 1 il comando, in 2 -n, quindi parto da 3
    indexToken = 3;
    
    // SEGNO LE RUOTE GIOCATE
    do
    {
        for(i = 0; i<12; i++)
        {
            if(strcmp(cmdString[indexToken],ruote[i])==0)
            {
                sc->ruotaSchedina[i] = 1;
                almenoUnaRuota = 1; // serve per indicare che almeno una ruota indicata tra i parametri ruota è tra quelle corrette
            }
        }
        indexToken++;
    }while(strcmp(cmdString[indexToken],"-n")!=0);
    
    if(almenoUnaRuota==0) 
    {
        // significa che i parametri passati come ruota sono tutti sbagliati
        // (nel caso ce ne siano di sbagliati ma almeno uno è giusto continuo
        // ugualmente considerando solo il parametro corretto.)
        // Se tutti sono sbagliati non posso però continuare perchè la giocata non è valida
        printf(" Parametri non validi: Ruota errata\n");
        strcpy(risp," Parametri passati come ruota errati\n");
        return -1;
    }
    
    // SEGNO I NUMERI GIOCATI
    // controllo indexToken+1 perchè devo evitare l'assegnamento se nella
    // pos indexToken+1 c'è -i, sono comunque sicuro che almeno una volta
    // entrerò nel for per i controlli prima effettuati
    // (ci deve essere almeno un parametro tra -n e -i)
    for(i = 0; i<10 && strcmp(cmdString[indexToken+1],"-i")!=0;i++)
    {
        // la prima volta che entro qui cmdString[indexToken] == "-n" quindi devo incrementarlo prima di usarlo
        indexToken++;
        
        if(atoi(cmdString[indexToken])>90 || atoi(cmdString[indexToken])<1)   
        {
            // se c'è un numero sbagliato la schedina è invalida
            printf(" Parametri non validi: numero non valido\n");
            strcpy(risp," Parametri non validi\n");
            return -1;
        }
        sc->numeriGiocati[i] = atoi(cmdString[indexToken]);
    }
    indexToken++; // in modo che ora la pos indexToken contiene -i
    
    // SEGNO GLI IMPORTI GIOCATI
    // stesse considerazioni del for precedente
    for(i=0; i<5 && cmdString[indexToken+1]!=NULL; i++)
    {
        indexToken++;
        if(atof(cmdString[indexToken])<0)
        {
            // se c'è un importo sbagliato (ngativo) la schedina è invalida
            printf(" Parametri non validi: numero negativo\n");
            strcpy(risp," Parametri non validi\n");
            return -1;
        }
        sc->imp[i] = atof(cmdString[indexToken]);
    }
  
   
    // Scrivo la giocata sul file di log
    rit = scriviGiocata(sc,username,0,attuale);

    
    if(rit==0)
    {
        printf(" Errore nella giocata\n");
        strcpy(risp," Errore nella giocata\n");
        return -1;
    }
    
    printf(" *** Fine gestione comando giocata\n");
    strcpy(risp," Giocata Effettuata\n");
    return 1;    
}

// Calcola il fattoriale di un numero
int fact(int numero)
{
    int i, fattoriale;

    fattoriale = 1;
    
    if ((numero < 1) || (numero > 20))
    {
        return 1;
    }
    else
    {
 		for(i = numero; i > 1; i--)
          	fattoriale = fattoriale * i;
    }
    return  fattoriale;
}

// Ritorna il numero di possibili giocate generabili di tipo "tipo" su n numeri 
int posComb(int tipo, int* numGiocati)
{
    int n,k;
    int i;
    int comb;
    
    for(i=0; i<10; i++)
    {
        if(numGiocati[i]==0)
            break;
    }
    // Per trovare le possibili combinazioni devo usare le disposizioni semplici
    // Si dice "disposizione semplice di n oggetti a k a k" o anche "di classe k" 
    // (con k ≤ n) ogni allineamento con oggetti tutti distinti, di n oggetti a gruppi di k. 
    // nel nostro caso n è il numero di numeri giocati e k il tipo di giocata
    n = i;  	// numeri giocati
    k = tipo+1; // tipo a 0 indica estratto a 1 ambo ecc....
                // quindi devo incrementarlo
    
    // La formula è       	        n!
    //              	Dn,k = -----------
    //                     		  (n-k)!
    
    comb = fact(n)/fact(n-k);
    return comb;
}

// ritorna 1 se tutto è andato bene 0 altrimenti
int isWin(struct des_schedina* game, int estrazione[11][5], FILE* fptr, char* name)
{
    
    int all = 0;				// a 1 se ho giocato tutte le ruote (nella schedina attuale)
    int i, j, k;
    int ruoteGiocate = 0;
    int numeriIndovinati = 0;
    int ind[5]; 				// numeri indovinati

    double vittoria[5];
    vittoria[0] = 11.23;
    vittoria[1] = 250;
    vittoria[2] = 4500;
    vittoria[3] = 120000;
    vittoria[4] = 6000000;
    
    // vale 1 se ho allocato il descrittore della i-esima ruota 0 altrimenti.
    // Serve per allocare un unico descrittore per ruota
    int allocareUnaVolta = 0;
    
    
    // per ogni vittoria su ogni ruota salvo un record sul file
    // ho quindi bisogno di un descrittore per ogni vittoria.
    // Ogni vittoria della stessa giocata avrà il timestamp
    // uguale e ciò sarà usato per distinguere le vittorie della stessa giocata
    struct des_schedina* array_dessc[11];
    int ri_flock;
    int fd = fileno(fptr);
    
    time_t attuale;
    time(&attuale);
    
    for(i=0; i<11; i++)
        array_dessc[i] = NULL;
    
    
    
    // controllo se ho vinto - se è così creo una nuova struttura per ogni ruota su cui ho vinto
    // e salvo sul file
    if(game->ruotaSchedina[11]==1)
    {
        // ho giocato tutte le ruote
        all = 1;
    }
    
    for(i=0; i<11; i++)
    {
        for(j=0; j<5; j++)
            ind[j] = 0;
        
        if(game->ruotaSchedina[i]==1 || all==1)
        {
            ruoteGiocate++;
            
            // controllo i numeri
            for(j=0; j<10; j++)
            {
                for(k=0; k<5; k++)
                {
                    if(game->numeriGiocati[j] == estrazione[i][k])
                    {
                        ind[numeriIndovinati] = estrazione[i][k];
                        numeriIndovinati++;
                    }
                }
            }      
            
            // Controllo ogni puntata.
            // Vinco se numeriIndovinati >= (j+1) e se ho puntato su j 
            // dove j indica 0=estratto 1=ambo ec...
            // la vincita corrisponderà a vincita = vittoriaj*puntataj/possibilicombj
            // la vincita dovrà essere aggiornata dividendo per le ruoteGiocate
            // Esempio con estratto
            /* if(game->imp[0]!=0 && numeriIndovinati>=1)
            {
                // Ho preso l'estratto
                array_dessc[0] = (game->imp[0]*(vittoria[0]))/(posComb(0));
            }*/
            
            for(j=0; j<5; j++)
            {
                if(game->imp[j]!=0 && numeriIndovinati>=(j+1))
                {
                    if(allocareUnaVolta==0)
                    {
                        // Alloco la struttura dati che mi contiene le vincite per questa ruota
                        array_dessc[i] = malloc(sizeof(struct des_schedina));

                        // inizializzazione di default
                        iniDesSc(array_dessc[i]);

                        // ini con info attuali
                        array_dessc[i]->ruotaSchedina[i] = 1;

                        for(k=0; k<5; k++)
                            array_dessc[i]->numeriGiocati[k] = ind[k]; // il campo numerigiocati contiene ora i numeri indovinati
                        
                        // Devo allocare una volta perchè dall'eventuale seconda vincita in poi
                        // devo scrivere in questa struttura dati (che vale per la ruota attuale i)
                        allocareUnaVolta = 1;
                    }
                    array_dessc[i]->imp[j] += (game->imp[j]*(vittoria[j]))/(posComb(j,game->numeriGiocati));
                }
            }
        }
        // per la prossima ruota devo eventualmente allocare
        allocareUnaVolta = 0;
        // i numeri indovinati sulla prossima ruota devono partire da zero
        numeriIndovinati = 0;
    }
    
    // devo aggiornare tutte le vincite dividendole per il numero di ruote giocate
    for(i=0; i<11; i++)
    {
        if(array_dessc[i]!=NULL) // se è null su quella ruota non ho vinto
        {
            // se è diverso da NULL su quella ruota ho vinto qualcosa
            for(j=0; j<5; j++)
            {
                array_dessc[i]->imp[j] /= ruoteGiocate;
            }
        }
    }
    
    // Ora per ogni ruota su cui ho vinto ho una struttura dati des_schedina
    // che mi indica che su quella ruota ho vinto.
    // I campi della struttura dati sono quindi reinterpretati nel seguente modo:
    // 1)   Il campo ruoteSchedina contiene 1 solo sulla ruota corrispondente 
    //      (fatto prima in fase di ini): array_dessc[i]->ruoteSchedina[i] = 1
    // 2)   Il campo numeriGiocati contiene i numeri indovinati
    // 3)   Il campo imp contiene le vittorie per ogni tipo di giocata
    
    
    // devo chiudere il file perchè riuso la funzione che scrive le giocate: 
    // tale funzione apre il file, quindi lo devo chiudere prima di riaprirlo per evitare errori
    flock(fd,LOCK_UN);
    fclose(fptr);
    
    // Scrivo sul file e libero la memoria allocata con la free
    for(i=0; i<11; i++)
    {
        if(array_dessc[i]!=NULL)
        {
            
            scriviGiocata(array_dessc[i],name,1,attuale);
            free(array_dessc[i]);
        }
    }
    
    // riapro il file che avevo chiuso
    fptr = fopen(name,"a+");

    if(fptr==NULL)
        return 0;
    
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);

    if(ri_flock<0)
		return 0;
	

    return 1;
}

// Ritorna il timestamp dell'ultima estrazione effettuata (se il flag last è a 1)
// o della penultima (se il flag last è a 0)
void getLastTimestamp(time_t* ts, int last)
{
    FILE* fptr;
    int fine = 1;
    int i;
    time_t app;
    time_t prec = 0;
    int arr[55];
    int fd, ri_flock;

    *ts = 0;         // ts viene assegnato a 0 in caso di errori
    
    fptr = fopen("estrazioni.txt","r");
    
    if(fptr==NULL)
        return;
    
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);
    
    if(ri_flock<0)
    {
    	perror(" Errore nella flock");
    	return;
    }

    while(1)
    {
        fine = fscanf(fptr,"%li ",&app);
        
        if(fine<=0)
        {
        	flock(fd,LOCK_UN);
            fclose(fptr);
            
            // Uso la funzione anche in casi in cui ho già scritto l'ultima estrazione
            // e quindi l'ultima che mi serve è la penultima, in questi casi il flag last è a 0
            if(!last)
                *ts = prec;
            return;
        }
        
        for(i=0; i<55;i++)
            fscanf(fptr,"%d ",&arr[i]);
        fscanf(fptr,"\n");
        prec = *ts;
        *ts = app;
    }
}

// Controlla le vincite per l'estrazione
int controlloVincite(int estrazione[11][5], time_t ts_estrazione)
{
    FILE* f_reg;		// puntatore al file registrati.txt
    FILE* f_user;		// puntatore al file di log dell'utente
    
    // stringhe per username e nome file
    char user[50];
    char tmp[50];
    
    int rit;            //ritorno fscanf su registrati.txt
    int altreGiocate;   // ritorno fscanf sul file utente
    int giocata;        // a 0 se il record è una giocata 1 altrimenti
    time_t ts_giocata;
    time_t ts_lastControlledEstr;
    int i;
    struct des_schedina game;
    struct des_schedina spazzatura;     // serve per finire di leggere il record che non mi interessa
    int scarica = 0;                    // se 1 indica che il record non è significativo e devo scaricarlo in spazzatura
    int fdreg, fduser;
    int ri_flock;
    getLastTimestamp(&ts_lastControlledEstr,0);
    
    
    printf(" $$$ Controllo vincite in corso\n");
    
    // Devo aprire tutti i file degli utenti registrati e controllare se hanno vinto
    // per vedere gli utenti registrati guardo nel file registrati.txt
    f_reg = fopen("registrati.txt","r");
    
    if(f_reg==NULL)
        return 0;

    fdreg = fileno(f_reg);
    ri_flock = flock(fdreg, LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

    for(;;)
    {
        rit = fscanf(f_reg,"%s\n",user);
        
        // condizione di uscita
        if(rit<=0)
            break;
        
        strcat(user,".txt");
        
        // apro il file dell'utente per vedere le giocate che ha fatto e controllarle
        f_user = fopen(user,"a+");
        

        if(f_user==NULL)
            return 0;
        
        fduser = fileno(f_user);
        
        ri_flock = flock(fduser,LOCK_EX);

        if(ri_flock<0)
		{
			perror(" Errore nella flock");
			return 0;
		}

        // il primo valore letto è la password che non mi serve a nulla
        // pertanto la metto nel campo tmp
        altreGiocate = fscanf(f_user,"%s\n",tmp);
        
        for(;;)
        {
            altreGiocate = fscanf(f_user,"%d ",&giocata);
            
            // condizione di uscita
            if(altreGiocate<=0)
                break;
            
            // se il primo numero vale 1 è un record corrispondente a una vincita
            // quindi non mi interessa e vado al successivo
            if(giocata==1)
                scarica = 1;
            
            fscanf(f_user,"%li ",&ts_giocata);
            
            // se la condizione è soddisfatta significa che la giocata è stata fatta dopo (o durante) l'estrazione
            // quindi non deve essere considerata, inoltre siccome i record di giocata vengono memorizzati in ordine di arrivo
            // sicuramente i record successivi di giocata avranno un ts ancora maggiore di quello attuale
            // quindi potrei uscire da questo file, tuttavia per avere il puntatore del file alla prossima
            // riga (aggiornamento dell'I/O pointer) e non lasciarlo all'inizio, scarico e vado alla prossima iterazione
            if(ts_giocata>=ts_estrazione)
                scarica = 1;
            
            // se la condizione è soddisfatta significa che tale giocata è stata già controllata
            // perchè relativa a una estrazione precedente, devo quindi andare a controllare il prossimo record
            if(ts_giocata<ts_lastControlledEstr)
                scarica = 1;
            
            if(scarica)
            {
                for(i=0; i<12; i++)
                    fscanf(f_user,"%d ",&spazzatura.ruotaSchedina[i]);
            
                for(i=0; i<10; i++)
                    fscanf(f_user,"%d ",&spazzatura.numeriGiocati[i]);
            
                for(i=0; i<5; i++)
                    fscanf(f_user,"%lf ",&spazzatura.imp[i]);
            
                fscanf(f_user,"\n");
            }
            else
            {
                for(i=0; i<12; i++)
                    fscanf(f_user,"%d ",&game.ruotaSchedina[i]);
            
                for(i=0; i<10; i++)
                    fscanf(f_user,"%d ",&game.numeriGiocati[i]);
            
                for(i=0; i<5; i++)
                    fscanf(f_user,"%lf ",&game.imp[i]);
            
                fscanf(f_user,"\n");
                
                // isWin ritorna 0 se c'è stato un errore, in questo caso termino
                if(!isWin(&game,estrazione,f_user,user))
                    return 0;
            }
            // resetto scarica
            scarica = 0;
        }
        
        // ho finito di controllare l'utente
        flock(fduser,LOCK_UN);
        fclose(f_user);
    }
    
    // ho finito di controllare tutto
    flock(fdreg,LOCK_UN);
    fclose(f_reg);
        
    return 1;
}

// Effettua una estrazione e chiama controlloVincite()
void effettuaEstrazione()
{
    int i,j,k;
    int random;					// Numero random estratto
    int isEstratto;				// vale 1 quando l'estrazione ha avuto successo (se esce un numero già estratto devo ripetere)
    FILE* fptr;
    time_t ts_estrazione;
   	int fd;
    int r;						// Ritorno controlloVincite()
    int ri_flock;

    // Matrice in cui ogni riga rappresenta la ruota e ogni colonna un numero estratto
    // esempio: estrazione[i][j] = x -> significa che sulla ruota i è stato estratto il numero x
    //                                  nella posizione j-esima (questo ultima info non è significativa)
    int estrazione[11][5];
    
    
    time(&ts_estrazione);
  	
  	// Impostazioni per la rand
    srand(time(NULL));
    
    printf(" $$$ Inizio Funzione Estrazione\n");  
    
    // seleziono random 5 numeri diversi per una ruota
    for(i = 0; i<11; i++)
    {
        for(j = 0; j<5; j++)
        {
            do{
                isEstratto = 0;
                random = (rand()%90) + 1;
                
                // siccome il numero estratto non viene reinserito
                // nella ruota devo verificare che in quella ruota non esca
                // lo stesso numero, se è così devo effettuare una nuova estrazione
                // L'algoritmo simula l'estrazione casuale senza reinserimento
                for(k=0;k<j;k++)
                {
                    if(random==estrazione[i][k])
                    {
                        isEstratto = 1;
                        break;
                    }
                }
            }while(isEstratto!=0);
            estrazione[i][j] = random;
        }
    }
    
    printf(" $$$ Estrazione effettuata\n");   
    
    // scrivo il risultato in un file di log
    fptr = fopen("estrazioni.txt","a");

    if(fptr==NULL)
    {
        printf(" Errore nell'apertura del file estrazioni.txt: Impossibile salvare l'estrazione\n");
        return;
    }
    
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return;
	}

    fprintf(fptr,"%li",ts_estrazione);
    
    for(i=0; i<11; i++)
    {
        for(j=0; j<5; j++)
            fprintf(fptr," %d",estrazione[i][j]);
    }
    
    fprintf(fptr,"\n");
    
    printf( " $$$ Salvataggio dell'estrazione sul file di log effettuato\n");
    flock(fd,LOCK_UN);
    fclose(fptr);
    
    printf(" $$$ Controllo le vincite\n");
    r = controlloVincite(estrazione,ts_estrazione);
    
    if(r==1)
        printf(" $$$ Fine Controllo vincite\n");
    else
        printf(" $$$ Errore nel controllo delle vincite\n");
    return;
}

// Dato un indice ritorna la ruota corrispondente
char* getRuota(int index)
{
    switch(index)
    {
        case 0:
            return "Bari";
            break;
        case 1:
            return "Cagliari";
            break;
        case 2:
            return "Firenze";
            break;
        case 3:
            return "Genova";
            break;
        case 4:
            return "Milano";
            break;
        case 5:
            return "Napoli";
            break;
        case 6:
            return "Palermo";
            break;
        case 7:
            return "Roma";
            break;
        case 8: 
            return "Torino";
            break;
        case 9:
            return "Venezia";
            break;
        case 10:
            return "Nazionale";
            break;
    }
}

// Dato un indice ritorna il tipo di puntata corrispondente
char* getTipoPun(int index)
{
     switch(index)
    {
        case 0:
            return "Estratto";
            break;
        case 1:
            return "Ambo";
            break;
        case 2:
            return "Terno";
            break;
        case 3:
            return "Quaterna";
            break;
        case 4:
            return "Cinquina";
            break;
     }
}

// Ritorna il numero di estrazioni effettuate (utility per il comando vedi_estrazione n)
int quanteEstr()
{
	FILE* fptr;
	int fd;
	int ri_flock;
	time_t ts; 
	int i,j;
	int cont = 0;		// contatore
	int stop;			// Se minore o uguale a 0 indica l'uscita dal for(;;)
	int estr[11][5];

	fptr = fopen("estrazioni.txt","r");

	if(fptr==NULL)
	{
		printf(" Errore nell'apertura del file delle estrazioni\n");
		return 0;
	}

	fd = fileno(fptr);
	ri_flock = flock(fd,LOCK_EX);

	if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

	for(;;)
	{
		stop = fscanf(fptr,"%li ",&ts);
		
		if(stop<=0)
			break;

		cont++;

		for(i=0; i<11; i++)
        {
            for(j=0; j<5; j++)
                fscanf(fptr,"%d ",&estr[i][j]);
        }
	}

	flock(fd,LOCK_UN);
	fclose(fptr);
	return cont;
}

// Ritorna il numero di vincite o di giocate dell'utente passato come parametro
// in base al valore del flag vincita_giocata (0:giocata - 1:vincita)
int quantiRecord(char* username, int vincita_giocata)
{
    FILE* fptr;
    int contatore = 0;
    char nome_file[50];
    int rit;				// Ritorno fscanf
    char pw[50];
    int gv;
    int i;
    struct des_schedina tmp;
    int fd;
    int ri_flock;

    strcpy(nome_file,username);
    strcat(nome_file,".txt");
    
    fptr = fopen(nome_file,"r");
    
    if(fptr==NULL)
    {
        printf(" Errore nell'apertura del file - Username non valido\n");
        return 0;
    }
    
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

	// All'inizio del file c'è la password, anche se non mi serve devo leggerla
    rit = fscanf(fptr,"%s\n",pw);
    
    while(1)
    {
        rit = fscanf(fptr,"%d ",&gv);
        
        if(rit<=0)
            break;
        
        // Se il flag del record è uguale a quello passato all funzione
        // allora devo contarlo
        if(gv==vincita_giocata)
        {
            contatore++;
        }
        
        // Devo finire di leggere la riga
        for(i=0; i<12; i++)
            fscanf(fptr,"%d ",&tmp.ruotaSchedina[i]);
              
        for(i=0; i<10; i++)
            fscanf(fptr,"%d ",&tmp.numeriGiocati[i]);
            
        for(i=0; i<5; i++)
           fscanf(fptr,"%lf ",&tmp.imp[i]);
        
        fscanf(fptr,"\n");
    }

    flock(fd,LOCK_UN);
    fclose(fptr);
    return contatore;
}

// Handler del comando vedi_vincite
void vedivincite(char** cmdString, char* risp, char* username)
{
    FILE* fptr;             // puntatore al file
    char nameFile[50];      // nome del file
    char tmp[50];           // campo contente la password (non significativo)
    int firstTime = 1;      // indica se copiare il buffer di appoggio work (1) in risp o concatenarlo(0)
    int cont;               // ritorno della fscanf, se <=0 stop
    int gv;                 // 0 se è giocata 1 se vincita
    time_t ts_giocata;      // timestamp giocata
    time_t ts_prec;         // timestamp giocata iterazione precedente
    
    const int MAX_VISIBILI = quantiRecord(username,1)+1;                
    
    struct des_schedina game[MAX_VISIBILI];     // struttura dati che descrive una vincita su una ruota
    int isFirst[MAX_VISIBILI];                  // 1 se la vincita i-esima è la prima di quella estrazione
                
    time_t ts_win[MAX_VISIBILI];                // array ts vincite
    struct des_schedina spazzatura;             // serve per finire di leggere le righe da scartare
    int i,j,k;
    int num = 0;                                // contatore numero ruote vinte

    int scarica = 0;        // se 1 significa che devo buttare via il record che sto leggendo perchè non significativo        
    double totVincite[5];   // Array riassuntivo delle vincite per ogni tipo di giocata
    
    int fd;
    int ri_flock;

    // Array di appoggio per la formattazione della risposta
    char p1[150];
    char work[1000];
        
    for(i=0; i<5; i++)
        totVincite[i] = 0;
        
    if(cmdString[2]!=NULL)
    {
        printf(" *** Parametri eccessivi\n");
        strcpy(risp," Parametri eccessivi\n");
        return;
    }
   
    printf(" *** Visione vincite\n");
    strcpy(nameFile,username);
    strcat(nameFile,".txt");
  

    fptr = fopen(nameFile,"r");
  
    if(fptr==NULL || fptr <=0)
    {
        printf(" Errore nell'accesso al file\n");
        strcpy(risp," Errore nell'accesso al file\n");
        return;
    }
    
    fd = fileno(fptr);
    ri_flock = flock(fd, LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return;
	}

    // Leggo dal file
    // Il primo campo letto è la password che non mi serve 
    cont = fscanf(fptr,"%s\n",tmp);
          
    while(num<MAX_VISIBILI)
    {
        cont = fscanf(fptr,"%d ",&gv);
  
        // non ci sono altri record da guardare
        if(cont<=0)
            break;
        
        // mi interessano solo le vincite quindi se è una giocata vado al prossimo record
        // Tuttavia non posso fare break perchè devo finire di leggere la riga
        if(gv==0)
            scarica = 1;
           
        fscanf(fptr,"%li ",&ts_giocata);
        
        if(gv==1)
        {
            ts_win[num] = ts_giocata;
            
            if(num==0 || ts_giocata!=ts_prec)
            {
                isFirst[num] = 1; 
            }
            else
            {
                isFirst[num] = 0;
            }
        
            ts_prec = ts_giocata;
        }
        
        // Qui ci devo arrivare sempre perchè anche se non devo considerare il record
        // devo finire di leggere la riga
        if(scarica)
        {
            // Devo scartare la linea ma per leggere
            // la successiva devo finire di leggere questa
            for(i=0; i<12; i++)
                fscanf(fptr,"%d ",&spazzatura.ruotaSchedina[i]);
              
            for(i=0; i<10; i++)
                fscanf(fptr,"%d ",&spazzatura.numeriGiocati[i]);
            
            for(i=0; i<5; i++)
                fscanf(fptr,"%lf ",&spazzatura.imp[i]);
        }
        else
        {
            for(i=0; i<12; i++)
                fscanf(fptr,"%d ",&game[num].ruotaSchedina[i]);
              
            for(i=0; i<10; i++)
                fscanf(fptr,"%d ",&game[num].numeriGiocati[i]);
            
            for(i=0; i<5; i++)
                fscanf(fptr,"%lf ",&game[num].imp[i]);
            
            // contatore giocate
            num++;
        }
            
        fscanf(fptr,"\n");
        
        // devo resettare scarica
        scarica = 0;
    }
    
    flock(fd,LOCK_UN);                                                            
    fclose(fptr);
    
    if(num==0)
    {
        strcpy(risp," Nessuna Vincita da visualizzare\n");
    }
    
    // stampo vincite
    // il numero di vincite è num         
    for(i=0; i<num; i++)
    {
        
        
        if(i!=0 && isFirst[i])
        {
            sprintf(p1,"**********************************************\n");
            strcat(work,p1);
        }
        
        if(isFirst[i]==1)
            sprintf(p1," Estrazione del %s", ctime(&ts_win[i]));
        
        if(i==0)
            strcpy(work,p1);
        else
            strcat(work,p1);
        
        // Metto la ruota vincente
        for(j=0; j<11; j++)
        {
            if(game[i].ruotaSchedina[j]==1)
            {
                sprintf(p1," %s   \t",getRuota(j));
                strcat(work,p1);
                break;  // solo una ruota vincente per record
            }
        }
        
        // Metto i numeri
        for(j=0; game[i].numeriGiocati[j]!=0; j++)
        {
            sprintf(p1,"%d ",game[i].numeriGiocati[j]);
            strcat(work,p1);
        }
        
        sprintf(p1," \t >> \t");
        strcat(work,p1);
        
        // Metto le vincite
        for(j=0; j<5; j++)
        {
            // potrei non aver vinto nulla su un estratto (quindi game[i].imp[0]==0)
            // ma aver vinto sull'ambo (quindi game[i].imp[1]!=0)
            // per questo motivo devo usare il seguente if e non posso mettere nella
            // condizione del for di continuare finchè game[i].imp[j]==0 (non posso bloccarmi quando incontro 0 perchè dopo 
            // potrebbero esserci numeri diversi da 0)
            if(game[i].imp[j]!=0)
            {
                totVincite[j] += game[i].imp[j];
                sprintf(p1," %s %.2f ",getTipoPun(j),game[i].imp[j]);
                strcat(work,p1);
            }
        }
        
        strcat(work,"\n");
        
        if(i==(num-1))
        {
            sprintf(p1,"**********************************************\n");
            strcat(work,p1);
            
            for(k=0; k<5; k++)
            {
                sprintf(p1," Vittoria su %s: %.2f\n",getTipoPun(k), totVincite[k]);
                strcat(work,p1);
            }
          
        }
        // In work ho il record
        // Stampo il record prima di concatenarlo
        printf(" Record numero %d: %s\n",(i+1),work);
        if(firstTime==1)
        {
            strcpy(risp,work);
            firstTime = 0;
        }
        else
        {
            strcat(risp,work); 
        }
        memset(&work,0,1000);
        memset(&p1,0,150);
    } 
    printf(" *** Fine visione vincite: grandezza risposta: %ld\n",strlen(risp)+1);
    return;
}

// Handler del comando vedi_giocate
void vedigiocate(char** cmdString, char* risp, char* username)
{
    FILE* fptr;             // puntatore al file
    char nameFile[50];      // nome del file
    char tmp[50];           // campo contente la password (non significativo)
    int firstTime = 1;      // indica se copiare il buffer di appoggio (1) in risp o concatenarlo(0)
    int cont;               // ritorno della fscanf, se <=0 stop
    int gv;                 // 0 se è giocata 1 se vincita
    time_t ts_giocata;
    const int MAX_VISIBILI = quantiRecord(username,0);                
    
    struct des_schedina game[MAX_VISIBILI];     
    struct des_schedina spazzatura;             // serve per finire di leggere le righe da scartare
    int i,j;
    int num = 0;
    time_t ts_lastControlledEstr;
    
    int tipo;           // tipo di lettura 0 (giocate non attive) 1 (giocate attive)
    int scarica = 0;    // se 1 significa che devo buttare via il record che sto leggendo perchè non significativo        
    int fd;
    int ri_flock;
    getLastTimestamp(&ts_lastControlledEstr,1);
    
    if(cmdString[2]==NULL)
    {
        printf(" *** Manca il parametro tipo\n");
        strcpy(risp," Manca il parametro tipo\n");
        return;
    }
    
    tipo = atoi(cmdString[2]);
    
    if(tipo!=1 && tipo!=0)
    {
        printf(" *** Manca il parametro tipo\n");
        strcpy(risp," Parametro non valido");
        return;
    }
    
    printf(" *** Visione giocate\n");
    strcpy(nameFile,username);
    strcat(nameFile,".txt");

    fptr = fopen(nameFile,"r");
  	fd = fileno(fptr);
  	ri_flock = flock(fd,LOCK_EX);

  	if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return;
	}

    if(fptr==NULL || fptr <=0)
    {
        printf(" Errore nell'accesso al file\n");
        strcpy(risp," Errore nell'accesso al file");
        return;
    }
    
    // Leggo dal file
    // Il primo campo letto è la password che non mi serve 
    cont = fscanf(fptr,"%s\n",tmp);
          
    while(num<MAX_VISIBILI)
    {
        cont = fscanf(fptr,"%d ",&gv);
  
        // non ci sono altri record da guardare
        if(cont<=0)
            break;
        
        // mi interessano solo le giocate quindi se è una vincita vado al prossimo record
        // (devo però scaricare, ovvero finire di leggere la riga)
        if(gv==1)
            scarica = 1;
           
        fscanf(fptr,"%li ",&ts_giocata);    
     
        // voglio visionare le giocate relative a estrazioni già effettuate:
        // quindi devo scartare le giocate che hanno timestamp >= a quello
        // dell'ultima estrazione controllata perch sono quelle attive
        if(ts_giocata>=ts_lastControlledEstr && tipo==0)
            scarica = 1;
      
        // voglio visionare le giocate attive:
        // quindi devo scartare le giocate che hanno timestamp minore rispetto a
        // quello dell'ultima estrazione controllata perchè sono quelle non attive
        if(ts_giocata<ts_lastControlledEstr && tipo==1)
            scarica = 1;
        
 
        // Qui ci devo arrivare sempre perchè anche se non devo considerare la giocata
        // devo finire di leggere la riga
        if(scarica)
        {
            // Devo scartare la linea ma per leggere correttamente
            // la successiva devo finire di leggere questa
            for(i=0; i<12; i++)
                fscanf(fptr,"%d ",&spazzatura.ruotaSchedina[i]);
              
            for(i=0; i<10; i++)
                fscanf(fptr,"%d ",&spazzatura.numeriGiocati[i]);
            
            for(i=0; i<5; i++)
                fscanf(fptr,"%lf ",&spazzatura.imp[i]);
        }
        else
        {
            for(i=0; i<12; i++)
                fscanf(fptr,"%d ",&game[num].ruotaSchedina[i]);
              
            for(i=0; i<10; i++)
                fscanf(fptr,"%d ",&game[num].numeriGiocati[i]);
            
            for(i=0; i<5; i++)
                fscanf(fptr,"%lf ",&game[num].imp[i]);
            
            // contatore giocate
            num++;
        }
            
        fscanf(fptr,"\n");
        
        // devo resettare scarica
        scarica = 0;
    }
    
    flock(fd,LOCK_UN);                                                        
    fclose(fptr);
    
    if(num==0)
    {
        strcpy(risp," Nessuna Giocata di questo tipo\n");
    }

    // Stampo giocate
    // Il numero di giocate è num
    // 9(lenmaxruota)*11 + 10(numgiocati)*2 + 3(numero) + 8(lenmaxtipoimp)*5 + 4(lenmaxpunt)*5 = 182
    // arrotondando a 200, con un buffer di 4096 posso mantenere 20 giocate, essendo il buffer
    // allocato dinamicamente non ho limitazioni
     
    for(i=0; i<num; i++)
    {
        char p1[50];
        char work[256];
    
        sprintf(p1," %d) ", (i+1));
        
        strcpy(work,p1);
        
        // Metto le ruote
        if(game[i].ruotaSchedina[11]==1)
        {
            sprintf(p1,"Tutte ");
            strcat(work,p1);
        }
        else
        {
            for(j=0; j<11; j++)
            {
                if(game[i].ruotaSchedina[j]==1)
                {
                    sprintf(p1,"%s ",getRuota(j));
                    strcat(work,p1);
                }
            }
        }
        
        // Metto i numeri
        for(j=0; game[i].numeriGiocati[j]!=0; j++)
        {
            sprintf(p1,"%d ",game[i].numeriGiocati[j]);
            strcat(work,p1);
        }
        
        // Metto le puntate
        for(j=0; j<5; j++)
        {
            // potrei non aver puntato nulla su un estratto (quindi game[i].imp[0]==0)
            // e aver puntato sull'ambo (quindi game[i].imp[1]!=0)
            // per questo motivo devo usare il seguente if e non posso metterlo nella
            // condizione del for (non posso bloccarmi quando incontro 0 perchè dopo 
            // potrebbero esserci numeri diversi da 0)
            if(game[i].imp[j]!=0)
            {
                sprintf(p1,"* %s %.2f ",getTipoPun(j),game[i].imp[j]);
                strcat(work,p1);
            }
        }
        
        strcat(work,"\n");
        
        // In work ho il record
        // Stampo il record prima di concatenarlo
    
        if(firstTime==1)
        {
            strcpy(risp,work);
            firstTime = 0;
        }
        else
        {
            strcat(risp,work); 
        }
    }      
    
    return;
}

// Ritorna 0 se str non è una ruota, 1 se è una ruota o se str è null (tutte le ruote)
// Assegna a index il valore di indice corrispondente alla ruota str
int getIndexRuota(char* str, int* index)
{
    char* ruote[12]; 
    int i;
    
    if(str==NULL)
    {
        *index = 11;
        return 1;
    }
    ruote[0] = "bari";
    ruote[1] = "cagliari";
    ruote[2] = "firenze";
    ruote[3] = "genova";
    ruote[4] = "milano";
    ruote[5] = "napoli";
    ruote[6] = "palermo";
    ruote[7] = "roma";
    ruote[8] = "torino";
    ruote[9] = "venezia";
    ruote[10] = "nazionale";
    ruote[11] = "tutte";
    
    for(i=0; i<11; i++)
    {
        if(strcmp(ruote[i],str)==0)
        {
            *index = i;
            return 1;
        }
    }
    
    // arrivo qui se non ho trovato corrispondenza
    return 0;
}

// Handler comando vedi_estrazioni
void vediestr(char** cmdString, char* risp)
{
    FILE* fptr;
    int quante;			// Quante estrazioni leggere
    int indexRuota;
    const int LIMIT = 16384;
   	int fd;
    
    // Se n passato dal client *350 è >= LIMIT viene messo come valore limite -1
    // (viene allocato un buffer predefinito e inviato l'errore per n troppo grande).
    // Questo limite è inserito perchè non posso permettere all'utente di passare un valore di n troppo
    // grande. Se lo permettessi darei il controllo della memoria del server al client
    // che potrebbe passare un numero enorme e rendere il server inutilizzabile
    // Al contrario se n*350<LIMIT non ho limiti di stampa perchè alloco un array adatto
    // Il controllo di cui sopra è fatto anche in main.c, quindi arrivati in questa funzione
    // se il limite è superato ho gia un array di 4096 in cui devo solo mettere il messaggio di errore 

    // Per l'inizializzazione vedi dopo
    int MAX_ESTR_VIS;   
    
    // stringhe per assemblare la risposta
    char p1[50];
    char work[400];
    
    int i, j;           // iteratori
    time_t ts_estr;     // timestamp estrazione in stampa
    int estr[11][5];    // matrice estrazione in stampa
    int contatore = 0;
    int rit;
    int primaVolta = 1;
    
    // Per leggere le ultime n estrazioni devo iniziare a leggere dall'estrazione
    // (numEstr-n)esima esclusa (partendo da 1), inclusa (se parto da 0).
    // Se (numEsr-n)<0 leggo tutte le estrazioni che ci sono
    int numEstr;			// quante estrazioni ci sono nel file
    int partenzaLettura; 	// indica l'estrazione dalla quale si deve iniziare a contare (inclusa)
    int ri_flock;

    printf(" *** Funzione che gestisce la visualizzazione delle estrazioni\n");
    if(cmdString[2]==NULL)
    {
        printf(" Parametri non validi\n");
        strcpy(risp," Parametro non valido\n");
        return;   
    }
    
    if(getIndexRuota(cmdString[3],&indexRuota)==0)
    {
        printf(" Parametri non validi\n");
        strcpy(risp," Parametro non valido\n");
        return;
    }
    
    quante = atoi(cmdString[2]);
    
    if((quante*350)>=LIMIT)
    {
        MAX_ESTR_VIS = -1;
    }
    else
    {
        MAX_ESTR_VIS = LIMIT;   // devo assegnare a MAX_ESTR_VIS un numero molto grande
                                // assegno quindi limit perchè so che è irragiungibile
                                // visti i controlli precedenti (quante << LIMIT)
    }
    
    if(quante<=0 || quante>MAX_ESTR_VIS)
    {
        printf(" Parametro n troppo grande\n");
        strcpy(risp," Parametro n troppo grande\n");
        return;
    }
    
    numEstr = quanteEstr();
    partenzaLettura = numEstr - quante;
    // Se partenzalettura è minore o uguale di 0 significa che voglio leggere più estrazioni
    // di quelle che sono state fatte (o l'esatto numero di estrazioni effettuate).
    // In entrambi i casi stampo tutte quelle disponibili e devo quindi partire dalla prima (la 0esima)
    partenzaLettura = (partenzaLettura<=0)? 0 : partenzaLettura;
    printf(" *** Nel file estrazioni ci sono %d estr, devo leggere dalla %desima", numEstr, partenzaLettura);
    printf(" *** Lettura File Estrazioni\n");

    // Assemblo la risposta
    fptr = fopen("estrazioni.txt","r");
    
    if(fptr==NULL)
    {
        printf(" Errore nell'apertura del file di log delle estrazioni: può significare che non sono state fatte estrazioni\n");
        strcpy(risp," Al momento non è stata effettuata alcuna estrazione\n");
        return;
    }
    
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return;
	}

    printf(" *** Assemblaggio Risposta\n");

    while(contatore<numEstr)
    {
        rit = fscanf(fptr,"%li ",&ts_estr);
    	
    	// Estrazioni finite
        if(rit<=0)
            break;
    
        sprintf(p1," Estrazione del %s",ctime(&ts_estr));
        strcpy(work,p1);
    
        for(i=0; i<11; i++)
        {
            for(j=0; j<5; j++)
                fscanf(fptr,"%d ",&estr[i][j]);
        }

        fscanf(fptr,"\n");
        
        //printf(" cont:%d part:%d\n",contatore,partenzaLettura);
      	
      	if(contatore>=partenzaLettura)
      	{
        	for(i=0; i<11; i++)
        	{
            	if(i==indexRuota || indexRuota==11)
            	{
                	sprintf(p1," %s   \t",getRuota(i));
                	strcat(work,p1);
                	for(j=0; j<5; j++)
                	{
                    	sprintf(p1,"%d\t",estr[i][j]);
                    	strcat(work,p1);
                	}
                	strcpy(p1,"\n");
                	strcat(work,p1);
            	}
        	}
        	
        	// Se quante è uguale a 1 non devo mettere
        	// il \n per motivi di formattazione
        	if(quante>1)
        	{
            	strcpy(p1,"\n");
            	strcat(work,p1);
        	}
        
        	if(primaVolta)
        	{
            	strcpy(risp,work);
            	primaVolta = 0;
        	}
        	else
        	{
            	strcat(risp,work);
        	}
        }
        contatore++;
    }

    flock(fd,LOCK_UN);
    fclose(fptr);
    return;
}

//********************************************************************************************************************************
//		SEZIONE FUNZIONI DI UTILITÀ

int menu(char* cmd)
{
    //char* comandiAcc = {"!signup","!login","!invia_giocata","!vedi_giocate","!vedi_estrazione","!vedi_vincite"};
    int i;
    char* comandiAcc[7];
    
    comandiAcc[0] = "!signup";
    comandiAcc[1] = "!login";
    comandiAcc[2] = "!invia_giocata";
    comandiAcc[3] = "!vedi_giocate";
    comandiAcc[4] = "!vedi_estrazione";
    comandiAcc[5] = "!vedi_vincite";
    comandiAcc[6] = "!esci";
    
    for(i=0; i<7; i++)
    {
        if(strcmp(cmd,comandiAcc[i])==0)
        {
            return i;
        }
    }
    return 9; // numero casuale non deve essere nel range di comandi disponibili [0,6]
}

// Invalida (inizializza) il sessionId
void invalidaId(char* id)
{
    strcpy(id,"0000000000");
    printf(" *** Azzero il sessionId\n");
}

// disconnessione sicura
int disc(struct clientInfo* clinf)
{
    printf(" *** Disconnessione Sicura\n");
    // se prima di disconnettere il client il server deve fare altro inserire qui
    invalidaId(clinf->sessionId);
    return 1;
}

// Ritorna 0 se l'username è disponibile (non presente) 1 altrimenti (presente)
int isPresent(char* username)
{
    FILE* fptr;
    char work[50];
    
    strcpy(work,username);
    
    strcat(work,".txt");
    fptr = fopen(work,"r");
    
    if(fptr==NULL)
        return 0;   // l'username non è già presente
    else
        return 1;   // l'username è presente
}

// Permette di iscriversi
int signup(char** cmdString, char* risp)
{
    char nameFile[50];
    FILE* new_file;
    FILE* reg;
    char password[50];
    int fdnew, fdreg;
    int ri_flock;

    // Controllo i parametri passati
    if(cmdString[2]==NULL || cmdString[3]==NULL)
    {
        strcpy(risp,"Parametri non validi\n");
        return -1;
    }
    
    // 50 - \0 - .txt = 45 spazi disponibili
    if(strlen(cmdString[2])>45 || strlen(cmdString[3])>45)
    {
        printf(" Parametri troppo lunghi\n");
        strcpy(risp,"Parametri troppo lunghi\n");
        return -1;
    }
    
    printf(" *** I parametri sono <%s> <%s> \n",cmdString[2],cmdString[3]);
   
    // controllo se esiste un utente con lo stesso username
    if(isPresent(cmdString[2]))
    {
        printf(" Esiste già un utente <%s>\n",cmdString[2]);
        strcpy(risp,"L'username scelto non è disponibile\n");
        return 0;
    }
    
    // Creo un file registro relativo al nuovo utente
    
    strcpy(nameFile,cmdString[2]);
    strcpy(password,cmdString[3]);
    
    strcat(nameFile,".txt");
    
    printf(" *** Creo un nuovo File di nome '%s' in cui scrivo '%s'\n",nameFile,password);
    new_file = fopen(nameFile,"a");
    
    if(new_file==NULL)
    {
        printf(" Errore durante la creazione del file di registro\n");
        strcpy(risp,"Errore durante la creazione del file di registro\n");
        return 0;
    }

    fdnew = fileno(new_file);
    ri_flock = flock(fdnew,LOCK_EX);
    
    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

    printf(" *** File creato\n");
    
    fprintf(new_file,"%s\n",password);
    flock(fdnew,LOCK_UN);
    fclose(new_file);
    
    // Inserisco il nome utente registrato nel file registrati.txt che mi tiene traccia degli utenti registrati.
    // Tale file serve alla funzione che controlla le giocate vincenti, le giocate vengono salvate
    // nei file di log degli utenti, quindi devo sapere il nome dell'utente per risalire al file di log
    reg = fopen("registrati.txt","a");
    fdreg = fileno(reg);
    ri_flock = flock(fdreg, LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

    if(reg==NULL)
    {
        printf(" Errore durante l'apertura del file di registro\n");
        strcpy(risp,"Errore durante l'apertura del file di registro\n");
        return 0;
    }
    
    fprintf(reg,"%s ",cmdString[2]);
    flock(fdreg, LOCK_UN);
    fclose(reg);
    
    strcpy(risp,"Iscrizione avvenuta con successo\n");
    return 1;
}

// Generatore id di sessione
void generaIdSessione(char* sessionId)
{
    int i;
    int num;
    char strRandom[11];
    printf(" *** Funzione per generare il sessionId\n");
    srand(time(NULL));
    
    for(i = 0; i<10; i++)
    {
        do
        {
            num = (rand() % 42) + 48;
        }while(num>=58 && num<=65);
        
        // il numero casuale trovato viene considerato come codice ascii
        // viene quindi convertito in char
        // si saltano i numeri nell'intervallo [58,65] perchè sono caratteri speciali non alfanumerici
        // i caratteri alfanumerici si trovano nell'intervallo [48,57] U [66,90]
        strRandom[i] = (char)num;
    } 
    strRandom[10] = '\0';
    strcpy(sessionId,strRandom);
}

// Permette di effettuare il login
int login(char** cmdString, char* risp, struct clientInfo* clinf)
{
    FILE* fptr;
    char name_file[50];
    char pwSaved[50];
    char sessionId[10];
    char app[1024];
   	int fd;
   	int ri_flock;

     // Controllo i parametri passati
    if(cmdString[2]==NULL || cmdString[3]==NULL)
    {
        strcpy(risp,"Parametri non validi\n");
        return -1;
    }
    
    // 50 - \0 - .txt = 45 spazi disponibili
    if(strlen(cmdString[2])>45 || strlen(cmdString[3])>45)
    {
        printf("Parametri troppo lunghi\n");
        strcpy(risp,"Parametri troppo lunghi\n");
        return -1;
    }
    
    strcpy(name_file,cmdString[2]);
    strcat(name_file,".txt");
    fptr = fopen(name_file,"r");
   
    if(fptr==NULL)
    {
        clinf->tentativi--;
        if(clinf->tentativi==0)
        {
            printf(" *** Credenziali errate per la terza volta - Il client verrà disconnesso è bloccato per 30min\n");
            strcpy(risp,"Credenziali errate - Hai finito i tentativi - IP bloccato per 30min\n");
            return 0;
        }
        else
        {
            printf(" *** Credenziali errate - rimangono %d tentativi\n",clinf->tentativi);
            strcpy(app,"Credenziali errate\n");
            strcpy(risp,app);
            return 1;
        }  
    }
    
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

    fscanf(fptr,"%s",pwSaved);
    flock(fd,LOCK_UN);
    fclose(fptr);
    printf(" *** La password salvata è '%s'\n",pwSaved);

    if(strcmp(cmdString[3],pwSaved)==0)
    {
        printf(" *** Le password coincidono\n");
        
        // Genero il sessionid
        generaIdSessione(sessionId);
        // imposto il sessionid e lo associo all'username       
        strcpy(clinf->sessionId,sessionId);
        strcpy(clinf->username,cmdString[2]);
   
        printf(" sessionId:%s username:%s cmd:%s\n",clinf->sessionId,clinf->username,cmdString[2]);
        strcpy(risp," Accesso Riuscito - SessionID:");
        strcat(risp,sessionId);
        strcat(risp,"\n");
        printf(" ***%s",risp);


        return 1;
    }
    else
    {
        clinf->tentativi--;
        if(clinf->tentativi==0)
        {
            printf(" *** Credenziali errate per la terza volta - Il client verrà disconnesso è bloccato per 30min\n");
            strcpy(risp,"Credenziali errate - Hai finito i tentativi - IP bloccato per 30min\n");
            return 0;
        }
        else
        {
            printf(" *** Credenziali errate - rimangono %d tentativi\n",clinf->tentativi);
            strcpy(app,"Credenziali errate\n");
            //strcat(app,(char)clinf->tentativi);
            strcpy(risp,app);
            return 1;
        }
    }
    
}

int bloccaIP(struct clientInfo* clinf)
{
    FILE* fptr;
    time_t tmstamp;
    int fd;
    int ri_flock;
    time(&tmstamp);
   
    printf(" *** Blocco l'ip del client\n");
    
    fptr = fopen("ipBlocked.txt","a");
    
    if(fptr==NULL)
    {
        printf(" Errore nell'apertura del file\n");
        return 0;
    }
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

    fprintf(fptr,"%d %li\n",clinf->ip,tmstamp);
    flock(fd,LOCK_UN);
    fclose(fptr);    
    return 1;
}

int isIpBlocked(uint32_t ipClient)
{
    FILE* fptr;
    uint32_t ipFromFile;
    time_t blocked;
    time_t attuale;
    time_t diff;
    time(&attuale);
    int stop = 1;
    int fd;
    int ri_flock;

    printf(" *** Controllo se l'ip è bloccato\n");
    fptr = fopen("ipBlocked.txt","r");
    
    if(fptr==NULL)
    {
        printf(" *** L'IP del Client non è attualmente bloccato\n");
        return 0; // se non esiste il file ipBlocked.txt significa che non ci sono ip bloccati
    }
    
    fd = fileno(fptr);
    ri_flock = flock(fd,LOCK_EX);

    if(ri_flock<0)
	{
		perror(" Errore nella flock");
		return 0;
	}

    do
    {        
        stop = fscanf(fptr,"%d %li",&ipFromFile,&blocked);
        printf("     IP: %d\n",ipFromFile);
        printf("     Timestamp Blocco: %li aka %s",blocked,ctime(&blocked));
        printf("     Timestamp Attuale: %li aka %s",attuale,ctime(&attuale));    
        diff = attuale-blocked;
        printf("     Differenza: %li aka %s",diff,ctime(&diff));
        
        // Tutte le date sono espresse come secondi dal 1 Gennaio 1970 1:00:00
        // 1800 sono 30 minuti espressi in secondi
        if(ipFromFile==ipClient && diff<=1800)
        {
            printf(" *** L'IP del Client è attualmente in fase di blocco\n");
            flock(fd,LOCK_UN);
            fclose(fptr);
            return 1;
        }
        
    }while(stop!=-1);

    flock(fd,LOCK_UN);
    fclose(fptr);
    // Se arrivo qui significa che non ho trovato corrispondenze
    printf(" *** L'IP del Client non è attualemente bloccato\n");
    return 0;
}

// Ritorna un numero che corrisponde a un comando
// che necessita una dimensione di allocazione diversa da quella di default
int cambiareAllSize(char* str)
{
    if(str==NULL)
        return 0;
    
    if(strcmp(str,"!vedi_estrazione")==0 )
        return 1;
    else if(strcmp(str,"!vedi_vincite")==0)
        return 2;
    else if(strcmp(str,"!vedi_giocate")==0)
        return 3;
    else
        return 0;
}

int calcolaRisposta(char* buffer, char* risp, struct clientInfo* clinf)
{
    int num = 0;        // indice di token, usato per la separazione dei comandi
    char* token[30];    // puntatore a stringhe - conterrà il comando e i parametri
    int ok;             // gestisce il ritorno delle funzioni standard
    int conn = 1;       // ritorno della funzioni che possono provocare disconnessione - viene messo a zero se devo uscire
    int i;
    
    for(i=0; i<30; i++)
        token[i] = NULL;
    
    printf(" *** Separo i vari comandi\n");
    // Primo token, è il sessionid
    // la separazione con \n mi permette di toglierlo e non considerarlo nei confronti
    // oltre che acquisire eventuali parametri successivi tramite i successivi split
    token[num] = strtok(buffer, " \n");
  
    // Altri token, ne esiste almeno uno ovvero il comando, gli altri sono gli eventuali parametri
    while( token[num] != NULL )
    {
      printf("     <%s>\n", token[num]);
      num++;
      token[num] = strtok(NULL, " \n\0");
      // il \0 serve per acquisire anche l'ultimo parametro
    }
   
    
    switch(menu(token[1]))
    {
        case 0:
            printf(" *** Ricevuto comando di iscrizione\n");
            ok = signup(token,risp);
            break;
        case 1:
            printf(" *** Ricevuto comando di login\n");
            conn = login(token,risp,clinf);
            if(clinf->tentativi==0)
                bloccaIP(clinf);
            break;
        case 2:
            if(strcmp(clinf->sessionId,token[0])==0)
            {
                printf(" *** Ricevuto comando di giocata\n");
                conn = giocata(token,risp,clinf->username);
            }
            else
            {
                strcpy(risp,"SessionId non valido\n");
            }
            break;
        case 3:
            if(strcmp(clinf->sessionId,token[0])==0)
            {
                printf(" *** Ricevuto comando di visione giocata\n");
                vedigiocate(token,risp,clinf->username);
            }
            else
            {
                strcpy(risp,"SessionId non valido\n");
            }
            break;
        case 4:
            if(strcmp(clinf->sessionId,token[0])==0)
            {
                printf(" *** Ricevuto comando di visione estrazioni\n");
                vediestr(token,risp);
            }
            else
            {
                strcpy(risp,"SessionId non valido\n");
            }
            break;
        case 5:
            if(strcmp(clinf->sessionId,token[0])==0)
            {
                printf(" *** Ricevuto comando di visualizzazione vincite\n");
                vedivincite(token,risp,clinf->username);
            }
            else
            {
                strcpy(risp,"SessionId non valido\n");
            }
            break;
        case 6:
            printf(" *** Ricevuto comando di uscita\n");
            ok = disc(clinf);
            if(ok)
            {
                strcpy(risp,"Disconnessione avvenuta con successo\n");
                conn = 0;
            }
            else
            {
                strcpy(risp,"Errore nella disconnessione\n");
            }
            break;
        default:
            strcpy(risp,"Comando non valido\n");
            break;
    }
    
    for(i=0; i<24;i++)
    {
        memset(&token[i],0,sizeof(token[i]));
    }
    return conn;
}



//********************************************************************************************************************************
// 		SERVER

void benvenuto()
{
	printf("************************************* SERVER LOTTO *************************************\n");
	printf(" --- Messaggi Processo Server Padre ---\n");
	printf(" $$$ Messaggi Processo Figlio deputato alle estrazioni\n");
	printf(" *** Messaggi Processi Figli deputati alla gestione delle connessioni con i client\n");
	printf(" !!! Messaggi di Errore di connessione\n");
	printf(" Messaggi di Errore generici\n");
	printf("****************************************************************************************\n");
}

int main(int argc, char* argv[]){
    int sckAscolto, sckCom;     // identificativi socket
    
    int ret;                    // gestisce i ritorni delle funzioni
    
    int lenRicevuto;            // lunghezza msg ricevuto
    socklen_t lenStruct;        // lunghezza struttura dati
    int lenRisp;                // lunghezza risposta
    uint16_t lmsg;              // lunghezza nella rete
    
    struct sockaddr_in my_addr, cl_addr; // strutture dati per la connessione
    
    // buffer di servizio
    char buffer[1024];
    
    // buffer, allocato successivamente, di risposta
    char* risposta;
    const int LIMIT = 16384; // 2^14 = 16KB  
    
    // id dei processi creati
    pid_t pid;
    
    // Porta di ascolto del server
    int porta_srv;  
    
    int periodo = 300; // 300sec = 5min, periodo di default
    
    if(argc>1 && argv[1]!=NULL)
    {
        porta_srv = atoi(argv[1]);
        if(argc==3)
            periodo = atoi(argv[2]);
    }
    else
    {
    	printf(" Parametri mancanti - Sintassi corretta: ./server <porta> <periodo>\n");
    	printf(" NB: Periodo è opzionale se non indicato vale 5 minuti\n");
    	return 0;
    }

    benvenuto();

    // Creo il socket
    sckAscolto = socket(AF_INET, SOCK_STREAM, 0);
    if(sckAscolto<0)
    {
        perror(" !!! Errore nella creazione del socket");
        exit(-1);
    }
    else
    {
        printf(" --- Socket di ascolto creato ---\n");
    }
    
    // Creo l'indirizzo di bind
    memset(&my_addr, 0, sizeof(my_addr)); // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(porta_srv);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    
    // Collego il socket con la bind
    ret = bind(sckAscolto, (struct sockaddr*)&my_addr, sizeof(my_addr) );
    
    if(ret < 0){
        perror(" !!! Errore in fase di bind");
        exit(-1);
    }
    else
    {
        printf(" --- Bind effettuata correttamente ---\n");
    }
    
    // metto il socket in ascolto
    ret = listen(sckAscolto, 10);
    
    if(ret < 0){
        perror(" !!! Errore in fase di listen");
        exit(-1);
    }
    else
    {
        printf(" --- Listen effettuata: Server in ascolto ---\n");
    }
    
    // Creo un processo figlio che si occupa di effettuare le estrazioni
    pid = fork();  
    if(pid==0)
    {
        //sono nel processo figlio deputato all'estrazione
        printf(" $$$ Processo Figlio deputato all'estrazione\n");
       
        while(1)
        {
            sleep(periodo);
            printf(" $$$ Estrazione in corso\n");
            effettuaEstrazione(); // effettua l'estrazione - controlla le vincite e salva nel file di log
        }
        return 1;
    }
    
    while(1){    
        // Accetto nuove connessioni
        // sckCom è il socket che mi serve per comunicare
        printf(" --- In attesa di nuove connessioni ---\n");
        lenStruct = sizeof(cl_addr);
        sckCom = accept(sckAscolto, (struct sockaddr*) &cl_addr, &lenStruct);
        
        if(sckCom<0)
        {
            perror(" !!! Errore nell'accept");
            exit(-1);
        }
        else
        {
            printf(" --- Connessione accettata ---\n");
        }
        
        pid = fork();
        
        if(pid<0)
        {
            perror(" !!! Errore nella fork");
            exit(-1);
        }
        
        if(pid==0)
        {
            // VARIABILI DI SESSIONE MANTENUTE NELLA STRUTTURA DATI clientInfo
            struct clientInfo clinf;
            clinf.tentativi = 3; 
            clinf.ip = cl_addr.sin_addr.s_addr;
           
            // la funzione invalidaId invalida l'id, in questo caso l'invalidazione vale come inizializzazione
            invalidaId(clinf.sessionId);
            
            //sono nel processo figlio
            printf(" *** PROCESSO FIGLIO ***\n");
            // chiudo il socket in ascolto perchè server al padre
            // io sono il figlio e uso new_sd
            close(sckAscolto);
            printf(" *** Chiudo il socket in ascolto\n");
            
            while(1)
            {
                int mantieniConn = 1; // ritorno della calcolaRisposta vale 1 se mantengo la conn, 0 se devo uscire
                
                // Ricevo la lunghezza del buffer dal client e la metto in lmsg
                ret = recv(sckCom,(void*)&lmsg,sizeof(uint16_t),0);
                
                if(ret < 0)
                {
                    perror(" Errore in fase di ricezione della lunghezza della stringa");
                    continue;
                }
                
                lenRicevuto = ntohs(lmsg); 
                if(lenRicevuto<=0)
                    continue;
                
                // ricevo la stringa
                ret = recv(sckCom, (void*)buffer, lenRicevuto, 0);
                
                
                if(ret < 0)
                {
                    perror(" Errore in fase di ricezione");
                    continue;
                }
                
                printf(" *** Ho ricevuto %s",buffer);
               
                // In buffer ho il messaggio ricevuto
                // calcolo la risposta da inviare al client
                if(isIpBlocked(clinf.ip)!=1)
                {
                    // Devo allocare il buffer di risposta.
                    // La dimensione di default è 4096
                    // la cambio solo nel caso in cui il comando 
                    // è vedi_estrazione - vedi_vincite - vedi_giocate
                    int ALLOCATION_SIZE = 4096 * sizeof(char);
                    int s;              // estrazioni che vorrebbe il comando
                    int tipocambio;     // tipologia di comando per cui devo cambiare
                    int quantiRec;      // quanti record vincita o giocata
                    
                    // array di appoggio per leggere comando e parametri
                    char p1[50];
                    char p2[50];
                    char p3[50];
                    
                    sscanf(buffer,"%s %s %s",p1,p2,p3);
                    tipocambio=cambiareAllSize(p2);
                    
                    // Per info sulle costanti moltiplicative guardare la sezione sulle strutture dati
                    if(tipocambio>0)
                    {
                        if(tipocambio==1 && p3!=NULL)
                        {
                            s = atoi(p3);
                            s = s * SIZE_ESTR_COMPLETA;
                            
                            // vedere la funzione vediestr nella sezione utilità lotto per maggiori info 
                            if(s>0 && s<LIMIT)
                                ALLOCATION_SIZE = s*sizeof(char);
                        }
                        else if(tipocambio==2)  // Vincita
                        {
                            quantiRec = quantiRecord(clinf.username,1);
                            
                            if(quantiRecord != 0)
                                ALLOCATION_SIZE = ((quantiRec*SIZE_VINCITA_COMPLETA)+MARGINE_ERRORE+SIZE_RECAP_WIN)*sizeof(char);
                        }
                        else if(tipocambio==3)  // GIOCATE
                        {
                            quantiRec = quantiRecord(clinf.username,0);
                            
                            if(quantiRecord!=0)
                                ALLOCATION_SIZE = ((quantiRec*SIZE_GIOCATA_COMPLETA)+MARGINE_ERRORE)*sizeof(char);
                        }
                    } 
                    risposta = (char*)malloc(ALLOCATION_SIZE);
                    mantieniConn = calcolaRisposta(buffer,risposta,&clinf);
                }
                else
                {
                    int ALLOCATION_SIZE = 4096 * sizeof(char);
                    risposta = (char*)malloc(ALLOCATION_SIZE);
                    mantieniConn = 0;
                    strcpy(risposta,"Il tuo IP è bloccato perchè hai inserito credenziali errate");
                }
                
                // intercetta eventuali errori non gestiti
                if(risposta==NULL)
                    strcpy(risposta,"Qualcosa è andato storto");
                
                printf(" *** Invio in risposta: %s\n",risposta);
                
                // calcolo la lunghezza del messaggio di risposta
                lenRisp = strlen(risposta)+1; // aggiungo +1 perchè voglio inviare anche il terminatore di stringa
               
                lmsg = htons(lenRisp);
                
                //invio lunghezza risposta
                ret = send(sckCom, (void*)&lmsg, sizeof(uint16_t),0);
                if(ret < 0){
                    perror(" Errore in fase di invio");
                    continue;
                }
                
                // Invio risposta
                ret = send(sckCom, (void*)risposta, lenRisp, 0);
            
                if(ret < 0){
                    perror(" Errore in fase di invio");
                    continue;
                }
                
                // resetto i buffer di servizio
                memset(&buffer, 0, 1024);
                free(risposta);
                if(mantieniConn==0)
                    break;
            }   
            
            ret = close(sckCom);
            
            if(ret<0)
            {
                perror(" Errore nella chiusura del socket di ascolto");
                exit(-1);
            }
            else
            {
                printf(" *** Chiuso il socket di comunicazione\n");
            }
            exit(1);
        }
        else
        {        
            // chiuo il socket sckCom perchè lo gestisco nel processo figli
            // e nel padre non mi serve
            printf(" --- Chiudo il socket di comunicazione che è gestito dal figlio ---\n");
            close(sckCom);
        }
    }

    return 1;
}
    
    
    
    


