



/* fichiers de la bibliothèque standard */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h> 
/* bibliothèque standard unix */
#include <unistd.h> /* close, read, write */
#include <sys/types.h>
#include <sys/socket.h>
/* spécifique à internet */
#include <arpa/inet.h> /* inet_pton */
/* spécifique aux comptines */
#include "comptine_utils.h"


#define PORT_WCP 4321

void usage(char *nom_prog)
{
	fprintf(stderr, "Usage: %s addr_ipv4\n"
			"client pour WCP (Wikicomptine Protocol)\n"
			"Exemple: %s 208.97.177.124\n", nom_prog, nom_prog);
}

/** Retourne (en cas de succès) le descripteur de fichier d'une socket
 *  TCP/IPv4 connectée au processus écoutant sur port sur la machine d'adresse
 *  addr_ipv4 */
int creer_connecter_sock(char *addr_ipv4, uint16_t port);

/** Lit la liste numérotée des comptines dans le descripteur fd et les affiche
 *  sur le terminal.
 *  retourne : le nombre de comptines disponibles */
uint16_t recevoir_liste_comptines(int fd);

/** Demande à l'utilisateur un nombre entre 0 (compris) et nc (non compris)
 *  et retourne la valeur saisie. */
uint16_t saisir_num_comptine(uint16_t nb_comptines);

/** Écrit l'entier ic dans le fichier de descripteur fd en network byte order */
void envoyer_num_comptine(int fd, uint16_t nc);

/** Affiche la comptine arrivant dans fd sur le terminal */
void afficher_comptine(int fd);

int main(int argc, char *argv[])
{
	if (argc != 2) {
        usage(argv[0]);
        return 1;
    }
	
	//créer socket
	int sock = creer_connecter_sock(argv[1],PORT_WCP);

	//recevoir la liste comptine depuis socks
	int nb_cpt=recevoir_liste_comptines(sock);
	int nc = saisir_num_comptine(nb_cpt);
	envoyer_num_comptine(sock,nc);
	afficher_comptine(sock);
	
	close(sock);
	return 0;
}

int creer_connecter_sock(char *addr_ipv4, uint16_t port){

	int n = socket(AF_INET,SOCK_STREAM,0);
	if(n < 0){
		perror("error creation socket client\n");
		exit(1);
	}

	struct sockaddr_in sa ={
		.sin_family = AF_INET,
		.sin_port = htons(port)
	};

	if (inet_pton(AF_INET, addr_ipv4, &sa.sin_addr) != 1) {
		perror( "adresse ipv4 non valable\n");
		close(n);
		exit(1);
	}

	if(connect(n,(struct sockaddr * ) &sa, sizeof(sa))<0){
		perror("erreur connect socks\n");
		close(n);
		exit(1);
	} 

	return n;
}


uint16_t recevoir_liste_comptines(int fd)
{
	uint16_t i = 0;
	char buf[2048];
	size_t taille;

	printf("\n\n");	// Mise en page.
	printf("--------------- WIKI-COMPTINES ------------------\n\n");

	while ((taille = read_until_nl(fd, buf)) != 0) {
		buf[taille + 1] = '\0';
		printf("%s", buf);
		i++;
	}

	return i;
}

uint16_t saisir_num_comptine(uint16_t nb_comptines){

	uint16_t n ;
	do{

		printf("\n\n-------------Choisir sa comptine------------------\n\n");	//Mise en page
		printf("Entrez un numéro de comptine entre [0 et %d] =",nb_comptines-1);	//Mise en page

		scanf("%hu",&n);
	}while (n>nb_comptines-1);
	return n;
}

void envoyer_num_comptine(int fd, uint16_t nc)
{
	nc = htons(nc);
	if(write(fd, &nc, 2)<0){
		perror("erreur client send num_comptine write \n");
		exit(2);
	}
}

void afficher_comptine(int fd)
{
	char buf[1];
	size_t size;

	printf("\n-------------La comptine-----------------------\n\n");

	while((size =read(fd,buf,1))!=0){
		if(!(strcmp(buf-3,"\n\n\n"))){
			printf("%s",buf);
			return;
		}
		printf("%s",buf);
	}
	printf("-------------Finit au revoir -----------------------\n\n");
	
}
