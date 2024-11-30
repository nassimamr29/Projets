


/* fichiers de la bibliothèque standard */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
/* bibliothèque standard unix */
#include <unistd.h> /* close, read, write */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <dirent.h>
#include <errno.h>
#include <pthread.h> // thread 
#include <semaphore.h> // semaphore
#include <time.h> 
/* spécifique à internet */
#include <arpa/inet.h> /* inet_pton */
/* spécifique aux comptines */
#include "comptine_utils.h"


#define PORT_WCP 4321


/** Structure contenant les arguments nécessaire pour que le threads execute les tâches;
 * - un nom de répertoire;
 * - un descripteur de socket pour parler au client;
 * - la structure du client (pour les log);
 * - le moment d'execution du accept(pour les log);
 * - le descripteur de fichier des log.
 */
struct argument{
	char *dir_name;
	int sock;
	struct sockaddr_in client;
	time_t time;
	int log;
};

void usage(char *nom_prog)
{
	fprintf(stderr, "Usage: %s repertoire_comptines\n"
			"serveur pour WCP (Wikicomptine Protocol)\n"
			"Exemple: %s comptines\n", nom_prog, nom_prog);
}
/** Retourne en cas de succès le descripteur de fichier d'une socket d'écoute
 *  attachée au port port et à toutes les adresses locales. */
int creer_configurer_sock_ecoute(uint16_t port);

/** Écrit dans le fichier de desripteur fd la liste des comptines présents dans
 *  le catalogue c comme spécifié par le protocole WCP, c'est-à-dire sous la
 *  forme de plusieurs lignes terminées par '\n' :
 *  chaque ligne commence par le numéro de la comptine (son indice dans le
 *  catalogue) commençant à 0, écrit en décimal, sur 6 caractères
 *  suivi d'un espace
 *  puis du titre de la comptine
 *  une ligne vide termine le message */
void envoyer_liste(int fd, struct catalogue *c);

/** Lit dans fd un entier sur 2 octets écrit en network byte order
 *  retourne : cet entier en boutisme machine. */
uint16_t recevoir_num_comptine(int fd);

/** Écrit dans fd la comptine numéro ic du catalogue c dont le fichier est situé
 *  dans le répertoire dirname comme spécifié par le protocole WCP, c'est-à-dire :
 *  chaque ligne du fichier de comptine est écrite avec son '\n' final, y
 *  compris son titre, deux lignes vides terminent le message */
void envoyer_comptine(int fd, const char *dirname, struct catalogue *c, uint16_t ic);

/** Fonction s'occupant des threads prenant une structure comme argument 
 *  envoyer_liste(a->sock,c);
 *	recevoir_num_comptine(a->sock);
 *	envoyer_comptine(a->sock,a->dir_name,c,nc);
 */
void *multi(void *arg);

/** Fonction s'occupant d'enregistrer les logs de connexion et les comptines demandées
 *  -struct argument qui contient l'adresse du client;
 *  -int nc = numéro de la comptine demandé
 */
int log_connect(struct argument* a, int nc );


// Pour l'écriture et les threads
pthread_mutex_t lock;


int main(int argc, char *argv[]){

	if (argc != 2) {
        usage(argv[0]);
        return 1;
    }

	//Crée La socket d'écoute du serveur
	int sock = creer_configurer_sock_ecoute(PORT_WCP);
	if(sock < 0){
		perror("erreur serv->main()->creer_config_sock");
		exit(1);
	}

	// Mutex pour le fichiers de log;
	pthread_mutex_init(&lock, NULL);

	// Création du fichiers de log 
	int fd_log;
    fd_log = open("Log.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); // on le rénitialise a chaque fermeture de serv;
	if (fd_log == -1) {
        perror("Erreur création\n");
        return 1;  
   	}

	while(1){

		//Création sockaddr client
		struct sockaddr_in addr_clt;
		socklen_t sl = sizeof(addr_clt);

		//Accepter l'écoute
		int a = accept(sock,(struct sockaddr *)&addr_clt,&sl);
		if(a<0){
			perror("erreur serv->main()->accept");
			exit(1);
		}

		time_t now;	// LOG
    	time(&now); // LOG

		// Préparation de l'argument (thread & log);
		struct argument *arg= malloc(sizeof(struct argument));
		arg->dir_name = argv[1];
		arg->sock= a;
		arg->client = addr_clt;
		arg->time = now;
		arg->log = fd_log;

		//Lancement du thread
		pthread_t thread;
		int ret=pthread_create(&thread,NULL,multi,arg);
		if(ret){
			fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
        	exit(2);
		}

		pthread_detach(thread);	// pas de pthread join car bloquant 

	}	

	pthread_mutex_destroy(&lock);	// mutex pour les logs 
	close(fd_log);
	close(sock);
	return 0;
}

int creer_configurer_sock_ecoute(uint16_t port){

	int n =socket(AF_INET,SOCK_STREAM,0);
	if(n < 0){
		perror("error creation socket");
		exit(1);
	}

	struct sockaddr_in sa ={.sin_family= AF_INET,
	.sin_port =htons(port),
	.sin_addr.s_addr= htonl(INADDR_ANY)
	};

	int opt = 1;
	setsockopt(n, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if(bind(n,(struct sockaddr *) &sa , sizeof(sa))<0){
		perror("error bind");
		close(n);
		exit(1);
	}

	if(listen(n,128)<0){
		perror("error ");
		close(n);
		exit(1);
	}
	return n;

}

void envoyer_liste(int fd, struct catalogue *c){

	for(int i = 0; i < c->nb;i++){

		dprintf(fd, "%6d : %s",i,c->tab[i]->titre);

	}
	dprintf(fd,"\n");
	
}

uint16_t recevoir_num_comptine(int fd)
{
	uint16_t n;
	size_t r=read(fd,&n,2);
	if(r<0){
		perror("read in envoyer_comptine");
		exit(1);
	}
	return ntohs(n);
}

void envoyer_comptine(int fd, const char *dirname, struct catalogue *c, uint16_t ic)
{
	//construire le bon chemin 
	char path[256];
	strcpy(path,(char*)dirname);
	strcat(path,"/");
	strcat(path,c->tab[ic]->nom_fichier);
	
	//ouvrir le bon fichier
	int fd_c;
	if((fd_c = open(path, O_RDONLY)) < 0) {
		perror("open in envoyer_comptine");
		exit(1);
	}

	char buf[256];
	size_t i;
	
	while((i = read(fd_c,buf,256)) > 0) {

		if(write(fd, buf, i)<0){
			perror("erreur serveur send comptine write\n");
			exit(2);
		}
	}

	dprintf(fd, "\n\n");
	close(fd_c);
}

void *multi(void *arg){
	if(arg==NULL){
		perror("argument thread NULL");
		exit(1);
	}
	
	struct argument *a = arg; // CAST

	struct catalogue *c;
	c = creer_catalogue(a->dir_name);
	if(c==NULL){
		perror("catalogue thread null");
		exit(1);
	}

	envoyer_liste(a->sock,c);
	int nc =recevoir_num_comptine(a->sock);
	
	int log =log_connect(a,nc);
	if(log<0){
		perror("erreur log\n");
	}
	
	envoyer_comptine(a->sock,a->dir_name,c,nc);

	close(a->sock);
	liberer_catalogue(c);
	free(arg);

	return NULL;
}

int log_connect(struct argument* a, int nc){

    char str[256];
	// Mettre l'adresse en string inverse de inet_pton
    if (inet_ntop(AF_INET, &(a->client.sin_addr), str, INET_ADDRSTRLEN) != NULL) {
	
	// Accès au fichiers de log;
	pthread_mutex_lock(&lock);

		dprintf(a->log,"Adresse du client(IPV4) = %s\n", str);  
		dprintf(a->log,"Comptine numéros: %d \n", nc); 
		char *time = ctime(&a->time);
		dprintf(a->log,"Connecté le : %s \n",time) ;

 	pthread_mutex_unlock(&lock);

    } else {
        perror("inet_ntop erreur log");  
        return -1;
    }
    return 0;
}

