
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "comptine_utils.h"

int read_until_nl(int fd, char *buf){
	
	int cpt = 0;

	while (read(fd, buf + cpt, 1) == 1) {
		
		if(buf[cpt] == '\n'){break;}	
		cpt++;

	}
	
	return cpt;
}


int est_nom_fichier_comptine(char *nom_fich){
	int n = strlen(nom_fich);
	
	if(!(strcmp(nom_fich + n - 4, ".cpt"))){return 1;} 
	return 0; 
	
}

struct comptine *init_cpt_depuis_fichier(const char *dir_name, const char *base_name){
	if(est_nom_fichier_comptine((char *)base_name) == 0){return NULL;}

    char buf[100];
 
    char path[500];
    strcpy(path, (char*)dir_name);
    strcat(path ,"/");
    strcat(path ,(char *)base_name);
	
    int fd = open(path,O_RDONLY);
    if(fd==-1){
        perror("open path init cpt"); 
		return NULL;
    }
	
    int n = read_until_nl(fd, buf);
    int taille = strlen(buf);

    struct comptine *cpt = malloc(sizeof(struct comptine));
    cpt->titre = malloc((n+2)* sizeof(char));
    cpt->nom_fichier = malloc((taille +1)*sizeof(char));

    buf[n]= '\n';
    buf[n+1]= '\0';
    strcpy(cpt->titre,buf);
    strcpy(cpt->nom_fichier,base_name);
    
    close(fd);
    return cpt;
}

void liberer_comptine(struct comptine *cpt)
{
	free(cpt->titre);
	free(cpt->nom_fichier);
	free(cpt);
}

struct catalogue *creer_catalogue(const char *dir_name){

	//on compte le nombre de comptines
	DIR *d = opendir(dir_name);
	if(d==NULL){perror("directory not open (count) ");return NULL;}
	 
	int cpt=0;
	struct dirent *ent;
	while((ent=readdir(d))!=NULL){

		if(est_nom_fichier_comptine(ent->d_name)){
			cpt++;
		}
	}

	// Rembobiner
	rewinddir(d);

	struct catalogue * ctl = malloc ( sizeof(struct catalogue));
	ctl->tab = malloc(cpt*sizeof(struct comptine));
	ctl->nb = cpt;
	int i=0;
	while((ent=readdir(d))!=NULL){
		// je ne met pas de verif pour ". "et ".." init_cpt_depuis_fichier le fait deja
		struct comptine *s =init_cpt_depuis_fichier(dir_name,ent->d_name);
		if(s!=NULL){
			ctl->tab[i++] = s;
		}
	
	}

	closedir(d);
	return ctl;
}

void liberer_catalogue(struct catalogue *c){
	if(c!=NULL){
		for(int i=0 ; i<c->nb ; i++){
			liberer_comptine(c->tab[i]);
		}
		free(c->tab);
		free(c);
	}
}