/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096
#define FATSIZE 65536

#define NSECTORSFAT 2 * FATSIZE / SECTORSIZE
#define NSECTORSDIR CLUSTERSIZE / SECTORSIZE
#define NCLUSTERSFAT 2 * FATSIZE / CLUSTERSIZE
#define NCLUSTERSDIR 1

#define FSTEP SECTORSIZE / sizeof(unsigned short)
#define DSTEP SECTORSIZE / sizeof(dir_entry)

unsigned short fat[65536];

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

dir_entry dir[128];


int fs_init() {
  int i;

  /*  Leitura da FAT */
  for (i = 0; i < NSECTORSFAT; i++)
	bl_read(i, (char *) fat + i*FSTEP);
  
  /*  Leitura do diretório */
  for (i = 0; i < NSECTORSDIR; i++)
	bl_read(i + NSECTORSFAT, (char *) dir + i*DSTEP);

  /*  Verificação de formatação */
  for (i = 0; i < NCLUSTERSFAT && (fat[i] == 3); i++); 

  if (i != NCLUSTERSFAT || fat[NCLUSTERSFAT] != 4)
	printf("Disco não formatado!\n");

  return 1;
}

int fs_format() {
  int i; 
  /*  Criação da FAT */
  for (i = 0; i < NCLUSTERSFAT; fat[i++] = 3);
  fat[NCLUSTERSFAT] = 4;
  for (i++; i < FATSIZE; fat[i++] = 1);
  for (i = 0; i < 128; dir[i++].used = 0);

  /*  Escrita da FAT */
  for (i = 0; i < NSECTORSFAT; i++){
	bl_write(i, (char *) (fat + i*FSTEP)); 
  }
  
  /*  Escrita do diretório */
  for (i = 0; i < NSECTORSDIR; i++)
    bl_write(i + NSECTORSFAT, (char *) (dir + i*DSTEP)); 


  return 0;
}

int fs_free() {
  int i, fsize;
  for (i = 0, fsize = 0; i < 128; i++) {
	if (dir[i].used) {
		fsize = fsize + dir[i].size;
	}
  }

  return (bl_size() - fsize - NSECTORSFAT - NSECTORSDIR) * SECTORSIZE;
}

int fs_list(char *buffer, int size) {
  int i, psize;
  char *p = buffer;
  for (i = 0, psize = 0; i < 128 && p != buffer + size; i++) {
	if (dir[i].used) {
	  psize = sprintf(p, "%s\t\t%d\n", dir[i].name, dir[i].size);
	  p = p + psize;
	}
  }
  if (psize == 0)
	buffer[0] = '\0';
  
  return 1;
}

int fs_create(char* file_name) {
  int i, j;

  for (i = 0; i < 128 && dir[i].used; i++) {
	if (dir[i].used && !strcmp(dir[i].name, file_name)) {
	  perror("Arquivo já existente.\n");
	  return 0;
	}
  }

  if (i == 128) {
	perror("Não é possível criar o arquivo.\n");
	return 0;
  }

  dir[i].used = 1;
  strcpy(dir[i].name, file_name); 
  dir[i].size = 0;
  dir[i].first_block = 0;
  
  for (j = NCLUSTERSFAT; j < FATSIZE && !dir[i].first_block; j++) {
	if (fat[j] == 1) {
		dir[i].first_block = j;
		fat[j] = 2;
	}
  }

  /*  Escrita da FAT */
  for (i = 0; i < NSECTORSFAT; i++)
	bl_write(i, (char *) (fat + i*FSTEP)); 
  
  /*  Escrita do diretório */
  for (i = 0; i < NSECTORSDIR; i++)
    bl_write(i + NSECTORSFAT, (char *) (dir + i*DSTEP));

  return 1;
}

int fs_remove(char *file_name) {
  int i,j,rem = -1;

  for (i = 0; i < 128 && rem == -1; i++) {
	if (dir[i].used && !strcmp(dir[i].name, file_name))
	  rem = i;
  }

  if (i == 128) {
	perror("Arquivo não existe.");
	return 1;
  }

  dir[rem].used = 0;
  j = dir[rem].first_block;

  while(fat[j] != 2) {
	rem = j;
	j = fat[j];
	fat[rem] = 1;
  }
  fat[rem] = 1;

  /*  Escrita da FAT */
  for (i = 0; i < NSECTORSFAT; i++){
	bl_write(i, (char *) (fat + i*FSTEP)); 
  }
  
  /*  Escrita do diretório */
  for (i = 0; i < NSECTORSDIR; i++)
    bl_write(i + NSECTORSFAT, (char *) (dir + i*DSTEP));

  return 0;
}

int fs_open(char *file_name, int mode) {
  printf("Função não implementada: fs_open\n");
  return -1;
}

int fs_close(int file)  {
  printf("Função não implementada: fs_close\n");
  return 0;
}

int fs_write(char *buffer, int size, int file) {
  printf("Função não implementada: fs_write\n");
  return -1;
}

int fs_read(char *buffer, int size, int file) {
  printf("Função não implementada: fs_read\n");
  return -1;
}

