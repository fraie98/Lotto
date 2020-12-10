#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Ritorna 1 se il client deve disconnettersi 0 altrimenti
int devoChiudere(char* risp)
{
    if( (strcmp(risp,"Disconnessione avvenuta con successo\n")==0)
        || (strcmp(risp,"Credenziali errate - Hai finito i tentativi - IP bloccato per 30min\n")==0)
        || (strcmp(risp,"Il tuo IP è bloccato perchè hai inserito credenziali errate\n")==0))
        return 1;
    else
        return 0;
}

// Funzione che gestisce il comando !help
void help(char* par)
{
    if(par==NULL)
    {
        printf(" COMANDI DISPONIBILI:\n");
        printf(" Per avere informazioni dettagliate su un comando lanciare il comando !help <comando>\n");
        printf(" 1) !signup <username> <password>  \t Crea un nuovo utente\n");
        printf(" 2) !login <username> <password>   \t Permette di autenticare un utente\n");
        printf(" 3) !invia_giocata <g>             \t Invia una giocata g al server\n");
        printf(" 4) !vedi_giocate <tipo>           \t Permette di vedere le giocate attive (tipo==1) o passate (tipo==0)\n");
        printf(" 5) !vedi_vincite                  \t Permette di vedere le vincite relative alle giocate dell'utente (se esistono)\n");
        printf(" 6) !vedi_estrazione <n> <ruota>   \t Mostra le ultime n estrazioni sulla ruota specificata\n");
        printf(" 7) !esci                          \t Permette di uscire dal programma\n");
    }
    else
    {
        if(strcmp(par,"!signup")==0)
        {
            printf(" !signup <username> <password>\n");
            printf(" Se non esiste già un utente con username <username> crea un nuovo utente\n");
        }
        else if(strcmp(par,"!login")==0)
        {
            printf(" !login <username> <password>\n");
            printf(" Permette di autenticare un utente, nel caso in cui si sbagliano le credenziali per 3 volte di fila\n");
            printf(" l'indirizzo IP del client viene bloccato per 30 minuti\n");
        }
        else if(strcmp(par,"!invia_giocata")==0)
        {
            printf(" !invia_giocata <g>\n");
            printf(" Invia una giocata g al server, il formato della giocata deve essere:\n");
            printf(" -r nomeRuota0 nomeRuota1 ... nomeRuota(N-1) -n numGiocato0 numGiocato1 ... numGiocato9 -i puntata0 puntata1 .... puntata4\n");
            printf(" Deve essere presente almeno una ruota, almeno un numero giocato e almeno una puntata.\n");
            printf(" Si può puntare su una tipologia di giocata solo se si sono giocati abbastanza numeri\n");
            printf(" Esempio: non posso puntare su una cinquina se ho giocata 3 numeri\n");
        }
        else if(strcmp(par,"!vedi_giocate")==0)
        {
             printf(" !vedi_giocate <tipo>\n");
             printf(" Permette di vedere le giocate attive (tipo==1) o passate (tipo==0)\n");
        }
        else if(strcmp(par,"!vedi_estrazione")==0)
        {
            printf(" !vedi_estrazione <n> <ruota>\n");
            printf(" Mostra le ultime n estrazioni sulla ruota specificata, se non è specificata alcuna ruota le mostra tutte\n");
        }
        else if(strcmp(par,"!vedi_vincite")==0)
        {
            printf(" !vedi_vincite\n");
            printf(" Permette di vedere le vincite relative alle giocate dell'utente (se esistono)\n");
        }
        else if(strcmp(par,"!esci")==0)
        {
            printf(" !esci\n");
            printf(" Permette di uscire\n");
        }
        else
        {
            printf(" Comando non valido help\n");
        }
        printf("\n");
    }
}

void benvenuto()
{
    printf("****************************************************** CLIENT LOTTO ******************************************************\n");
    help(NULL);
    printf("**************************************************************************************************************************\n\n");
}

// CLIENT
int main(int argc, char* argv[]){
    int sd;                     // id socket
    int ret;                    // gestisce il ritorno delle funzioni
    int len;                    // lunghezza messaggio da inviare
    int size;                   // lunghezza risposta del server
    uint16_t sizeMsgServer;     // lunghezza in rete
    
    char sessionID[11];         // sessionId
    
    // inizializzazione sessionID
    strcpy(sessionID,"000000000");
        
    struct sockaddr_in srv_addr;
    char buffer[1024];
    char* risp;
    uint16_t lmsg;
    
    char* ip_srv;
    int porta_srv;
  
    if(argc>1 && argv[1]!=NULL && argv[2]!=NULL)
    {
        ip_srv = argv[1];
        porta_srv = atoi(argv[2]);
    }
    else
    {
        printf(" Parametri mancanti - Sintassi corretta: ./client <ipServer> <portaServer>\n");
        return 0;
    }
   
    benvenuto();
    
    // Creo il socket
    sd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Creazione indirizzo del server
    memset(&srv_addr, 0, sizeof(srv_addr)); // Pulizia 
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(porta_srv);
    inet_pton(AF_INET, ip_srv, &srv_addr.sin_addr);
    
    // connetto il socket
    ret = connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    
    if(ret < 0){
        perror("Errore in fase di connessione: \n");
        exit(-1);
    }
    
    
    while(1){
        int num = 0;            // serve come contatore per split_vect
        char* split_vect[4];    // vettore che contiene le parti di msg da splittate
        char tosend[1024];      // contiene il buffer da inviare al server
        char helpcmd[1024];     // vettore di appoggio per intercettare il comando help
                                // non posso effettuare lo split su tosend altrimenti modifico la stringa
        int i;                  // var iteratore
        
        strcpy(buffer,sessionID);
        strcat(buffer," ");
       
        // Attendo input da tastiera
        printf(">");
        fgets(tosend,1024,stdin);
        
        strcpy(helpcmd,tosend);
        
        // Se il comando è help la gestione è locale
        split_vect[num] = strtok(helpcmd, " \n");  
        while( split_vect[num] != NULL && strcmp(split_vect[0],"!help")==0)
        {       
            num++;
            if(num>3)
                break;      
            split_vect[num] = strtok(NULL, " \n\0");
        } 
          
        if(strcmp(split_vect[0],"!help")==0)
        {
            help(split_vect[1]);
            continue;
        }
        
        // resetto i parametri usati
        num = 0;
        for(i=0; i<4; i++)
            split_vect[i] = NULL;

        // Concateno al sessionid in buffer il resto del messaggio da inviare al server
        strcat(buffer,tosend);
        
        len = strlen(buffer)+1; // aggiungo +1 per inviare anche il terminatore di stringa
        lmsg = htons(len);
        
        // invio al server la lunghezza della stringa
        ret = send(sd,(void*)&lmsg,sizeof(uint16_t),0);
        
        // Invio al server la stringa
        ret = send(sd, (void*) buffer, len, 0);
     
        if(ret < 0){
            perror("Errore in fase di invio: \n");
            exit(-1);
        }
        
        // Attendo risposta
        // lunghezza risposta
        ret = recv(sd, (void*)&sizeMsgServer, sizeof(uint16_t), 0);
        
        if(ret < 0){
            perror("Errore in fase di ricezione: \n");
            exit(-1);
        }
        
        size = ntohs(sizeMsgServer);
        
        // alloco il buffer contenente la risposta
        risp = (char*)malloc(sizeof(char)*size);
       
        if(risp==NULL)
        {
            printf(" Memoria Terminata\n");
            break;
        }
        
        // risposta dal server
        ret = recv(sd, (void*)risp, size, 0);
        
        if(ret < 0){
            perror("Errore in fase di ricezione: \n");
            exit(-1);
        }
        
        printf("%s\n", risp);
        
        
        // Quando faccio il login per la prima volta in una sessione
        // devo ricavare dalla risposta del server il sessionID
        split_vect[num] = strtok(risp, ":");
  
        while( split_vect[num] != NULL)
        {       
            num++;
            // Si entra qui dentro anche quando si riceve l'estrazione,
            // tale situazione viene riconosciuta nel momento in cui num è > di 3
            if(num>3)
                break;
            
            split_vect[num] = strtok(NULL, ":\n\0");
            // il \0 serve per acquisire anche l'ultimo parametro
        }

        if(strcmp(split_vect[0]," Accesso Riuscito - SessionID")==0)
        {       
            strcpy(sessionID,split_vect[1]);
        }

        // se ho la risposta è una delle seguenti stringhe devo terminare
        if(devoChiudere(risp)==1)
        {
            break;
        }
        memset(&buffer, 0, 1024);
        free(risp);
        memset(&tosend, 0, 1024);
    }
    
    close(sd);
        
}
    
    
    
    

