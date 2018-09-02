# Descripción

El proyecto consiste en desarrollar un driver para dispositivos de almacenamiento conectados a través
de la interfaz ATA.

1. No es necesario interactuar con dispositivos ATAPI, excepto en la fase de identificación.

2. Las operaciones I/O serán en modo PIO o interrupt-driven, mas no DMA.

3. La interfaz a desarrollar está compuesta, básicamente, por tres funciones declaradas en
   src/kernel/include/ata.h, que deberán ser definidas en src/kernel/drivers/ata.c.
   Por supuesto, se pueden crear archivos extra de ser necesario, así como modificar el módulo
   ata.c para incluir las funcionalidades que se necesiten. Los detalles relativos a cada función
   se encuentran en sus respectivas definiciones.

## Sistema Operativo

Implementado en `Linux`.

## Colaboraciones

Cree un `issue` o envíe un `pull request`

## Autores

Iván Galbán Smith <ivan.galban.smith@gmail.com>

Raydel E. Alonso Baryolo <raydelalonsobaryolo@gmail.com>

3rd year of Computer Science

University of Havana, 2015
