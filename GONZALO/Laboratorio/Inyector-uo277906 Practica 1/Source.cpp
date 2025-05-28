#include <windows.h>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>

using namespace std;
	//DAR VALORES A ESTAS CONSTANTES
#define MAXPETICIONES 10
#define MAXUSUARIOS 50

/*
->INTRODUCIR VALORES DE FUNCIONAMIENTO
->BIEN AQUI O LEYENDOLOS COMO VALORES POR TECLADO
*/


int numUsuarios;
int numPeticiones;
float tReflex;
	
// Estructura de almacenamiento 

struct datos {
	int contPet;
	float reflex[MAXPETICIONES];
};

datos datoHilo[MAXUSUARIOS];


//------------------------------------------------------------------------
float NumeroAleatorio(float limiteInferior, float limiteSuperior) {
	float num = (float)rand();
	num = num * (limiteSuperior - limiteInferior) / RAND_MAX;
	num += limiteInferior;
	return num;
}

//------------------------------------------------------------------------
float DistribucionExponencial(float media) {
	float numAleatorio = NumeroAleatorio(0, 1);
	while (numAleatorio == 0 || numAleatorio == 1)
		numAleatorio = NumeroAleatorio(0, 1);
	return (-media) * logf(numAleatorio);
}

//------------------------------------------------------------------------
// Funci�n preparada para ser un thread

DWORD WINAPI Usuario(LPVOID parametro) {

	DWORD dwResult = 0;
	int numHilo = *((int*)parametro);
	int i;
	float tiempo;

	srand(101 + numHilo * 7);

	datoHilo[numHilo].contPet = 0;

	// ... Resto de cosas comunes para cada usuario

	for (i = 0; i < numPeticiones; i++) {
		// PRINTF solo para depuraci�n NUNCA en medici�n
		printf("Peticion %d para el usuario %d\n", i, numHilo);
		// Hacer petici�n cuando se implementen los sockets
		// ----
		// Calcular el tiempo de reflexi�n antes de la siguiente petici�n
		tiempo = DistribucionExponencial((float)tReflex);

		// Guarda los valores de la petici�n
		datoHilo[numHilo].reflex[i] = tiempo;
		datoHilo[numHilo].contPet++;

		// Espera los milisegundos calculados previamente
				Sleep(tiempo*1000);
	}
	return dwResult;
}


int main(int argc, char* argv[])
{
	int i, j;
	HANDLE handleThread[MAXUSUARIOS];
	int parametro[MAXUSUARIOS];

	// Leer por teclado los valores para realizar la prueba o asignarlos. En este caso, los obtiene de los argumentos.
	numUsuarios = atoi(argv[1]);
	numPeticiones = atoi(argv[2]);
	tReflex = atof(argv[3]);

	// Lanza los hilos
	for (i = 0; i < numUsuarios; i++) {
		parametro[i] = i;
		handleThread[i] = CreateThread(NULL, 0, Usuario, &parametro[i], 0, NULL);
		if (handleThread[i] == NULL) {
			cerr << "Error al lanzar el hilo" << endl;
			exit(EXIT_FAILURE);
		}
	}

	// Hacer que el Thread principal espere por sus hijos
	for (i = 0; i < numUsuarios; i++)
		WaitForSingleObject(handleThread[i], INFINITE);


	// Recopilar resultados y mostrarlos a pantalla o 
	// guardarlos en disco
	for (int i = 0; i < numUsuarios; i++) {
		//Obtener el número de peticiones por hilo
		int cont = datoHilo[i].contPet;
		printf("Contpet del hilo %d es %d\n", i, cont);
	}


	for (int i = 0; i < numUsuarios; i++) {
		float tiempo_total = 0;
		for (int j = 0; j < datoHilo[i].contPet; j++) {
			float reflex = datoHilo[i].reflex[i];
			tiempo_total += reflex;
		}
		float average = tiempo_total / datoHilo[i].contPet;
		printf("El hilo %d tiene un tiempo medio de reflexion de %d\n",i,average );
	}


	/*
	->	POR HACER
	*/

		return 0;
}

