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
#define DIRSIZE 128

#define NSECTORSFAT 2 * FATSIZE / SECTORSIZE
#define NSECTORSDIR CLUSTERSIZE / SECTORSIZE
#define NCLUSTERSFAT 2 * FATSIZE / CLUSTERSIZE
#define NCLUSTERSDIR 1

#define NFORMATADO "Disco não formatado!\n"

int formatado;

unsigned short fat[FATSIZE];

typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

typedef struct {
	char mode; 
	unsigned short current_block;
	unsigned short offset;
} file;

dir_entry dir[DIRSIZE];

file fildes[DIRSIZE];

void fs_update() {
  int i;

  /*  Escrita da FAT */
  for (i = 0; i < NSECTORSFAT; bl_write(i, (char *) fat + i*SECTORSIZE), i++); 
  
  /*  Escrita do diretório */
  for (i = 0; i < NSECTORSDIR; bl_write(i + NSECTORSFAT, (char *) dir + i*SECTORSIZE), i++); 
}

int fs_init() {
  int i;

  /*  Leitura da FAT */
  for (i = 0; i < NSECTORSFAT; bl_read(i, (char *) fat + i*SECTORSIZE), i++);
  
  /*  Leitura do diretório */
  for (i = 0; i < NSECTORSDIR; bl_read(i + NSECTORSFAT, (char *) dir + i*SECTORSIZE), i++);

  /*  Inicialização da tabela de FDs */
  for (i = 0; i < DIRSIZE; fildes[i].current_block = 0, i++);

  /*  Verificação de formatação */
  for (i = 0; i < NCLUSTERSFAT && (fat[i] == 3); i++); 

  if (i != NCLUSTERSFAT || fat[NCLUSTERSFAT] != 4) {
	printf("Disco não formatado!\n");
	formatado = 0;
  }
  else
  	formatado = 1;

  return 1;
}

int fs_format() {
  int i; 
  /*  Criação da FAT */
  for (i = 0; i < NCLUSTERSFAT; fat[i++] = 3);
  fat[NCLUSTERSFAT] = 4;
  for (i++; i < FATSIZE; fat[i++] = 1);

  /*  Criação do Diretório */
  for (i = 0; i < 128; dir[i++].used = 0);

  fs_update();

  formatado = 1;

  return 1;
}

int fs_free() {
  int i, fsize;

  for (i = 0, fsize = 0; i < 128; i++) {
	if (dir[i].used) {
		fsize = fsize + dir[i].size;
	}
  }

  return (bl_size() - NSECTORSFAT - NSECTORSDIR) * SECTORSIZE - fsize;
}

int fs_list(char *buffer, int size) {
  int i, psize;
  char *p = buffer;
  
  if (!formatado && printf(NFORMATADO)) return 0;
  
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

  if (!formatado && printf(NFORMATADO)) return 0;
  
  for (i = 0; i < 128 && dir[i].used; i++) {
	if (dir[i].used && !strcmp(dir[i].name, file_name)) {
	  printf("Arquivo já existente.\n");
	  return -1;
	}
  }

  if (i == 128) {
	printf("Não há espaço no diretório.\n");
	return 0;
  }

  if (strlen(file_name) > 25) {
	printf("O nome excede o máximo.\n");
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

  fs_update();

  return i;
}

int fs_remove(char *file_name) {
  int i,j,rem = -1;

  if (!formatado && printf(NFORMATADO)) return 0;
  
  for (i = 0; i < 128 && rem == -1; i++) {
	if (dir[i].used && !strcmp(dir[i].name, file_name))
	  rem = i;
  }

  if (i == 128) {
	printf("Arquivo não existe.\n");
	return -1;
  }

  dir[rem].used = 0;
  j = dir[rem].first_block;

  while(fat[j] != 2) {
	rem = j;
	j = fat[j];
	fat[rem] = 1;
  }
  fat[j] = 1;


  fs_update();

  return i;
}

int fs_open(char *file_name, int mode) {
  int i, rem, fb, entry;

  for (i = 0; i < 128; i++) /*  Busca pelo arquivo */
	if (dir[i].used && !strcmp(dir[i].name, file_name)) 
		break;
 
  if (fildes[i].current_block) {
	printf("Arquivo já aberto.\n");
	return -1;
  } 
  
  if (mode == FS_R) { /*  Modo de leitura */
	if (i == 128) {
      printf("Arquivo não existe.\n");
      return -1;
	}
	entry = i;
  }
  else if (mode == FS_W) { /*  Modo de escrita */
	if (i == 128) { /*  Arquivo não existe */
		entry = fs_create(file_name);
	}
	else { /*  Arquivo existe */
	  entry = i;
	  dir[entry].size = 0;
	  fb = dir[i].first_block;
	  i = fb;
  	  while(fat[i] != 2) {
	    rem = i;
	    i = fat[i];
	    fat[rem] = 1;
      }
      fat[i] = 1;
	  fat[fb] = 2;
	  i = fb;

	  fs_update();
	}
  }
  else {
	printf("Erro ao abrir arquivo, modo não reconhecido.\n");
	return -1;
  }

  fildes[entry].offset = 0;
  fildes[entry].current_block = dir[entry].first_block;
  fildes[entry].mode = mode;
  printf("Primeiro bloco: %d\n", fildes[entry].current_block);
  return entry;
}

int fs_close(int file)  {
 
  if (!fildes[file].current_block) {
	printf("Arquivo não aberto.\n");
	return -1;
  }

  fildes[file].current_block = 0;
  return file; 
}

int fs_write(char *buffer, int size, int file) {
  char sector[SECTORSIZE];
  unsigned long write_count, write_offset;
  unsigned short i, cb, loops;

  if (!fildes[file].current_block) {
    printf("Arquivo não aberto.\n");
	return 0;
  }
  if (fs_free() < size) {
	printf("Não há espaço suficiente no disco.\n");
	return 0;
  }

  write_count = 0;
  write_offset = 0;
  loops = fildes[file].offset / SECTORSIZE;
  cb = fildes[file].current_block;
  sector[0] = '\0'; 

  bl_read(cb * CLUSTERSIZE / SECTORSIZE + loops, sector); /*  Leitura inicial */

  while ((fildes[file].offset % SECTORSIZE) + size > SECTORSIZE) { /*  A escrita começa e termina em blocos diferentes */
	if (fildes[file].offset < CLUSTERSIZE) {
		write_count += SECTORSIZE - (fildes[file].offset % SECTORSIZE);
		size -= write_count;
    	strncpy(sector + (fildes[file].offset % SECTORSIZE), buffer + write_offset, write_count);
		write_offset += write_count;
		}
	
	bl_write(cb * CLUSTERSIZE / SECTORSIZE + loops, sector);
	
	loops = (loops + 1);

	if (!(loops % (CLUSTERSIZE / SECTORSIZE))) { /*  Todos os setores do agrupamento estão usados */
		for (i = NCLUSTERSFAT; fat[i] != 1; i++);

		fat[cb] = i;
		fat[i] = 2;
		cb = i;
		loops = 0;
		fildes[file].current_block = cb;
	}
	printf("Inside while\n");	
    printf("Texto: %s\nSetor: %d\n", sector, cb * CLUSTERSIZE / SECTORSIZE + loops);
	
    fildes[file].offset = (fildes[file].offset + write_count) % CLUSTERSIZE;
   
    sector[0] = '\0';	
	bl_read(cb * CLUSTERSIZE / SECTORSIZE + loops, sector);
  }

  /*  Escrita no setor corrente */
  write_count += size;
  strncpy(sector + (fildes[file].offset % SECTORSIZE), buffer + write_offset, size);
  bl_write(cb * CLUSTERSIZE / SECTORSIZE + loops, sector);
  
  printf("Texto: %s\nSetor: %d\n", sector, cb * CLUSTERSIZE / SECTORSIZE + loops);
 
  /*  Atualização do arquivo */ 
  fildes[file].offset = (fildes[file].offset + size) % CLUSTERSIZE;
  dir[file].size += write_count;
  
  printf("Tamanho: %d\n", dir[file].size);

  fs_update();

  return write_count;
}

int fs_read(char *buffer, int size, int file) {
  char sector[SECTORSIZE];
  unsigned long read_count, read_offset;
  unsigned short cb, loops;

  if (!fildes[file].current_block) {
    printf("Arquivo não aberto.\n");
	return 0;
  }

  read_count = 0;
  read_offset = 0;
  loops = fildes[file].offset / SECTORSIZE;
  cb = fildes[file].current_block; 

  bl_read(cb * CLUSTERSIZE / SECTORSIZE + loops, sector);

  while ((fildes[file].offset % SECTORSIZE) + size > SECTORSIZE) {
	if (fildes[file].offset < CLUSTERSIZE) {
		read_count += fat[cb] == 2 ? 
					  dir[file].size % CLUSTERSIZE :
					  SECTORSIZE - (fildes[file].offset % SECTORSIZE);
		size -= read_count;
    	strncpy(buffer + read_offset, sector + (fildes[file].offset % SECTORSIZE), read_count);
		read_offset += read_count;
	}
	
	loops = (loops + 1);
	
	if (!(loops % (CLUSTERSIZE / SECTORSIZE))) {
		cb = fat[cb];
		loops = 0;
	}

	printf("Inside while\n");
    printf("Texto: %s\nSetor: %d\nOffset: %d\n ", buffer, cb * CLUSTERSIZE / SECTORSIZE + loops, fildes[file].offset);

	fildes[file].offset = (fildes[file].offset + read_count) % CLUSTERSIZE;
    
	bl_read(cb * CLUSTERSIZE / SECTORSIZE + loops, sector);
  }

    if (fat[cb] == 2) {
		if (fildes[file].offset == dir[file].size % CLUSTERSIZE)
			return 0;
		read_count += dir[file].size % CLUSTERSIZE > size ?
					  size :
					  dir[file].size % CLUSTERSIZE;
  		strncpy(buffer + read_offset, sector + (fildes[file].offset % SECTORSIZE), read_count - read_offset);
  		fildes[file].offset = (fildes[file].offset + read_count - read_offset) % CLUSTERSIZE;
	}
	else {
		read_count += size;
  		strncpy(buffer + read_offset, sector + (fildes[file].offset % SECTORSIZE), read_count);
  	fildes[file].offset = (fildes[file].offset + size) % CLUSTERSIZE;
	}

    printf("Texto: %s\nSetor: %d\nOffset: %d\n ", buffer, cb * CLUSTERSIZE / SECTORSIZE + loops, fildes[file].offset);
  
	return read_count;
}

