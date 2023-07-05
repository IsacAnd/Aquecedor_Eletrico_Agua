// Defini��o de Bibliotecas
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "socket.h"
#include "sensores.h"
#include "tela.h"
#include "bufduplo.h" // buffer do tempo de resposta da temperatura
#include "bufduplo2.h" // buffer do tempo de resposta do nivel de agua
#include "bufduplo3.h" // buffer que guarda a temperatura
#include "bufduplo4.h"
#include "referenciaT.h" 
#include "referenciaH.h"

#define NSEC_PER_SEC (1000000000) // Numero de nanosegundos em um segundo

#define N_AMOSTRAS 1000

void thread_mostra_status(void)
{
	double t, h;
	while (1)
	{
		t = sensor_get("t");
		h = sensor_get("h");
		aloca_tela();
		system("tput reset");
		printf("---------------------------------------\n");
		printf("Temperatura (T)--> %.2lf\n", t);
		printf("Nivel       (H)--> %.2lf\n", h);
		printf("---------------------------------------\n");
		libera_tela();
		sleep(1);
		//
	}
}

void thread_le_sensor(void)
{ // Le Sensores periodicamente a cada 10ms
	struct timespec t;
	long periodo = 10e6; // 10e6ns ou 10ms

	// Le a hora atual, coloca em t
	clock_gettime(CLOCK_MONOTONIC, &t);
	while (1)
	{
		// Espera ateh inicio do proximo periodo
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		// Envia mensagem via canal de comunica��o para receber valores de sensores
		sensor_put(msg_socket("st-0"), msg_socket("sh-0"));

		// Calcula inicio do proximo periodo
		t.tv_nsec += periodo;
		while (t.tv_nsec >= NSEC_PER_SEC)
		{
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_sec++;
		}
	}
}

void thread_alarme(void)
{
	while (1)
	{
		sensor_alarmeT(29);
		aloca_tela();
		printf("ALARME\n");
		libera_tela();
		sleep(1);
	}
}

void thread_controle_temperatura(void)
{
	char msg_enviada[1000];
	long atraso_fim;
	struct timespec t, t_fim;
	long periodo = 50e6; // 50ms
	double temp, ref_temp;

	// Le a hora atual, coloca em t
	clock_gettime(CLOCK_MONOTONIC, &t);
	t.tv_sec++;

	while (1)
	{
		// Espera ateh inicio do proximo periodo
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		temp = sensor_get("t");
		ref_temp = ref_getT();
		double na;

		if (temp > ref_temp)
		{ // diminui temperatura

			sprintf(msg_enviada, "ani%lf", 100.0);
			msg_socket(msg_enviada);

			sprintf(msg_enviada, "anf%lf", 100.0);
			msg_socket(msg_enviada);

			sprintf(msg_enviada, "ana%lf", 0.0);
			msg_socket(msg_enviada);
		}

		if (temp < ref_temp)
		{ // aumenta temperatura

			if ((ref_temp - temp) * 20 > 10.0)
				na = 10.0;
			else
				na = (ref_temp - temp) * 20;

			sprintf(msg_enviada, "ani%lf", 0.0);
			msg_socket(msg_enviada);

			sprintf(msg_enviada, "anf%lf", 0.0);
			msg_socket(msg_enviada);

			sprintf(msg_enviada, "ana%lf", na);
			msg_socket(msg_enviada);
		}

		// Le a hora atual, coloca em t_fim
		clock_gettime(CLOCK_MONOTONIC, &t_fim);

		// Calcula o tempo de resposta observado em microsegundos
		atraso_fim = 1000000 * (t_fim.tv_sec - t.tv_sec) + (t_fim.tv_nsec - t.tv_nsec) / 1000;
		bufduplo_insereLeitura(atraso_fim);
		bufduplo3_insereLeitura(temp);

		// Calcula inicio do proximo periodo
		t.tv_nsec += periodo;
		while (t.tv_nsec >= NSEC_PER_SEC)
		{
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_sec++;
		}
	}
}

void thread_grava_temp_resp(void)
{
	FILE *dados_f;
	dados_f = fopen("dados.txt", "w");
	if (dados_f == NULL)
	{
		printf("Erro, nao foi possivel abrir o arquivo\n");
		exit(1);
	}
	int amostras = 1;
	while (amostras++ <= N_AMOSTRAS / 200)
	{
		long *buf = bufduplo_esperaBufferCheio();
		int n2 = tamBuf();
		int tam = 0;
		while (tam < n2)
			fprintf(dados_f, "%4ld\n", buf[tam++]);
		fflush(dados_f);
		aloca_tela();
		printf("Gravando no arquivo...\n");

		libera_tela();
	}

	fclose(dados_f);
}

void thread_controle_nivel(void)
{
	char msg_enviada[1000];
	long atraso_fim;
	long periodo = 50e6; // 50ms
	struct timespec t, t_fim;
	double nivel;
	double ref_nivel;
	double n;

	clock_gettime(CLOCK_MONOTONIC, &t);
	t.tv_sec++;

	while (1)
	{
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

		double nivel = sensor_get("h");
		double ref_nivel = ref_getH();
		double n;

		if (ref_nivel < nivel)
		{
			sprintf(msg_enviada, "ani%lf", 0.0);
			msg_socket(msg_enviada);

			sprintf(msg_enviada, "anf%lf", 100.0);
			msg_socket(msg_enviada);

			sprintf(msg_enviada, "ana%lf", 0.0);
			msg_socket(msg_enviada);
		}

		if (ref_nivel > nivel)
		{
			sprintf(msg_enviada, "ani%lf", 20.0);
			msg_socket(msg_enviada);

			sprintf(msg_enviada, "anf%lf", 0.0);
			msg_socket(msg_enviada);

			sprintf(msg_enviada, "ana%lf", 10.0);
			msg_socket(msg_enviada);
		}

		// Le a hora atual, coloca em t_fim
		clock_gettime(CLOCK_MONOTONIC, &t_fim);

		// Calcula o tempo de resposta observado em microsegundos
		atraso_fim = 1000000 * (t_fim.tv_sec - t.tv_sec) + (t_fim.tv_nsec - t.tv_nsec) / 1000;
		// teste = atraso_fim;
		bufduplo2_insereLeitura(atraso_fim);
		bufduplo4_insereLeitura(nivel);

		// Calcula inicio do proximo periodo
		t.tv_nsec += periodo;
		while (t.tv_nsec >= NSEC_PER_SEC)
		{
			t.tv_nsec -= NSEC_PER_SEC;
			t.tv_sec++;
		}
	}
}

void thread_grava_nivel_resp(void)
{
	FILE *dados2_f;
	dados2_f = fopen("dados2.txt", "w");
	if (dados2_f == NULL)
	{
		printf("Erro, nao foi possivel abrir o arquivo\n");
		exit(1);
	}
	int amostras = 1;
	while (amostras++ <= N_AMOSTRAS / 200)
	{
		long *buf = bufduplo2_esperaBufferCheio();
		int n2 = tamBuf2();
		int tam = 0;
		while (tam < n2)
			fprintf(dados2_f, "%4ld\n", buf[tam++]);
		fflush(dados2_f);
		aloca_tela();
		printf("Gravando no arquivo 2...\n");

		libera_tela();
	}

	fclose(dados2_f);
}

void thread_grava_nivel_temp(void)
{
	FILE *dados3_f;
	dados3_f = fopen("dados3.txt", "w");
	if (dados3_f == NULL)
	{
		printf("Erro, nao foi possivel abrir o arquivo\n");
		exit(1);
	}
	int amostras = 1;
	while (amostras++ <= N_AMOSTRAS / 200)
	{
		double *buf3 = bufduplo3_esperaBufferCheio();
		double *buf4 = bufduplo4_esperaBufferCheio();
		int n = tamBuf3();
		int n2 = tamBuf4();
		int tam = 0;
		while (tam < n2 && tam < n){
			fprintf(dados3_f, "%4lf\n", buf3[tam++]);
			fprintf(dados3_f, "%4lf\n", buf4[tam++]);}
		fflush(dados3_f);
		aloca_tela();
		printf("Gravando no arquivo 3...\n");

		libera_tela();
	}

	fclose(dados3_f);
}

int main(int argc, char *argv[])
{
	ref_putT(29.0);
	ref_putH(2.0);
	cria_socket(argv[1], atoi(argv[2]));
	pthread_t t1, t2, t3, t4, t5, t6, t7, t8;

	pthread_create(&t1, NULL, (void *)thread_mostra_status, NULL);
	pthread_create(&t2, NULL, (void *)thread_le_sensor, NULL);
	pthread_create(&t3, NULL, (void *)thread_alarme, NULL);
	pthread_create(&t4, NULL, (void *)thread_controle_temperatura, NULL);
	pthread_create(&t5, NULL, (void *)thread_grava_temp_resp, NULL);
	pthread_create(&t6, NULL, (void *)thread_controle_nivel, NULL);
	pthread_create(&t7, NULL, (void *)thread_grava_nivel_resp, NULL);
	pthread_create(&t8, NULL, (void *)thread_grava_nivel_temp, NULL);

	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	pthread_join(t3, NULL);
	pthread_join(t4, NULL);
	pthread_join(t5, NULL);
	pthread_join(t6, NULL);
	pthread_join(t7, NULL);
	pthread_join(t8, NULL);

	return 0;
}
