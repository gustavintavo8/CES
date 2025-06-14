﻿#include <windows.h>
#include <iostream>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>

using namespace std;

#define MAXPETICIONES 10000
#define MAXUSUARIOS 500
#define PUERTO 57000
#define TAM_PET 1250
#define TAM_RES 1250


// Inicializar estas variables en main antes de lanzar las pruebas
float tReflex;  // en segundos
char* ipServidor;
int numUsuarios;
int numPeticiones;

// Esta variable es global para mayor eficiencia. Se le asigna valor antes
// de lanzar varios hilos y luego la leen varios hilos en paralelo, pero como
// no le vuelven a asignar valor, no es necesario protegerla con un mutex

float ticksPorMilisegundo;

LARGE_INTEGER tickBase; // Todos los ticks seran relativos a este tiempo
LARGE_INTEGER tickInicio; // Instante en el que se empiezan a tomar tiempos: tickBase + ticksCalentamiento
LARGE_INTEGER tickFin;  // Instante en el que se acaba la prueba: tickInicio + ticksDuracion


struct datos {
	int contPet;
	float reflex[MAXPETICIONES];
	float tres[MAXPETICIONES];
	unsigned long ciclosIniPeticion[MAXPETICIONES];
	unsigned long ciclosFinPeticion[MAXPETICIONES];
	// Si se eligiera la opcion de trabajar con tiempos unicamente
	// float tinicio[MAXPETICIONES];
	// float tfinal[MAXPETICIONES];
};

datos datoHilo[MAXUSUARIOS];

// --------------------------------------
double MilisegundosTranscurridos(LARGE_INTEGER inicio, LARGE_INTEGER final) {
	LARGE_INTEGER diferencia;
	float milisegundos;

	diferencia.QuadPart = final.QuadPart - inicio.QuadPart;
	milisegundos = diferencia.LowPart / ticksPorMilisegundo;
	if (diferencia.HighPart != 0)
		milisegundos += (float)ldexp((double)diferencia.HighPart, 32) / ticksPorMilisegundo;

	return milisegundos;
}
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
// Funcion preparada para ser un thread. Simula un usuario
DWORD WINAPI Usuario(LPVOID parametro) {
	DWORD dwResult = 0;
	int numHilo = *((int*)parametro);

	SOCKET elSocket;
	sockaddr_in dirServidor;
	char peticion[TAM_PET];
	char respuesta[TAM_RES];
	int valorRetorno;
	LARGE_INTEGER tIni, tFin;
	float tmpReflex;


	datoHilo[numHilo].contPet = 0;

	srand(127 + numHilo * 5);

	do {

		//
		// Creacion del socket
		//
		elSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (elSocket == INVALID_SOCKET) {
			cerr << "No se pudo crear el socket" << endl;
			WSACleanup();
			exit(EXIT_FAILURE);
		}

		//
		// Toma de tiempo inicial
		//
		QueryPerformanceCounter(&tIni);
		//
		// Conexion con el servidor
		//

		dirServidor.sin_family = AF_INET;
		dirServidor.sin_addr.s_addr = inet_addr(ipServidor);
		dirServidor.sin_port = htons(PUERTO + numHilo);
		valorRetorno = connect(elSocket, (struct sockaddr*) & dirServidor, sizeof(dirServidor));
		if (valorRetorno == SOCKET_ERROR) {
			cerr << "Error en el connect: " << WSAGetLastError() << endl;
			closesocket(elSocket);
			WSACleanup();
			exit(EXIT_FAILURE);
		}

		//
		// Enviar una cadena
		//
		valorRetorno = send(elSocket, peticion, sizeof(peticion), 0);
		if (valorRetorno == SOCKET_ERROR) {
			cerr << "Error en el send: " << WSAGetLastError() << endl;
			closesocket(elSocket);
			WSACleanup();
			exit(EXIT_FAILURE);
		}

		//
		// Recibir la respuesta
		//
		valorRetorno = recv(elSocket, respuesta, sizeof(respuesta), 0);
		if (valorRetorno != TAM_RES) {
			cerr << "Error en el recv: " << WSAGetLastError() << endl;
			closesocket(elSocket);
			WSACleanup();
			exit(EXIT_FAILURE);
		}


		//
		// Cerrar la conexion
		//

		closesocket(elSocket);

		//
		// Medicion final
		//

		QueryPerformanceCounter(&tFin);

		//
		// Comprobar si peticion valida
		//

		tmpReflex = DistribucionExponencial((float)tReflex);

		if (tIni.QuadPart > tickInicio.QuadPart&& tFin.QuadPart < tickFin.QuadPart) {

			// La implementacion de esta parte puede variar
			// dependiendo de la opcion elegida para almacenar 
			// valores. Tiempos de inicio y fin, o ciclos de inicio y fin

			datoHilo[numHilo].tres[datoHilo[numHilo].contPet] = (float)MilisegundosTranscurridos(tIni, tFin);
			datoHilo[numHilo].reflex[datoHilo[numHilo].contPet] = tmpReflex;
			datoHilo[numHilo].ciclosIniPeticion[datoHilo[numHilo].contPet] = (unsigned)(tIni.QuadPart - tickBase.QuadPart);
			datoHilo[numHilo].ciclosFinPeticion[datoHilo[numHilo].contPet] = (unsigned)(tFin.QuadPart - tickBase.QuadPart);
			datoHilo[numHilo].contPet++;
			if (datoHilo[numHilo].contPet > MAXPETICIONES) {
				printf("Superado el limite de peticiones para un hilo \n");
				exit(0);
			}
		}

		Sleep((int)(tmpReflex * 1000));

	} while (tFin.QuadPart < tickFin.QuadPart);


	return dwResult;
}

//------------------------------------------------------------------------
// Funci�n para cargar la libreria de sockets
int Ini_sockets(void) {
	WORD wVersionDeseada;
	WSAData wsaData;

	int error;

	wVersionDeseada = MAKEWORD(2, 0);
	if (error = WSAStartup(wVersionDeseada, &wsaData) != 0) {
		return error;
	}

	// Comprobar si la DLL soporta la versi�n 2.0

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 0) {
		error = 27;
		cerr << "La libreria no soporta la version 2.0" << endl;
		WSACleanup();
	}
	return error;
}
//------------------------------------------------------------------------
// Funci�n para descargar la librer�a de sockets
void Fin_sockets(void) {
	WSACleanup();
}
//------------------------------------------------------------------------

int main(int argc, char* argv[])
{
	int i, j;
	HANDLE handleThread[MAXUSUARIOS];
	int parametro[MAXUSUARIOS];
	int segCal; // Segundos de calentamiento
	int segMed; // Segundos de medicion
	FILE* info;

	// Variables para calculos de tiempo de respuesta y productividad
	float sumaTiempos;
	float sumaTiempos2;
	float taux1, taux2; // variables auxiliares para calcular tiempo de respuesta 2
	int sumaPet;


	time_t hora_ini_exp; //Marca la hora de inicio del experimento
	time_t hora_inicio_medicion; //Hora inicio de la medicion
	time_t hora_fin_medicion; //Hora fin de la medicion


	if (argc != 6) {
		printf("Numero de parametros no valido: inyector usuarios TReflex(seg) IPservidor Calentamiento(seg) Medidicon(seg) \n");
		exit(0);
	}
	numUsuarios = atoi(argv[1]);
	if (numUsuarios <= 0 || numUsuarios > MAXUSUARIOS) {
		printf("El numero de usuarios debe estar comprendido entre 1 y %d\n", MAXUSUARIOS);
		exit(0);
	}

	tReflex = (float)atof(argv[2]);
	if (tReflex <= 0) {
		printf("El tiempo de reflexion debe ser mayor que 0\n");
		exit(0);
	}

	ipServidor = argv[3];


	segCal = atoi(argv[4]);
	if (segCal < 0) {
		printf("El tiempo de transitorio debe ser mayor o igual que 0\n");
		exit(0);
	}

	segMed = atoi(argv[5]);
	if (segMed <= 0) {
		printf("El tiempo de medicion debe ser mayor que 0\n");
		exit(0);
	}

	Ini_sockets();

	// Calcular hora de inicio de la medicion y hora final de la medicion
	time(&hora_ini_exp);
	hora_inicio_medicion = hora_ini_exp + segCal;
	hora_fin_medicion = hora_ini_exp + segCal + segMed;


	// Preparar la medicion de tiempos
	LARGE_INTEGER ticksPorSeg;
	if (!QueryPerformanceFrequency(&ticksPorSeg)) {
		cout << "No esta disponible el contador de alto rendimiento" << endl;
		exit(-1);
	}
	ticksPorMilisegundo = (float)(ticksPorSeg.LowPart / 1E3);

	QueryPerformanceCounter(&tickBase);
	tickInicio.QuadPart = tickBase.QuadPart + (LONGLONG)(segCal * 1000 * ticksPorMilisegundo);
	tickFin.QuadPart = tickInicio.QuadPart + (LONGLONG)(segMed * 1000 * ticksPorMilisegundo);

	// Lanzar los hilos
	//!!!!!!!!!!!!!!!!!!!!!!!! COMPLETAR CON LA PRACTICA 1 !!!!!!!!!!!!!!!!!!!!!!!!!!!!
		for (i = 0; i < numUsuarios; i++) {
			parametro[i] = i;
			handleThread[i] = CreateThread(NULL, 0, Usuario, &parametro[i], 0, NULL);
			if (handleThread[i] == NULL) {
				cerr << "Error al lanzar el hilo" << endl;
				exit(EXIT_FAILURE);
			}
		}

		// Hacer que el Thread principal espere por sus hijo	
		//!!!!!!!!!!!!!!!!!!!!!!!! COMPLETAR CON LA PRACTICA 1 //!!!!!!!!!!!!!!!!!!!!!!!!
		for (i = 0; i < numUsuarios; i++)
			WaitForSingleObject(handleThread[i], INFINITE);
		Fin_sockets();



	// Escribir los resultados a disco

	fopen_s(&info, "info.txt", "w");

	//TIEMPOS

	char cadena[26];

	ctime_s(cadena, sizeof(cadena), &hora_inicio_medicion);

	//Almacenar en el fichero la hora de inicio de la medicion y la hora de fin de la medicion.
	fprintf(info, "\nHORA INICIO DE MEDICION:  %s\n", cadena);

	ctime_s(cadena, sizeof(cadena), &hora_fin_medicion);

	fprintf(info, "HORA FIN DE MEDICION: %s \n", cadena);


	// Parametros de la prueba

	fprintf(info, "\nParametros del experimento: \n");
	fprintf(info, "Nº Usuarios: %d; Tpo. Reflex (seg): %f; IP servidor: %s; Transitorio (seg): %d; Medicion (seg): %d \n", numUsuarios, tReflex, ipServidor, segCal, segMed);

	printf("\nParametros del experimento: \n");
	printf("Usuarios: %d; Tpo. Reflex (seg); %f; IP servidor: %s; Transitorio (seg): %d; Medicion (seg): %d \n", numUsuarios, tReflex, ipServidor, segCal, segMed);

	fprintf(info, "N. usu.; N. pet.; Tpo Reflex(Seg); Tpo. Ini.(mseg); Tpo. Fin(mseg)\n");

	sumaTiempos = 0;
	sumaTiempos2 = 0;
	sumaPet = 0;

	for (i = 0; i < numUsuarios; i++) {
		sumaPet = sumaPet + datoHilo[i].contPet;
		for (j = 0; j < datoHilo[i].contPet; j++) {
			sumaTiempos = sumaTiempos + datoHilo[i].tres[j];
			taux1 = datoHilo[i].ciclosIniPeticion[j] / ticksPorMilisegundo;
			taux2 = datoHilo[i].ciclosFinPeticion[j] / ticksPorMilisegundo;
			sumaTiempos2 = sumaTiempos2 + (taux2 - taux1);
			fprintf(info, "%d;%d;%f;%f;%f\n", i, j, datoHilo[i].reflex[j], taux1, taux2);
		}
	}
	fprintf(info, "\n\n");

	printf("Resultados:\n");
	printf("N. Pet: %d \n", sumaPet);
	printf("Seg.Med: %d \n", segMed);
	printf("Tpo.Res1(mseg) : %f\n", (float)sumaTiempos / sumaPet);
	printf("Tpo.Res2(mseg) : %f\n", (float)sumaTiempos2 / sumaPet);
	printf("Product. (pet/seg): %f\n", (float)sumaPet / segMed);

	fclose(info);

	return 0;
}




